#pragma once
#include <httplib.h>

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "dadata.hpp"

namespace fake {

/// Ответ DaData об организации (сокращённый, поля как в документации findById/party).
inline nlohmann::json legal_data() {
    return nlohmann::json::parse(R"({
        "type": "LEGAL", "inn": "7707083893", "kpp": "773601001", "ogrn": "1027700132195",
        "opf": {"full": "Общество с ограниченной ответственностью", "short": "ООО"},
        "name": {"full": "ТОРГОВЫЙ ДОМ \"ВОСХОД\"", "short": "ТД \"ВОСХОД\"",
                 "full_with_opf": "ОБЩЕСТВО С ОГРАНИЧЕННОЙ ОТВЕТСТВЕННОСТЬЮ \"ТОРГОВЫЙ ДОМ \"ВОСХОД\"\"",
                 "short_with_opf": "ООО \"ТД \"ВОСХОД\"\""},
        "management": {"name": "ПЕТРОВ ПЁТР ПАВЛОВИЧ", "post": "ГЕНЕРАЛЬНЫЙ ДИРЕКТОР"},
        "address": {"value": "г Москва, ул Ленина, д 1", "unrestricted_value": "117312, г Москва, ул Ленина, д 1"}
    })");
}

inline nlohmann::json individual_data() {
    return nlohmann::json::parse(R"({
        "type": "INDIVIDUAL", "inn": "500100732259", "ogrn": "304500116000157", "kpp": null,
        "fio": {"surname": "Сидорова", "name": "Анна", "patronymic": "Сергеевна"},
        "name": {"full": "Сидорова Анна Сергеевна"},
        "management": null,
        "address": {"value": "Московская обл, г Химки", "unrestricted_value": "Московская обл, г Химки"}
    })");
}

/**
 * @brief Подставной сервер DaData на локальном порте: отвечает по ключу и запросу, запоминает последний запрос.
 */
class FakeDaData {
   public:
    FakeDaData() {
        m_server.Post("/api/findById/:method", [this](httplib::Request const &req, httplib::Response &res) {
            m_authorization = req.get_header_value("Authorization");
            m_body = nlohmann::json::parse(req.body);
            if (m_authorization != "Token good") {
                res.status = 403;
                return;
            }
            auto const query = m_body.at("query").get<std::string>();
            nlohmann::json suggestions = nlohmann::json::array();
            if (req.path_params.at("method") == "party" && query == "7707083893") {
                suggestions.push_back({{"value", "ТД"}, {"data", legal_data()}});
            } else if (req.path_params.at("method") == "party" && query == "500100732259") {
                suggestions.push_back({{"value", "ИП"}, {"data", individual_data()}});
            } else if (req.path_params.at("method") == "bank" && query == "044525225") {
                suggestions.push_back({{"value", "ПАО Сбербанк"},
                                       {"data", {{"name", {{"payment", "ПАО \"Сбербанк\""}}}, {"correspondent_account", "30101810400000000225"}}}});
            }
            res.set_content(nlohmann::json{{"suggestions", suggestions}}.dump(), "application/json");
        });
        m_port = m_server.bind_to_any_port("127.0.0.1");
        m_thread = std::thread{[this] { m_server.listen_after_bind(); }};
        while (!m_server.is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    }

    ~FakeDaData() {
        m_server.stop();
        m_thread.join();
    }

    FakeDaData(FakeDaData const &) = delete;
    FakeDaData &operator=(FakeDaData const &) = delete;
    FakeDaData(FakeDaData &&) = delete;
    FakeDaData &operator=(FakeDaData &&) = delete;

    [[nodiscard]] DaData client(std::string const &key) const { return DaData{"http://127.0.0.1:" + std::to_string(m_port) + "/api", key}; }

    [[nodiscard]] std::string const &authorization() const { return m_authorization; }
    [[nodiscard]] nlohmann::json const &body() const { return m_body; }

   private:
    httplib::Server m_server;
    std::thread m_thread;
    std::string m_authorization;
    nlohmann::json m_body;
    int m_port = 0;
};

} // namespace fake
