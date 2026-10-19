#pragma once
#include <functional>
#include <string>

#include "contracts.hpp"
#include "dadata.hpp"
#include "db.hpp"
#include "equipment.hpp"
#include "landlords.hpp"
#include "tenants.hpp"

namespace httplib {
class Server;
struct Request;
struct Response;
} // namespace httplib

/**
 * @brief HTTP API и раздача веб-интерфейса.
 *
 * Все ответы /api/ в JSON; ошибки возвращаются как {"error": "текст"}.
 */
class Api {
   public:
    /**
     * @param db база, к которой обращаются обработчики
     * @param web_dir каталог статических страниц; пустой, если раздавать не нужно
     * @param dadata клиент DaData для заполнения реквизитов; без ключа заполнение отвечает подсказкой
     */
    Api(Database &db, std::string web_dir, DaData dadata = {});

    /// Регистрирует обработчики на сервере.
    void register_routes(httplib::Server &server);

   private:
    using Handler = std::function<void(httplib::Request const &, httplib::Response &)>;

    /**
     * @brief Оборачивает обработчик: захват базы и перевод исключений в ответы 400/404/502.
     * @param lock_db false - без захвата базы (обработчик не обращается к ней, а ждёт внешний сервис)
     */
    [[nodiscard]] Handler guarded(Handler handler, bool lock_db = true);

    void register_landlords(httplib::Server &server);
    void register_tenants(httplib::Server &server);
    void register_lookup(httplib::Server &server);
    void register_contracts(httplib::Server &server);

    std::string m_web_dir;
    DaData m_dadata;
    Landlords m_landlords;
    Tenants m_tenants;
    Equipment m_equipment;
    Contracts m_contracts;
    Database &m_db;
};
