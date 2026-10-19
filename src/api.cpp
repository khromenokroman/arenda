#include "api.hpp"

#include <httplib.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <exception>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

#include "contract_print.hpp"
#include "errors.hpp"
#include "json_io.hpp"
#include "schema.hpp"

namespace {

constexpr char const JSON_TYPE[] = "application/json; charset=utf-8";
constexpr char const HTML_TYPE[] = "text/html; charset=utf-8";

/// Наибольший размер тела запроса: реквизиты и условия договора укладываются с большим запасом.
constexpr std::size_t MAX_REQUEST_BYTES = std::size_t{1024} * 1024;

constexpr int HTTP_OK = 200;
constexpr int HTTP_CREATED = 201;
constexpr int HTTP_NO_CONTENT = 204;
constexpr int HTTP_BAD_REQUEST = 400;
constexpr int HTTP_UNAUTHORIZED = 401;
constexpr int HTTP_FORBIDDEN = 403;
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

/// Список id через запятую: «1,2,3».
std::vector<std::int64_t> parse_ids(std::string const &text) {
    std::vector<std::int64_t> ids;
    std::size_t pos = 0;
    while (pos < text.size()) {
        auto const end = std::min(text.find(',', pos), text.size());
        std::int64_t id = 0;
        auto const [ptr, ec] = std::from_chars(text.data() + pos, text.data() + end, id);
        if (ec != std::errc{} || ptr != text.data() + end) {
            throw UserError("неверный список договоров: " + text);
        }
        ids.push_back(id);
        pos = end + 1;
    }
    return ids;
}

/// id из параметра запроса. @throws UserError не число
std::int64_t query_id(httplib::Request const &req, std::string const &key) {
    auto const text = req.get_param_value(key);
    std::int64_t id = 0;
    auto const [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), id);
    if (ec != std::errc{} || ptr != text.data() + text.size()) {
        throw UserError("параметр " + key + " должен быть числом: " + text);
    }
    return id;
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

constexpr char const SESSION_COOKIE[] = "arenda_session";

/// Токен сессии из cookie запроса; пустой, если его нет.
std::string session_token(httplib::Request const &req) {
    auto const cookies = req.get_header_value("Cookie");
    std::string const key = std::string(SESSION_COOKIE) + "=";
    for (std::size_t pos = 0; pos < cookies.size();) {
        auto const end = std::min(cookies.find(';', pos), cookies.size());
        auto const first = cookies.find_first_not_of(' ', pos);
        if (first < end && cookies.compare(first, key.size(), key) == 0) {
            return cookies.substr(first + key.size(), end - first - key.size());
        }
        pos = end + 1;
    }
    return {};
}

/// Cookie сессии: только для сервера (HttpOnly), не уходит с чужих сайтов (SameSite=Strict).
void set_session_cookie(httplib::Response &res, std::string const &token) {
    res.set_header("Set-Cookie", std::string(SESSION_COOKIE) + "=" + token + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=" +
                                     (token.empty() ? "0" : std::to_string(SESSION_DAYS * 24 * 3600)));
}

nlohmann::json user_json(User const &user) {
    return {{"id", user.id}, {"login", user.login}, {"name", user.name}, {"role", to_string(user.role)}, {"blocked", user.blocked}};
}

/// Строковое поле запроса. @throws UserError
std::string string_field(nlohmann::json const &j, char const *key) {
    if (!j.contains(key) || !j[key].is_string()) {
        throw UserError(std::string("поле ") + key + " должно быть строкой");
    }
    return j[key].get<std::string>();
}

/// Необязательное строковое поле запроса: нет - fallback. @throws UserError если оно не строка
std::string optional_string(nlohmann::json const &j, char const *key, std::string fallback) {
    return j.contains(key) ? string_field(j, key) : std::move(fallback);
}

} // namespace

Api::Api(Database &db, std::string web_dir, DaData dadata, std::string backup_dir)
    : m_web_dir{std::move(web_dir)},
      m_dadata{std::move(dadata)},
      m_backups{db, std::move(backup_dir)},
      m_landlords{db},
      m_tenants{db},
      m_equipment{db},
      m_contracts{db},
      m_db{db} {}

Api::Handler Api::guarded(Handler handler, bool lock_db) { return guarded(Access::USER, std::move(handler), lock_db); }

Api::Handler Api::guarded(Access access, Handler handler, bool lock_db) {
    return [this, access, lock_db, handler = std::move(handler)](httplib::Request const &req, httplib::Response &res) {
        auto lock = m_db.lock();
        try {
            m_user = Users{m_db}.by_session(session_token(req));
            if (access != Access::PUBLIC && !m_user) {
                throw UnauthorizedError("нужно войти");
            }
            if (access == Access::ADMIN && m_user->role != Role::ADMIN) {
                throw ForbiddenError("недостаточно прав: это делает администратор");
            }
            if (!lock_db) {
                m_user.reset();
                lock.unlock();
            }
            handler(req, res);
        } catch (UserError const &e) {
            send_error(res, HTTP_BAD_REQUEST, e.what());
        } catch (UnauthorizedError const &e) {
            send_error(res, HTTP_UNAUTHORIZED, e.what());
        } catch (ForbiddenError const &e) {
            send_error(res, HTTP_FORBIDDEN, e.what());
        } catch (NotFoundError const &e) {
            send_error(res, HTTP_NOT_FOUND, e.what());
        } catch (ExternalError const &e) {
            send_error(res, HTTP_BAD_GATEWAY, e.what());
        }
        if (lock.owns_lock()) {
            m_user.reset();
        }
    };
}

void Api::register_routes(httplib::Server &server) {
    server.set_payload_max_length(MAX_REQUEST_BYTES);
    server.Get("/api/version", guarded(Access::PUBLIC, [this](httplib::Request const &, httplib::Response &res) {
                   send_json(res, {{"version", ARENDA_VERSION}, {"schema", current_schema_version(m_db)}});
               }));
    register_auth(server);
    register_backups(server);
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

    server.Get("/api/contracts", guarded([this, contracts_json](httplib::Request const &req, httplib::Response &res) {
                   ContractFilter filter;
                   filter.from = req.get_param_value("from");
                   filter.to = req.get_param_value("to");
                   if (req.has_param("landlord_id") && !req.get_param_value("landlord_id").empty()) {
                       filter.landlord_id = query_id(req, "landlord_id");
                   }
                   if (req.has_param("tenant_id") && !req.get_param_value("tenant_id").empty()) {
                       filter.tenant_id = query_id(req, "tenant_id");
                   }
                   send_json(res, contracts_json(m_contracts.list(filter)));
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

    // печатная форма: HTML-страница A4, один договор или пачка ?ids=1,2,3 в заданном порядке
    server.Get("/print/contracts/:id", guarded([this](httplib::Request const &req, httplib::Response &res) {
                   res.set_content(contract_print::render({m_contracts.get(path_id(req))}), HTML_TYPE);
               }));
    server.Get("/print/contracts", guarded([this](httplib::Request const &req, httplib::Response &res) {
                   std::vector<Contract> list;
                   for (auto const id : parse_ids(req.get_param_value("ids"))) {
                       list.push_back(m_contracts.get(id));
                   }
                   res.set_content(contract_print::render(list), HTML_TYPE);
               }));
}

void Api::register_auth(httplib::Server &server) {
    server.Get("/api/auth", guarded(Access::PUBLIC, [this](httplib::Request const &, httplib::Response &res) {
                   send_json(res, {{"setup", Users{m_db}.empty()}, {"user", m_user ? user_json(*m_user) : nlohmann::json(nullptr)}});
               }));

    server.Post("/api/auth/setup", guarded(Access::PUBLIC, [this](httplib::Request const &req, httplib::Response &res) {
                    auto const j = parse_object(req.body);
                    Users users{m_db};
                    User user;
                    user.login = string_field(j, "login");
                    user.name = optional_string(j, "name", "");
                    auto const id = users.setup(user, string_field(j, "password"));
                    set_session_cookie(res, users.open_session(id));
                    send_json(res, {{"user", user_json(users.get(id))}}, HTTP_CREATED);
                }));

    server.Post("/api/auth/login", guarded(Access::PUBLIC, [this](httplib::Request const &req, httplib::Response &res) {
                    auto const j = parse_object(req.body);
                    Users users{m_db};
                    auto const token = users.login(string_field(j, "login"), string_field(j, "password"));
                    set_session_cookie(res, token);
                    send_json(res, {{"user", user_json(*users.by_session(token))}});
                }));

    server.Post("/api/auth/logout", guarded(Access::PUBLIC, [this](httplib::Request const &req, httplib::Response &res) {
                    Users{m_db}.logout(session_token(req));
                    set_session_cookie(res, "");
                    send_json(res, nlohmann::json::object());
                }));

    server.Put("/api/auth/password", guarded([this](httplib::Request const &req, httplib::Response &res) {
                   auto const j = parse_object(req.body);
                   Users users{m_db};
                   if (!users.check_password(m_user->id, string_field(j, "old_password"))) {
                       throw UserError("текущий пароль неверный");
                   }
                   users.set_password(m_user->id, string_field(j, "password"));
                   // смена пароля закрывает все сессии, в том числе эту: вместо неё открывается новая
                   set_session_cookie(res, users.open_session(m_user->id));
                   send_json(res, nlohmann::json::object());
               }));

    server.Get("/api/users", guarded(Access::ADMIN, [this](httplib::Request const &, httplib::Response &res) {
                   auto out = nlohmann::json::array();
                   for (auto const &user : Users{m_db}.list()) {
                       out.push_back(user_json(user));
                   }
                   send_json(res, out);
               }));

    server.Post("/api/users", guarded(Access::ADMIN, [this](httplib::Request const &req, httplib::Response &res) {
                    auto const j = parse_object(req.body);
                    Users users{m_db};
                    User user;
                    user.login = string_field(j, "login");
                    user.name = optional_string(j, "name", "");
                    user.role = role_from_string(optional_string(j, "role", "user"));
                    auto const id = users.create(user, string_field(j, "password"));
                    send_json(res, user_json(users.get(id)), HTTP_CREATED);
                }));

    server.Put("/api/users/:id", guarded(Access::ADMIN, [this](httplib::Request const &req, httplib::Response &res) {
                   auto const j = parse_object(req.body);
                   Users users{m_db};
                   auto user = users.get(path_id(req));
                   if (j.contains("name")) {
                       user.name = string_field(j, "name");
                   }
                   if (j.contains("role")) {
                       user.role = role_from_string(string_field(j, "role"));
                   }
                   if (j.contains("blocked")) {
                       if (!j["blocked"].is_boolean()) {
                           throw UserError("поле blocked должно быть true или false");
                       }
                       user.blocked = j["blocked"].get<bool>();
                   }
                   users.update(user);
                   if (j.contains("password")) {
                       users.set_password(user.id, string_field(j, "password"));
                   }
                   send_json(res, user_json(users.get(user.id)));
               }));
}

void Api::register_backups(httplib::Server &server) {
    auto const state = [this] {
        auto const schedule = m_backups.schedule();
        auto list = nlohmann::json::array();
        for (auto const &b : m_backups.list()) {
            list.push_back({{"name", b.name}, {"created", b.created}, {"size", b.size}, {"scheduled", b.scheduled}});
        }
        return nlohmann::json{{"dir", m_backups.dir().string()},
                              {"schedule",
                               {{"enabled", schedule.enabled},
                                {"time", schedule.time},
                                {"keep", schedule.keep},
                                {"last_run", schedule.last_run},
                                {"last_error", schedule.last_error}}},
                              {"backups", list}};
    };

    server.Get("/api/backups", guarded(Access::ADMIN, [state](httplib::Request const &, httplib::Response &res) { send_json(res, state()); }));

    server.Post("/api/backups", guarded(Access::ADMIN, [this, state](httplib::Request const &, httplib::Response &res) {
                    auto const backup = m_backups.create(false, local_now());
                    auto out = state();
                    out["created"] = backup.name;
                    send_json(res, out, HTTP_CREATED);
                }));

    server.Put("/api/backups/schedule", guarded(Access::ADMIN, [this, state](httplib::Request const &req, httplib::Response &res) {
                   auto const j = parse_object(req.body);
                   auto schedule = m_backups.schedule();
                   if (j.contains("enabled")) {
                       if (!j["enabled"].is_boolean()) {
                           throw UserError("поле enabled должно быть true или false");
                       }
                       schedule.enabled = j["enabled"].get<bool>();
                   }
                   if (j.contains("time")) {
                       schedule.time = string_field(j, "time");
                   }
                   if (j.contains("keep")) {
                       if (!j["keep"].is_number_integer()) {
                           throw UserError("поле keep должно быть целым числом");
                       }
                       schedule.keep = j["keep"].get<int>();
                   }
                   m_backups.set_schedule(schedule);
                   send_json(res, state());
               }));

    server.Delete("/api/backups/:name", guarded(Access::ADMIN, [this, state](httplib::Request const &req, httplib::Response &res) {
                      m_backups.remove(req.path_params.at("name"));
                      send_json(res, state());
                  }));

    server.Get("/api/backups/:name", guarded(Access::ADMIN, [this](httplib::Request const &req, httplib::Response &res) {
                   auto const name = req.path_params.at("name");
                   // файл открывается сейчас: отдаётся целиком, даже если его удалят во время скачивания
                   auto file = std::make_shared<std::ifstream>(m_backups.path(name), std::ios::binary);
                   if (!*file) {
                       throw NotFoundError("не удалось открыть копию " + name);
                   }
                   res.set_header("Content-Disposition", "attachment; filename=\"" + name + "\"");
                   res.set_content_provider("application/vnd.sqlite3", [file](std::size_t, httplib::DataSink &sink) {
                       std::array<char, std::size_t{64} * 1024> buf{};
                       file->read(buf.data(), buf.size());
                       if (file->gcount() > 0) {
                           sink.write(buf.data(), static_cast<std::size_t>(file->gcount()));
                       }
                       if (!*file) {
                           sink.done();
                       }
                       return true;
                   });
               }));
}
