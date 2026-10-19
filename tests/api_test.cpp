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
#include "schema.hpp"

using testing::HasSubstr;

namespace {

/**
 * @brief Сервер API на случайном локальном порте, работающий в отдельном потоке.
 */
class TestServer {
   public:
    explicit TestServer(std::string const &web_dir = {}) : m_db{":memory:"} {
        migrate(m_db);
        m_api = std::make_unique<Api>(m_db, web_dir);
        m_api->register_routes(m_server);
        m_port = m_server.bind_to_any_port("127.0.0.1");
        m_thread = std::thread{[this] { m_server.listen_after_bind(); }};
        while (!m_server.is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    }

    ~TestServer() {
        m_server.stop();
        m_thread.join();
    }

    TestServer(TestServer const &) = delete;
    TestServer &operator=(TestServer const &) = delete;
    TestServer(TestServer &&) = delete;
    TestServer &operator=(TestServer &&) = delete;

    [[nodiscard]] httplib::Client client() const { return httplib::Client{"127.0.0.1", m_port}; }

   private:
    httplib::Server m_server;
    Database m_db;
    std::unique_ptr<Api> m_api;
    std::thread m_thread;
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
