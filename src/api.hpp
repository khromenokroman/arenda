#pragma once
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

#include "activities.hpp"
#include "backups.hpp"
#include "contracts.hpp"
#include "dadata.hpp"
#include "db.hpp"
#include "equipment.hpp"
#include "landlords.hpp"
#include "tenants.hpp"
#include "users.hpp"

namespace httplib {
class Server;
struct Request;
struct Response;
} // namespace httplib

/**
 * @brief HTTP API и раздача веб-интерфейса.
 *
 * Все ответы /api/ в JSON; ошибки возвращаются как {"error": "текст"}. Без входа доступны только версия,
 * вход и создание первого администратора; пользователями управляет администратор.
 */
class Api {
   public:
    /**
     * @param db база после migrate(), к которой обращаются обработчики; из неё читается ключ DaData из «Настроек»
     * @param web_dir каталог статических страниц; пустой, если раздавать не нужно
     * @param dadata клиент DaData для заполнения реквизитов; без ключа заполнение отвечает подсказкой
     * @param backup_dir каталог резервных копий
     */
    Api(Database &db, std::string web_dir, DaData dadata = {}, std::string backup_dir = "backups");

    /// Регистрирует обработчики на сервере.
    void register_routes(httplib::Server &server);

   private:
    using Handler = std::function<void(httplib::Request const &, httplib::Response &)>;

    /**
     * @brief Кому доступен метод.
     */
    enum class Access : std::uint8_t {
        PUBLIC, ///< без входа
        USER,   ///< любой вошедший пользователь
        ADMIN,  ///< только администратор
    };

    /**
     * @brief Оборачивает обработчик: захват базы, проверка сессии и роли, перевод исключений в ответы
     * 400/401/403/404/502. На время обработчика m_user - вошедший пользователь.
     * @param lock_db false - база захватывается только на проверку сессии (обработчик ждёт внешний сервис
     * и к базе не обращается, m_user недоступен)
     */
    [[nodiscard]] Handler guarded(Access access, Handler handler, bool lock_db = true);

    /// То же для любого вошедшего пользователя.
    [[nodiscard]] Handler guarded(Handler handler, bool lock_db = true);

    void register_auth(httplib::Server &server);
    void register_backups(httplib::Server &server);

    void register_landlords(httplib::Server &server);
    void register_tenants(httplib::Server &server);
    void register_activities(httplib::Server &server);
    void register_lookup(httplib::Server &server);
    void register_contracts(httplib::Server &server);
    void register_settings(httplib::Server &server);

    /// Клиент DaData с ключом из «Настроек», а если он не задан - из переменной окружения.
    [[nodiscard]] DaData dadata();

    std::optional<User> m_user; ///< вошедший пользователь текущего запроса; меняется только под блокировкой базы
    std::string m_web_dir;
    DaData m_dadata;       ///< адрес и ключ из переменной окружения
    std::string m_web_key; ///< ключ DaData из «Настроек»; читается и меняется под m_key_mutex
    std::mutex m_key_mutex;
    Backups m_backups;
    Landlords m_landlords;
    Tenants m_tenants;
    Activities m_activities;
    Equipment m_equipment;
    Contracts m_contracts;
    Database &m_db;
};
