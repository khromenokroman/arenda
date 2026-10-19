#include "api.hpp"

#include <httplib.h>

#include <charconv>
#include <exception>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

#include "errors.hpp"
#include "json_io.hpp"
#include "schema.hpp"

namespace {

constexpr char const JSON_TYPE[] = "application/json; charset=utf-8";

/// Наибольший размер тела запроса: реквизиты и условия договора укладываются с большим запасом.
constexpr std::size_t MAX_REQUEST_BYTES = std::size_t{1024} * 1024;

constexpr int HTTP_OK = 200;
constexpr int HTTP_CREATED = 201;
constexpr int HTTP_NO_CONTENT = 204;
constexpr int HTTP_BAD_REQUEST = 400;
constexpr int HTTP_NOT_FOUND = 404;
constexpr int HTTP_PAYLOAD_TOO_LARGE = 413;
constexpr int HTTP_INTERNAL_ERROR = 500;
constexpr int HTTP_BAD_GATEWAY = 502;

void send_json(httplib::Response &res, nlohmann::json const &body, int status = HTTP_OK) {
    res.status = status;
    res.set_content(body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace), JSON_TYPE);
}

void send_error(httplib::Response &res, int status, std::string const &message) { send_json(res, {{"error", message}}, status); }

/// Текст ошибки для ответа API, который httplib сформировал сам (нет обработчика, слишком большой запрос).
std::string http_error_text(httplib::Request const &req, int status) {
    if (status == HTTP_NOT_FOUND) {
        return "нет такого метода: " + req.method + " " + req.path;
    }
    if (status == HTTP_PAYLOAD_TOO_LARGE) {
        return "запрос больше " + std::to_string(MAX_REQUEST_BYTES / 1024) + " КБ";
    }
    return "ошибка запроса";
}

/// id из пути запроса; не число означает несуществующий объект.
std::int64_t path_id(httplib::Request const &req) {
    auto const &text = req.path_params.at("id");
    std::int64_t id = 0;
    auto const [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), id);
    if (ec != std::errc{} || ptr != text.data() + text.size()) {
        throw NotFoundError("неверный id: " + text);
    }
    return id;
}

/// Арендатор с названием для списков и названием вида.
nlohmann::json tenant_json(Tenant const &tenant) {
    auto out = to_json(tenant);
    out["title"] = Tenants::title(tenant);
    out["kind_label"] = Tenants::kind(tenant.fields.at("kind")).label;
    return out;
}

} // namespace

Api::Api(Database &db, std::string web_dir, DaData dadata)
    : m_web_dir{std::move(web_dir)}, m_dadata{std::move(dadata)}, m_landlords{db}, m_tenants{db}, m_equipment{db}, m_contracts{db}, m_db{db} {}

Api::Handler Api::guarded(Handler handler, bool lock_db) {
    return [this, lock_db, handler = std::move(handler)](httplib::Request const &req, httplib::Response &res) {
        std::unique_lock<std::mutex> lock;
        if (lock_db) {
            lock = m_db.lock();
        }
        try {
            handler(req, res);
        } catch (UserError const &e) {
            send_error(res, HTTP_BAD_REQUEST, e.what());
        } catch (NotFoundError const &e) {
            send_error(res, HTTP_NOT_FOUND, e.what());
        } catch (ExternalError const &e) {
            send_error(res, HTTP_BAD_GATEWAY, e.what());
        }
    };
}

void Api::register_routes(httplib::Server &server) {
    server.set_payload_max_length(MAX_REQUEST_BYTES);
    server.Get("/api/version", guarded([this](httplib::Request const &, httplib::Response &res) {
                   send_json(res, {{"version", ARENDA_VERSION}, {"schema", current_schema_version(m_db)}});
               }));
    register_landlords(server);
    register_tenants(server);
    register_lookup(server);
    register_contracts(server);

    if (!m_web_dir.empty() && !server.set_mount_point("/", m_web_dir)) {
        throw std::runtime_error("каталог веб-интерфейса не найден: " + m_web_dir);
    }

    server.set_exception_handler([](httplib::Request const &, httplib::Response &res, std::exception_ptr const &ep) {
        try {
            std::rethrow_exception(ep);
        } catch (std::exception const &e) {
            send_error(res, HTTP_INTERNAL_ERROR, e.what());
        } catch (...) {
            send_error(res, HTTP_INTERNAL_ERROR, "неизвестная ошибка");
        }
    });

    server.set_error_handler([](httplib::Request const &req, httplib::Response &res) {
        if (req.path.starts_with("/api/") && res.body.empty()) {
            send_error(res, res.status, http_error_text(req, res.status));
        }
    });
}

void Api::register_landlords(httplib::Server &server) {
    server.Get("/api/landlords/fields",
               guarded([](httplib::Request const &, httplib::Response &res) { send_json(res, to_json(Landlords::specs())); }));

    server.Get("/api/landlords", guarded([this](httplib::Request const &, httplib::Response &res) {
                   auto out = nlohmann::json::array();
                   for (auto const &landlord : m_landlords.list()) {
                       out.push_back(to_json(landlord));
                   }
                   send_json(res, out);
               }));

    server.Post("/api/landlords", guarded([this](httplib::Request const &req, httplib::Response &res) {
                    auto const id = m_landlords.create(fields_from_json(parse_object(req.body)));
                    send_json(res, to_json(m_landlords.get(id)), HTTP_CREATED);
                }));

    server.Get("/api/landlords/:id",
               guarded([this](httplib::Request const &req, httplib::Response &res) { send_json(res, to_json(m_landlords.get(path_id(req)))); }));

    server.Put("/api/landlords/:id", guarded([this](httplib::Request const &req, httplib::Response &res) {
                   auto const id = path_id(req);
                   m_landlords.update(id, fields_from_json(parse_object(req.body)));
                   send_json(res, to_json(m_landlords.get(id)));
               }));

    server.Delete("/api/landlords/:id", guarded([this](httplib::Request const &req, httplib::Response &res) {
                      m_landlords.remove(path_id(req));
                      res.status = HTTP_NO_CONTENT;
                  }));
}

void Api::register_tenants(httplib::Server &server) {
    server.Get("/api/tenants/kinds", guarded([](httplib::Request const &, httplib::Response &res) {
                   auto out = nlohmann::json::array();
                   for (auto const &kind : Tenants::kinds()) {
                       out.push_back({{"code", kind.code}, {"label", kind.label}, {"fields", to_json(kind.specs)}});
                   }
                   send_json(res, out);
               }));

    server.Get("/api/tenants", guarded([this](httplib::Request const &, httplib::Response &res) {
                   auto out = nlohmann::json::array();
                   for (auto const &tenant : m_tenants.list()) {
                       out.push_back(tenant_json(tenant));
                   }
                   send_json(res, out);
               }));

    server.Post("/api/tenants", guarded([this](httplib::Request const &req, httplib::Response &res) {
                    auto const id = m_tenants.create(fields_from_json(parse_object(req.body)));
                    send_json(res, tenant_json(m_tenants.get(id)), HTTP_CREATED);
                }));

    server.Get("/api/tenants/:id",
               guarded([this](httplib::Request const &req, httplib::Response &res) { send_json(res, tenant_json(m_tenants.get(path_id(req)))); }));

    server.Put("/api/tenants/:id", guarded([this](httplib::Request const &req, httplib::Response &res) {
                   auto const id = path_id(req);
                   m_tenants.update(id, fields_from_json(parse_object(req.body)));
                   send_json(res, tenant_json(m_tenants.get(id)));
               }));

    server.Get("/api/tenants/:id/equipment", guarded([this](httplib::Request const &req, httplib::Response &res) {
                   auto out = nlohmann::json::array();
                   for (auto const &row : m_equipment.list(path_id(req))) {
                       out.push_back(to_json(row));
                   }
                   send_json(res, out);
               }));

    server.Put("/api/tenants/:id/equipment", guarded([this](httplib::Request const &req, httplib::Response &res) {
                   auto const id = path_id(req);
                   auto const body = nlohmann::json::parse(req.body, nullptr, false);
                   if (body.is_discarded()) {
                       throw UserError("тело запроса не JSON");
                   }
                   m_equipment.replace(id, equipment_from_json(body));
                   auto out = nlohmann::json::array();
                   for (auto const &row : m_equipment.list(id)) {
                       out.push_back(to_json(row));
                   }
                   send_json(res, out);
               }));

    server.Delete("/api/tenants/:id", guarded([this](httplib::Request const &req, httplib::Response &res) {
                      m_tenants.remove(path_id(req));
                      res.status = HTTP_NO_CONTENT;
                  }));
}

void Api::register_lookup(httplib::Server &server) {
    // внешний сервис отвечает до нескольких секунд: база на это время не захватывается
    server.Get("/api/lookup/party", guarded(
                                        [this](httplib::Request const &req, httplib::Response &res) {
                                            auto const target = req.get_param_value("for");
                                            if (target != "landlord" && target != "org" && target != "ip") {
                                                throw UserError("параметр for: нужен landlord, org или ip");
                                            }
                                            auto const inn = trim(req.get_param_value("inn"));
                                            if (inn.empty()) {
                                                throw UserError("не указан ИНН");
                                            }
                                            auto const data = m_dadata.find_party(inn);
                                            send_json(res, {{"fields", party_fields(data, target)}});
                                        },
                                        false));

    server.Get("/api/lookup/bank", guarded(
                                       [this](httplib::Request const &req, httplib::Response &res) {
                                           auto const data = m_dadata.find_bank(trim(req.get_param_value("bik")));
                                           send_json(res, {{"fields", bank_fields(data)}});
                                       },
                                       false));
}

void Api::register_contracts(httplib::Server &server) {
    auto const contracts_json = [](std::vector<Contract> const &list) {
        auto out = nlohmann::json::array();
        for (auto const &c : list) {
            out.push_back(to_json(c));
        }
        return out;
    };

    server.Post("/api/contracts/plan", guarded([this, contracts_json](httplib::Request const &req, httplib::Response &res) {
                    auto const [landlord_id, rows] = contract_requests_from_json(parse_object(req.body));
                    send_json(res, contracts_json(m_contracts.plan(landlord_id, rows)));
                }));

    server.Post("/api/contracts", guarded([this, contracts_json](httplib::Request const &req, httplib::Response &res) {
                    auto const [landlord_id, rows] = contract_requests_from_json(parse_object(req.body));
                    std::vector<Contract> created;
                    for (auto const id : m_contracts.create(landlord_id, rows)) {
                        created.push_back(m_contracts.get(id));
                    }
                    send_json(res, contracts_json(created), HTTP_CREATED);
                }));

    server.Get("/api/contracts", guarded([this, contracts_json](httplib::Request const &, httplib::Response &res) {
                   send_json(res, contracts_json(m_contracts.list()));
               }));

    server.Get("/api/contracts/:id",
               guarded([this](httplib::Request const &req, httplib::Response &res) { send_json(res, to_json(m_contracts.get(path_id(req)))); }));

    server.Post("/api/contracts/:id/refresh", guarded([this](httplib::Request const &req, httplib::Response &res) {
                    auto const id = path_id(req);
                    m_contracts.refresh(id);
                    send_json(res, to_json(m_contracts.get(id)));
                }));

    server.Delete("/api/contracts/:id", guarded([this](httplib::Request const &req, httplib::Response &res) {
                      m_contracts.remove(path_id(req));
                      res.status = HTTP_NO_CONTENT;
                  }));
}
