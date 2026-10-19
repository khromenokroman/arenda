#include "api.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <httplib.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>

#include "db.hpp"
#include "fake_dadata.hpp"
#include "schema.hpp"

using testing::HasSubstr;

namespace {

/**
 * @brief Сервер API на случайном локальном порте, работающий в отдельном потоке.
 */
class TestServer {
   public:
    explicit TestServer(std::string const &web_dir = {}, DaData dadata = {}) : m_db{":memory:"} {
        migrate(m_db);
        m_backup_dir = std::filesystem::temp_directory_path() /
                       ("arenda-api-backups-" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + "-" + std::to_string(::getpid()));
        std::filesystem::remove_all(m_backup_dir);
        m_api = std::make_unique<Api>(m_db, web_dir, std::move(dadata), m_backup_dir.string());
        m_api->register_routes(m_server);
        m_port = m_server.bind_to_any_port("127.0.0.1");
        m_thread = std::thread{[this] { m_server.listen_after_bind(); }};
        while (!m_server.is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        // первый пользователь - администратор; его сессия уходит со всеми запросами client()
        auto const res = anonymous().Post("/api/auth/setup", R"({"login": "admin", "name": "Админ", "password": "secret1"})", "application/json");
        if (res && res->status == 201) {
            m_cookie = cookie_of(*res);
        }
    }

    /// Пара "arenda_session=..." из Set-Cookie ответа.
    static std::string cookie_of(httplib::Response const &res) {
        auto const header = res.get_header_value("Set-Cookie");
        return header.substr(0, header.find(';'));
    }

    ~TestServer() {
        m_server.stop();
        m_thread.join();
        std::filesystem::remove_all(m_backup_dir);
    }

    TestServer(TestServer const &) = delete;
    TestServer &operator=(TestServer const &) = delete;
    TestServer(TestServer &&) = delete;
    TestServer &operator=(TestServer &&) = delete;

    /// Клиент с сессией администратора.
    [[nodiscard]] httplib::Client client() const { return client_as(m_cookie); }

    /// Клиент с заданной cookie сессии.
    [[nodiscard]] httplib::Client client_as(std::string const &cookie) const {
        httplib::Client cli{"127.0.0.1", m_port};
        cli.set_default_headers({{"Cookie", cookie}});
        return cli;
    }

    /// Клиент без входа.
    [[nodiscard]] httplib::Client anonymous() const { return httplib::Client{"127.0.0.1", m_port}; }

   private:
    httplib::Server m_server;
    Database m_db;
    std::unique_ptr<Api> m_api;
    std::thread m_thread;
    std::filesystem::path m_backup_dir;
    std::string m_cookie;
    int m_port = 0;
};

} // namespace

/**
 * @brief /api/version отдаёт версию программы и версию схемы базы.
 */
TEST(Api, Version) {
    TestServer srv;
    auto const res = srv.client().Get("/api/version");
    ASSERT_TRUE(res) << "нет ответа: " << httplib::to_string(res.error());
    EXPECT_EQ(res->status, 200) << res->body;
    EXPECT_THAT(res->get_header_value("Content-Type"), HasSubstr("application/json"));
    auto const body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body.at("version"), ARENDA_VERSION) << res->body;
    EXPECT_EQ(body.at("schema"), schema_version()) << res->body;
}

/**
 * @brief Неизвестный метод API отвечает 404 с текстом ошибки в JSON, а не пустым телом.
 */
TEST(Api, UnknownMethodIsJsonError) {
    TestServer srv;
    auto const res = srv.client().Get("/api/nothing");
    ASSERT_TRUE(res) << "нет ответа: " << httplib::to_string(res.error());
    EXPECT_EQ(res->status, 404);
    auto const body = nlohmann::json::parse(res->body);
    EXPECT_THAT(body.at("error").get<std::string>(), HasSubstr("/api/nothing")) << res->body;
}

/**
 * @brief Страницы веб-интерфейса раздаются из каталога web_dir.
 */
TEST(Api, ServesWebDir) {
    auto const dir = std::filesystem::temp_directory_path() / ("arenda-web-" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    std::ofstream{dir / "index.html"} << "<p>договоры</p>";
    {
        TestServer srv{dir.string()};
        auto const res = srv.client().Get("/");
        ASSERT_TRUE(res) << "нет ответа: " << httplib::to_string(res.error());
        EXPECT_EQ(res->status, 200);
        EXPECT_THAT(res->body, testing::StrEq("<p>договоры</p>")) << "ожидалось содержимое " << (dir / "index.html");
    }
    std::filesystem::remove_all(dir);
}

/**
 * @brief Несуществующий каталог веб-интерфейса обнаруживается при запуске, а не при первом запросе.
 */
TEST(Api, MissingWebDirFails) {
    Database db{":memory:"};
    Api api{db, "/nonexistent/arenda-web"};
    httplib::Server server;
    try {
        api.register_routes(server);
        FAIL() << "ожидался отказ для несуществующего каталога";
    } catch (std::runtime_error const &e) {
        EXPECT_THAT(std::string(e.what()), HasSubstr("/nonexistent/arenda-web"));
    }
}

/**
 * @brief Арендодатели через HTTP: описание полей, создание, список, изменение, удаление.
 */
TEST(Api, LandlordsCrud) {
    TestServer srv;
    auto cli = srv.client();
    constexpr char const JSON[] = "application/json";

    auto res = cli.Get("/api/landlords/fields");
    ASSERT_TRUE(res) << "нет ответа: " << httplib::to_string(res.error());
    auto const specs = nlohmann::json::parse(res->body);
    ASSERT_TRUE(specs.is_array() && !specs.empty()) << res->body;
    EXPECT_EQ(specs[0].at("key"), "name_full") << res->body;
    EXPECT_EQ(specs[0].at("required"), true) << res->body;

    res = cli.Post("/api/landlords", R"({"name_full": "ООО «Ромашка»", "name_short": "Ромашка", "inn": "7707083893"})", JSON);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 201) << res->body;
    auto const created = nlohmann::json::parse(res->body);
    auto const id = created.at("id").get<std::int64_t>();
    EXPECT_EQ(created.at("inn"), "7707083893") << res->body;
    EXPECT_EQ(created.at("city"), "") << "все поля в ответе: " << res->body;

    res = cli.Put("/api/landlords/" + std::to_string(id), R"({"id": 999, "city": "г. Химки"})", JSON);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200) << res->body;
    EXPECT_EQ(nlohmann::json::parse(res->body).at("city"), "г. Химки") << "id в теле игнорируется: " << res->body;

    res = cli.Get("/api/landlords");
    ASSERT_TRUE(res);
    auto const list = nlohmann::json::parse(res->body);
    ASSERT_EQ(list.size(), 1U) << res->body;
    EXPECT_EQ(list[0].at("id"), id);

    res = cli.Delete("/api/landlords/" + std::to_string(id));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 204) << res->body;
    res = cli.Get("/api/landlords/" + std::to_string(id));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404) << res->body;
}

/**
 * @brief Неверные запросы к арендодателям: 400 или 404 с текстом причины в JSON.
 */
TEST(Api, LandlordsErrors) {
    TestServer srv;
    auto cli = srv.client();
    constexpr char const JSON[] = "application/json";
    struct Case {
        std::string method;
        std::string path;
        std::string body;
        int status;
        std::string reason;
    };
    std::vector<Case> const cases = {
        {"POST", "/api/landlords", "не json", 400, "не JSON"},
        {"POST", "/api/landlords", "[]", 400, "JSON-объектом"},
        {"POST", "/api/landlords", R"({"name_full": "А", "name_short": 5})", 400, "поле name_short должно быть строкой"},
        {"POST", "/api/landlords", R"({"name_full": "А", "name_short": "Б", "inn": "123"})", 400, "ИНН организации должен состоять из 10 цифр"},
        {"PUT", "/api/landlords/77", R"({"city": "х"})", 404, "нет арендодателя с id 77"},
        {"GET", "/api/landlords/abc", "", 404, "неверный id: abc"},
    };
    for (auto const &c : cases) {
        SCOPED_TRACE(c.method + " " + c.path + " " + c.body);
        auto const res = c.method == "POST" ? cli.Post(c.path, c.body, JSON) : c.method == "PUT" ? cli.Put(c.path, c.body, JSON) : cli.Get(c.path);
        ASSERT_TRUE(res) << "нет ответа";
        EXPECT_EQ(res->status, c.status) << res->body;
        EXPECT_THAT(nlohmann::json::parse(res->body).at("error").get<std::string>(), HasSubstr(c.reason));
    }
}

/**
 * @brief Арендаторы через HTTP: виды с полями, создание, название и вид в ответе, смена вида, удаление.
 */
TEST(Api, TenantsCrud) {
    TestServer srv;
    auto cli = srv.client();
    constexpr char const JSON[] = "application/json";

    auto res = cli.Get("/api/tenants/kinds");
    ASSERT_TRUE(res) << "нет ответа: " << httplib::to_string(res.error());
    auto const kinds = nlohmann::json::parse(res->body);
    ASSERT_EQ(kinds.size(), 3U) << res->body;
    EXPECT_EQ(kinds[0].at("code"), "ip");
    auto const &gender = kinds[0].at("fields")[1];
    EXPECT_EQ(gender.at("key"), "gender") << res->body;
    EXPECT_EQ(gender.at("input"), "select");
    EXPECT_EQ(gender.at("choices").size(), 2U);

    res = cli.Post("/api/tenants", R"({"kind": "ip", "name": "Петров Пётр Петрович", "gender": "m", "inn": "500100732259"})", JSON);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 201) << res->body;
    auto const created = nlohmann::json::parse(res->body);
    auto const id = std::to_string(created.at("id").get<std::int64_t>());
    EXPECT_EQ(created.at("title"), "ИП Петров Пётр Петрович") << res->body;
    EXPECT_EQ(created.at("kind_label"), "Индивидуальный предприниматель") << res->body;

    res = cli.Put("/api/tenants/" + id, R"({"kind": "person", "birth_date": "1980-01-31"})", JSON);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200) << res->body;
    EXPECT_EQ(nlohmann::json::parse(res->body).at("title"), "Петров Пётр Петрович") << res->body;

    res = cli.Put("/api/tenants/" + id, R"({"birth_date": "1980-02-30"})", JSON);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400) << res->body;
    EXPECT_THAT(nlohmann::json::parse(res->body).at("error").get<std::string>(), HasSubstr("нет такой даты"));

    res = cli.Get("/api/tenants");
    ASSERT_TRUE(res);
    EXPECT_EQ(nlohmann::json::parse(res->body).size(), 1U) << res->body;

    res = cli.Delete("/api/tenants/" + id);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 204) << res->body;
    res = cli.Get("/api/tenants/" + id);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404) << res->body;
}

/**
 * @brief Заполнение по ИНН и БИК через HTTP: поля для нужной карточки, ошибки 400/404/502 с причиной.
 */
TEST(Api, Lookup) {
    fake::FakeDaData dadata;
    TestServer srv{{}, dadata.client("good")};
    auto cli = srv.client();

    auto res = cli.Get("/api/lookup/party?for=landlord&inn=7707083893");
    ASSERT_TRUE(res) << "нет ответа: " << httplib::to_string(res.error());
    ASSERT_EQ(res->status, 200) << res->body;
    auto const fields = nlohmann::json::parse(res->body).at("fields");
    EXPECT_EQ(fields.at("kpp"), "773601001") << res->body;
    EXPECT_EQ(fields.at("signer_name_gen"), "Петрова Петра Павловича") << res->body;

    res = cli.Get("/api/lookup/party?for=ip&inn=%20500100732259%20");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << "пробелы вокруг ИНН убираются: " << res->body;
    EXPECT_EQ(nlohmann::json::parse(res->body).at("fields").at("gender"), "f") << res->body;

    res = cli.Get("/api/lookup/bank?bik=044525225");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    EXPECT_EQ(nlohmann::json::parse(res->body).at("fields").at("bank"), "ПАО «Сбербанк»") << res->body;

    struct Case {
        std::string path;
        int status;
        std::string reason;
    };
    std::vector<Case> const cases = {
        {"/api/lookup/party?for=bank&inn=7707083893", 400, "параметр for"},
        {"/api/lookup/party?for=org", 400, "не указан ИНН"},
        {"/api/lookup/party?for=ip&inn=7707083893", 400, "зарегистрирована организация"},
        {"/api/lookup/party?for=org&inn=7728168971", 404, "ничего не найдено"},
    };
    for (auto const &c : cases) {
        SCOPED_TRACE(c.path);
        res = cli.Get(c.path);
        ASSERT_TRUE(res);
        EXPECT_EQ(res->status, c.status) << res->body;
        EXPECT_THAT(nlohmann::json::parse(res->body).at("error").get<std::string>(), HasSubstr(c.reason));
    }

    TestServer no_key;
    res = no_key.client().Get("/api/lookup/bank?bik=044525225");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400) << res->body;
    EXPECT_THAT(nlohmann::json::parse(res->body).at("error").get<std::string>(), HasSubstr("ARENDA_DADATA_KEY"));

    TestServer bad_key{{}, dadata.client("bad")};
    res = bad_key.client().Get("/api/lookup/bank?bik=044525225");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 502) << res->body;
}

/**
 * @brief Оборудование арендатора через HTTP: замена перечня, мощность строкой в ответе, ошибки.
 */
TEST(Api, TenantEquipment) {
    TestServer srv;
    auto cli = srv.client();
    constexpr char const JSON[] = "application/json";
    auto res = cli.Post("/api/tenants", R"({"kind": "ip", "name": "Петров Пётр Петрович", "gender": "m", "inn": "500100732259"})", JSON);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 201) << res->body;
    auto const path = "/api/tenants/" + std::to_string(nlohmann::json::parse(res->body).at("id").get<std::int64_t>()) + "/equipment";

    res = cli.Put(path, R"([{"name": "Станок", "model": "Silca", "power": "0,55", "qty": 1}, {"name": "Точило", "power": "0.25", "qty": 2}])", JSON);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    auto const rows = nlohmann::json::parse(res->body);
    ASSERT_EQ(rows.size(), 2U) << res->body;
    EXPECT_EQ(rows[0].at("power"), "0,55");
    EXPECT_EQ(rows[1].at("model"), "") << res->body;

    res = cli.Get(path);
    ASSERT_TRUE(res);
    EXPECT_EQ(nlohmann::json::parse(res->body).size(), 2U) << res->body;

    res = cli.Put(path, R"([{"name": "Станок", "power": "мощный"}])", JSON);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400) << res->body;
    EXPECT_THAT(nlohmann::json::parse(res->body).at("error").get<std::string>(), HasSubstr("строка 1: мощность, кВт: нужно число"));

    res = cli.Put(path, R"({"name": "Станок"})", JSON);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400) << res->body;

    res = cli.Get("/api/tenants/999/equipment");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404) << res->body;
}

/**
 * @brief Договоры через HTTP: расчёт без записи, создание, список, обновление из карточек, удаление, ошибки.
 */
TEST(Api, Contracts) {
    TestServer srv;
    auto cli = srv.client();
    constexpr char const JSON[] = "application/json";
    auto const id_of = [](httplib::Result const &res) { return nlohmann::json::parse(res->body).at("id").get<std::int64_t>(); };
    auto res = cli.Post("/api/landlords", R"({"name_full": "ООО «Ромашка»", "name_short": "Ромашка"})", JSON);
    ASSERT_TRUE(res);
    auto const landlord = id_of(res);
    res = cli.Post("/api/tenants", R"({"kind": "ip", "name": "Петров Пётр Петрович", "gender": "m", "inn": "500100732259"})", JSON);
    ASSERT_TRUE(res);
    auto const tenant = id_of(res);
    auto const body =
        nlohmann::json{{"landlord_id", landlord}, {"rows", {{{"tenant_id", tenant}, {"start", "2026-10-19"}, {"rent", "20 000"}}}}}.dump();

    res = cli.Post("/api/contracts/plan", body, JSON);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    auto const plan = nlohmann::json::parse(res->body);
    ASSERT_EQ(plan.size(), 3U) << res->body;
    EXPECT_EQ(plan[0].at("number"), "2026-10-19-500100732259");
    EXPECT_EQ(plan[0].at("rent"), "20 000,00");
    EXPECT_EQ(plan[0].at("total"), "80 000,00") << "4 месяца по 20 000";
    EXPECT_EQ(plan[0].at("tenant_title"), "ИП Петров Пётр Петрович");

    res = cli.Get("/api/contracts");
    ASSERT_TRUE(res);
    EXPECT_EQ(nlohmann::json::parse(res->body).size(), 0U) << "расчёт не записывает";

    res = cli.Post("/api/contracts", body, JSON);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 201) << res->body;
    auto const created = nlohmann::json::parse(res->body);
    ASSERT_EQ(created.size(), 3U);
    auto const first = "/api/contracts/" + std::to_string(created[0].at("id").get<std::int64_t>());

    res = cli.Post("/api/contracts", body, JSON);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400) << res->body;
    EXPECT_THAT(nlohmann::json::parse(res->body).at("error").get<std::string>(), HasSubstr("уже есть"));

    res = cli.Delete("/api/tenants/" + std::to_string(tenant));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400) << "арендатора с договорами не удалить: " << res->body;

    res = cli.Put("/api/tenants/" + std::to_string(tenant), R"({"floor": "3"})", JSON);
    ASSERT_TRUE(res);
    res = cli.Post(first + "/refresh", "", JSON);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    EXPECT_EQ(nlohmann::json::parse(res->body).at("tenant").at("floor"), "3") << res->body;

    res = cli.Delete(first);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 204);
    res = cli.Get("/api/contracts");
    ASSERT_TRUE(res);
    EXPECT_EQ(nlohmann::json::parse(res->body).size(), 2U) << res->body;

    res = cli.Post("/api/contracts/plan", R"({"rows": []})", JSON);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
    EXPECT_THAT(nlohmann::json::parse(res->body).at("error").get<std::string>(), HasSubstr("выберите арендодателя"));
    res = cli.Post("/api/contracts/plan",
                   nlohmann::json{{"landlord_id", landlord}, {"rows", {{{"tenant_id", tenant}, {"start", "2026-10-19"}, {"rent", 5}}}}}.dump(), JSON);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
    EXPECT_THAT(nlohmann::json::parse(res->body).at("error").get<std::string>(), HasSubstr("строка 1: укажите плату в месяц строкой"));
}

/**
 * @brief Печать: один договор и пачка как HTML, ошибки списка и несуществующий договор.
 */
TEST(Api, Print) {
    TestServer srv;
    auto cli = srv.client();
    constexpr char const JSON[] = "application/json";
    auto res = cli.Post("/api/landlords", R"({"name_full": "ООО «Ромашка»", "name_short": "Ромашка"})", JSON);
    ASSERT_TRUE(res);
    auto const landlord = nlohmann::json::parse(res->body).at("id").get<std::int64_t>();
    res = cli.Post("/api/tenants", R"({"kind": "ip", "name": "Петров Пётр Петрович", "gender": "m", "inn": "500100732259"})", JSON);
    ASSERT_TRUE(res);
    auto const tenant = nlohmann::json::parse(res->body).at("id").get<std::int64_t>();
    res = cli.Post("/api/contracts",
                   nlohmann::json{{"landlord_id", landlord}, {"rows", {{{"tenant_id", tenant}, {"start", "2026-10-19"}, {"rent", "20 000"}}}}}.dump(),
                   JSON);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 201) << res->body;
    auto const created = nlohmann::json::parse(res->body);
    auto const a = std::to_string(created[0].at("id").get<std::int64_t>());
    auto const b = std::to_string(created[1].at("id").get<std::int64_t>());

    res = cli.Get("/print/contracts/" + a);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    EXPECT_THAT(res->get_header_value("Content-Type"), HasSubstr("text/html"));
    EXPECT_THAT(res->body, HasSubstr("<title>Договор аренды № 2026-10-19-500100732259</title>"));

    res = cli.Get("/print/contracts?ids=" + b + "," + a);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    EXPECT_THAT(res->body, HasSubstr("<title>Договоры аренды: 2</title>"));
    EXPECT_LT(res->body.find("№ 2027-02-19-500100732259"), res->body.find("№ 2026-10-19-500100732259")) << "порядок как в ids";

    res = cli.Get("/print/contracts?ids=1,x");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400) << res->body;
    res = cli.Get("/print/contracts?ids=");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400) << res->body;
    res = cli.Get("/print/contracts/999");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404) << res->body;
}

/**
 * @brief Без входа доступны только версия, состояние входа и вход; данные и печать отвечают 401.
 */
TEST(Api, LoginRequired) {
    TestServer srv;
    auto cli = srv.anonymous();
    for (auto const *path :
         {"/api/landlords", "/api/tenants", "/api/contracts", "/api/users", "/print/contracts?ids=1", "/api/lookup/bank?bik=044525225"}) {
        SCOPED_TRACE(path);
        auto const res = cli.Get(path);
        ASSERT_TRUE(res);
        EXPECT_EQ(res->status, 401) << res->body;
        EXPECT_THAT(nlohmann::json::parse(res->body).at("error").get<std::string>(), HasSubstr("нужно войти"));
    }
    auto res = cli.Get("/api/version");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    res = cli.Get("/api/auth");
    ASSERT_TRUE(res);
    auto const state = nlohmann::json::parse(res->body);
    EXPECT_EQ(state.at("setup"), false) << "администратор уже создан: " << res->body;
    EXPECT_TRUE(state.at("user").is_null());
    res = cli.Post("/api/auth/setup", R"({"login": "second", "password": "secret1"})", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400) << "второй раз первого администратора не создать: " << res->body;
}

/**
 * @brief Вход, выход, пользователь без прав администратора, смена пароля.
 */
TEST(Api, LoginLogoutAndRoles) {
    TestServer srv;
    constexpr char const JSON[] = "application/json";
    auto res = srv.client().Post("/api/users", R"({"login": "olga", "name": "Ольга", "password": "secret2"})", JSON);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 201) << res->body;
    EXPECT_EQ(nlohmann::json::parse(res->body).at("role"), "user");

    res = srv.anonymous().Post("/api/auth/login", R"({"login": "olga", "password": "wrong!"})", JSON);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
    EXPECT_THAT(nlohmann::json::parse(res->body).at("error").get<std::string>(), HasSubstr("неверный логин или пароль"));

    res = srv.anonymous().Post("/api/auth/login", R"({"login": "OLGA", "password": "secret2"})", JSON);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << "логин без учёта регистра: " << res->body;
    auto const olga = TestServer::cookie_of(*res);
    EXPECT_THAT(res->get_header_value("Set-Cookie"), HasSubstr("HttpOnly"));

    res = srv.client_as(olga).Get("/api/landlords");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200) << "пользователю доступны справочники";
    res = srv.client_as(olga).Get("/api/users");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 403) << res->body;
    EXPECT_THAT(nlohmann::json::parse(res->body).at("error").get<std::string>(), HasSubstr("это делает администратор"));

    res = srv.client_as(olga).Put("/api/auth/password", R"({"old_password": "secret2", "password": "secret3"})", JSON);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    auto const renewed = TestServer::cookie_of(*res);
    res = srv.client_as(olga).Get("/api/landlords");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401) << "смена пароля закрывает прежние сессии";
    res = srv.client_as(renewed).Post("/api/auth/logout", "", JSON);
    ASSERT_TRUE(res);
    res = srv.client_as(renewed).Get("/api/landlords");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401) << "после выхода сессия закрыта";
}

/**
 * @brief Резервные копии через HTTP: только администратор; копия, скачивание, расписание, удаление.
 */
TEST(Api, Backups) {
    TestServer srv;
    auto cli = srv.client();
    constexpr char const JSON[] = "application/json";
    auto res = cli.Get("/api/backups");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    auto state = nlohmann::json::parse(res->body);
    EXPECT_EQ(state.at("schedule").at("enabled"), true) << "расписание включено по умолчанию: " << res->body;
    EXPECT_EQ(state.at("backups").size(), 0U);

    res = cli.Post("/api/backups", "", JSON);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 201) << res->body;
    auto const name = nlohmann::json::parse(res->body).at("created").get<std::string>();
    EXPECT_THAT(name, testing::MatchesRegex(R"(arenda-[0-9]{8}-[0-9]{6}-manual\.db)"));

    res = cli.Get("/api/backups/" + name);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200);
    EXPECT_THAT(res->get_header_value("Content-Disposition"), HasSubstr(name));
    EXPECT_EQ(res->body.substr(0, 15), "SQLite format 3") << "скачивается файл базы SQLite";

    res = cli.Put("/api/backups/schedule", R"({"enabled": false, "time": "22:15", "keep": 7})", JSON);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    state = nlohmann::json::parse(res->body).at("schedule");
    EXPECT_EQ(state.at("time"), "22:15");
    EXPECT_EQ(state.at("keep"), 7);
    res = cli.Put("/api/backups/schedule", R"({"time": "25:00"})", JSON);
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400) << res->body;

    res = cli.Get("/api/backups/..%2Farenda.db");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404) << "выход из каталога копий: " << res->body;

    res = cli.Post("/api/users", R"({"login": "olga", "password": "secret2"})", JSON);
    ASSERT_TRUE(res);
    res = srv.anonymous().Post("/api/auth/login", R"({"login": "olga", "password": "secret2"})", JSON);
    ASSERT_TRUE(res);
    res = srv.client_as(TestServer::cookie_of(*res)).Get("/api/backups");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 403) << "копии только у администратора: " << res->body;

    res = cli.Delete("/api/backups/" + name);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    EXPECT_EQ(nlohmann::json::parse(res->body).at("backups").size(), 0U);
}
