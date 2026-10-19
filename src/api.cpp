#include "api.hpp"

#include <httplib.h>

#include <exception>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

#include "errors.hpp"
#include "schema.hpp"

namespace {

constexpr char const JSON_TYPE[] = "application/json; charset=utf-8";

/// Наибольший размер тела запроса: реквизиты и условия договора укладываются с большим запасом.
constexpr std::size_t MAX_REQUEST_BYTES = std::size_t{1024} * 1024;

constexpr int HTTP_OK = 200;
constexpr int HTTP_BAD_REQUEST = 400;
constexpr int HTTP_NOT_FOUND = 404;
constexpr int HTTP_PAYLOAD_TOO_LARGE = 413;
constexpr int HTTP_INTERNAL_ERROR = 500;

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

} // namespace

Api::Api(Database &db, std::string web_dir) : m_web_dir{std::move(web_dir)}, m_db{db} {}

Api::Handler Api::guarded(Handler handler) {
    return [this, handler = std::move(handler)](httplib::Request const &req, httplib::Response &res) {
        auto const lock = m_db.lock();
        try {
            handler(req, res);
        } catch (UserError const &e) {
            send_error(res, HTTP_BAD_REQUEST, e.what());
        } catch (NotFoundError const &e) {
            send_error(res, HTTP_NOT_FOUND, e.what());
        }
    };
}

void Api::register_routes(httplib::Server &server) {
    server.set_payload_max_length(MAX_REQUEST_BYTES);
    server.Get("/api/version", guarded([this](httplib::Request const &, httplib::Response &res) {
                   send_json(res, {{"version", ARENDA_VERSION}, {"schema", current_schema_version(m_db)}});
               }));

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
