#include "users.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "db.hpp"
#include "errors.hpp"
#include "schema.hpp"
#include "test_util.hpp"

namespace {

class UsersTest : public testing::Test {
   protected:
    UsersTest() { migrate(m_db); }

    std::int64_t add_user(std::string const &login, Role role, std::string const &password = "secret1") {
        User user;
        user.login = login;
        user.role = role;
        return m_users.create(user, password);
    }

    Database m_db{":memory:"};
    Users m_users{m_db};
};

} // namespace

/**
 * @brief Первый пользователь создаётся только в пустой базе и всегда администратором.
 */
TEST_F(UsersTest, SetupOnlyOnce) {
    EXPECT_TRUE(m_users.empty());
    User first;
    first.login = " anna ";
    first.name = "Анна";
    first.role = Role::USER;
    auto const id = m_users.setup(first, "secret1");
    EXPECT_FALSE(m_users.empty());
    auto const user = m_users.get(id);
    EXPECT_EQ(user.login, "anna") << "логин обрезается по краям";
    EXPECT_EQ(user.role, Role::ADMIN) << "первый пользователь - администратор, какую бы роль ни передали";
    EXPECT_THROW_REASON(m_users.setup(first, "secret1"), UserError, "первый пользователь уже создан");
}

/**
 * @brief Вход: верный пароль открывает сессию, неверный пароль, неизвестный логин и блокировка отклоняются;
 * логин без учёта регистра; выход и блокировка закрывают сессию.
 */
TEST_F(UsersTest, LoginAndSessions) {
    auto const id = add_user("olga", Role::USER);
    auto const token = m_users.login("OLGA", "secret1");
    EXPECT_EQ(token.size(), 64U) << "токен - 32 случайных байта в hex";
    auto const user = m_users.by_session(token);
    ASSERT_TRUE(user.has_value()) << "сессия должна действовать";
    EXPECT_EQ(user.value_or(User{}).id, id);
    EXPECT_FALSE(m_users.by_session("deadbeef")) << "неизвестный токен";
    EXPECT_FALSE(m_users.by_session("")) << "пустой токен";

    EXPECT_THROW_REASON((void)m_users.login("olga", "wrong!"), UserError, "неверный логин или пароль");
    EXPECT_THROW_REASON((void)m_users.login("nobody", "secret1"), UserError, "неверный логин или пароль");

    m_users.logout(token);
    EXPECT_FALSE(m_users.by_session(token)) << "после выхода сессия закрыта";

    auto const second = m_users.login("olga", "secret1");
    add_user("boss", Role::ADMIN);
    auto blocked = m_users.get(id);
    blocked.blocked = true;
    m_users.update(blocked);
    EXPECT_FALSE(m_users.by_session(second)) << "блокировка закрывает сессии";
    EXPECT_THROW_REASON((void)m_users.login("olga", "secret1"), UserError, "заблокирован");
}

/**
 * @brief Пароль хранится хешем; смена пароля закрывает сессии и меняет проверку; короткий отклоняется.
 */
TEST_F(UsersTest, Password) {
    auto const id = add_user("olga", Role::USER);
    auto stmt = m_db.prepare("SELECT pass_hash FROM users WHERE id = ?");
    stmt.bind(1, id);
    stmt.step();
    EXPECT_THAT(stmt.column_text(0), testing::StartsWith("pbkdf2-sha256$")) << "пароль не должен храниться открыто";
    EXPECT_THAT(stmt.column_text(0), testing::Not(testing::HasSubstr("secret1")));

    auto const token = m_users.login("olga", "secret1");
    m_users.set_password(id, "newpass");
    EXPECT_FALSE(m_users.by_session(token)) << "смена пароля закрывает сессии";
    EXPECT_TRUE(m_users.check_password(id, "newpass"));
    EXPECT_FALSE(m_users.check_password(id, "secret1")) << "старый пароль больше не подходит";
    EXPECT_THROW_REASON(m_users.set_password(id, "12345"), UserError, "короче 6");
    EXPECT_THROW_REASON(add_user("short", Role::USER, "abc"), UserError, "короче 6");
}

/**
 * @brief Логин: пустой, с пробелом и занятый (без учёта регистра) отклоняются.
 */
TEST_F(UsersTest, LoginValidation) {
    add_user("olga", Role::USER);
    EXPECT_THROW_REASON(add_user("   ", Role::USER), UserError, "логин не может быть пустым");
    EXPECT_THROW_REASON(add_user("olga petrova", Role::USER), UserError, "не должно быть пробелов");
    EXPECT_THROW_REASON(add_user("Olga", Role::USER), UserError, "логин «Olga» уже занят");
    EXPECT_THROW_REASON((void)role_from_string("boss"), UserError, "неизвестная роль");
}

/**
 * @brief Последнего действующего администратора нельзя ни понизить, ни заблокировать.
 */
TEST_F(UsersTest, LastAdminStays) {
    auto const admin = add_user("admin", Role::ADMIN);
    auto user = m_users.get(admin);
    user.role = Role::USER;
    EXPECT_THROW_REASON(m_users.update(user), UserError, "последний администратор");
    user = m_users.get(admin);
    user.blocked = true;
    EXPECT_THROW_REASON(m_users.update(user), UserError, "последний администратор");

    add_user("boss", Role::ADMIN);
    user.blocked = false;
    user.role = Role::USER;
    EXPECT_NO_THROW(m_users.update(user)) << "второй администратор есть: первого можно понизить";
    EXPECT_EQ(m_users.get(admin).role, Role::USER);
}
