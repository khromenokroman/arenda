#include "requisites.hpp"

#include <gtest/gtest.h>

#include "errors.hpp"
#include "test_util.hpp"

using namespace requisites;

/**
 * @brief Пустой реквизит допустим: незаполненное поле печатается прочерком.
 */
TEST(Requisites, EmptyIsAllowed) {
    EXPECT_NO_THROW(check_inn_org(""));
    EXPECT_NO_THROW(check_inn_person(""));
    EXPECT_NO_THROW(check_ogrn(""));
    EXPECT_NO_THROW(check_ogrnip(""));
    EXPECT_NO_THROW(check_kpp(""));
    EXPECT_NO_THROW(check_bik(""));
    EXPECT_NO_THROW(check_account(""));
    EXPECT_NO_THROW(check_account_bik("", "044525225")) << "без счёта ключ не проверяется";
    EXPECT_NO_THROW(check_account_bik("40702810538000123456", "")) << "без БИК ключ не проверяется";
}

/**
 * @brief Настоящие реквизиты с верными контрольными цифрами принимаются.
 */
TEST(Requisites, ValidValues) {
    EXPECT_NO_THROW(check_inn_org("7707083893"));
    EXPECT_NO_THROW(check_inn_person("500100732259"));
    EXPECT_NO_THROW(check_ogrn("1027700132195"));
    EXPECT_NO_THROW(check_ogrnip("304500116000157"));
    EXPECT_NO_THROW(check_kpp("773601001"));
    EXPECT_NO_THROW(check_kpp("7736AB001")) << "5-6 знаки КПП могут быть латинскими буквами";
    EXPECT_NO_THROW(check_bik("044525225"));
    EXPECT_NO_THROW(check_account_bik("40702810538000123456", "044525225"));
    EXPECT_NO_THROW(check_account_bik("40817810700000000001", "044525225"));
}

/**
 * @brief Неверная длина или не цифры: отказ называет реквизит и нужное число цифр.
 */
TEST(Requisites, WrongFormat) {
    EXPECT_THROW_REASON(check_inn_org("770708389"), UserError, "ИНН организации должен состоять из 10 цифр");
    EXPECT_THROW_REASON(check_inn_org("77070838 3"), UserError, "из 10 цифр: «77070838 3»");
    EXPECT_THROW_REASON(check_inn_person("7707083893"), UserError, "ИНН физического лица должен состоять из 12 цифр");
    EXPECT_THROW_REASON(check_ogrn("102770013219"), UserError, "ОГРН должен состоять из 13 цифр");
    EXPECT_THROW_REASON(check_ogrnip("1027700132195"), UserError, "ОГРНИП должен состоять из 15 цифр");
    EXPECT_THROW_REASON(check_kpp("77360100"), UserError, "КПП должен состоять из 9 знаков");
    EXPECT_THROW_REASON(check_kpp("7736ab001"), UserError, "КПП должен состоять из 9 знаков");
    EXPECT_THROW_REASON(check_bik("04452522A"), UserError, "БИК должен состоять из 9 цифр");
    EXPECT_THROW_REASON(check_account("4070281053800012345"), UserError, "Расчётный счёт должен состоять из 20 цифр");
}

/**
 * @brief Опечатка в одной цифре ловится контрольной цифрой.
 */
TEST(Requisites, WrongControlDigit) {
    EXPECT_THROW_REASON(check_inn_org("7707083894"), UserError, "ИНН «7707083894» с ошибкой");
    EXPECT_THROW_REASON(check_inn_person("500100732258"), UserError, "ИНН «500100732258» с ошибкой");
    {
        SCOPED_TRACE("неверна 11-я цифра");
        EXPECT_THROW_REASON(check_inn_person("500100732269"), UserError, "с ошибкой");
    }
    EXPECT_THROW_REASON(check_ogrn("1027700132196"), UserError, "ОГРН «1027700132196» с ошибкой");
    EXPECT_THROW_REASON(check_ogrnip("304500116000158"), UserError, "ОГРНИП «304500116000158» с ошибкой");
}

/**
 * @brief Счёт от другого банка или с опечаткой не сходится с БИК.
 */
TEST(Requisites, AccountDoesNotMatchBik) {
    EXPECT_THROW_REASON(check_account_bik("40702810538000123457", "044525225"), UserError,
                        "расчётный счёт «40702810538000123457» не подходит к БИК «044525225»");
    EXPECT_THROW_REASON(check_account_bik("40702810538000123456", "044525226"), UserError, "не подходит к БИК");
    SCOPED_TRACE("длина счёта проверяется и при сверке с БИК");
    EXPECT_THROW_REASON(check_account_bik("407028105380001234", "044525225"), UserError, "из 20 цифр");
}
