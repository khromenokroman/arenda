#include "dadata.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "errors.hpp"
#include "fake_dadata.hpp"
#include "test_util.hpp"

using fake::FakeDaData;
using fake::individual_data;
using fake::legal_data;

/**
 * @brief Запрос уходит с ключом в заголовке и ИНН в теле; организация ищется без филиалов.
 */
TEST(DaData, RequestFormat) {
    FakeDaData fake;
    auto const data = fake.client("good").find_party("7707083893");
    EXPECT_EQ(fake.authorization(), "Token good");
    EXPECT_EQ(fake.body().at("query"), "7707083893") << fake.body().dump();
    EXPECT_EQ(fake.body().at("branch_type"), "MAIN") << "нужна основная организация, а не филиал";
    EXPECT_EQ(data.at("ogrn"), "1027700132195");
}

/**
 * @brief Организация: наименования с формой собственности и ёлочками, адрес с индексом, подписант с родительным падежом.
 */
TEST(DaData, LegalFields) {
    auto const f = party_fields(legal_data(), "landlord");
    EXPECT_EQ(f.at("name_full"), "Общество с ограниченной ответственностью «ТОРГОВЫЙ ДОМ «ВОСХОД»»");
    EXPECT_EQ(f.at("name_short"), "ООО «ТД «ВОСХОД»»");
    EXPECT_EQ(f.at("inn"), "7707083893");
    EXPECT_EQ(f.at("kpp"), "773601001");
    EXPECT_EQ(f.at("ogrn"), "1027700132195");
    EXPECT_EQ(f.at("address"), "117312, г Москва, ул Ленина, д 1") << "адрес с индексом";
    EXPECT_EQ(f.at("signer_post"), "Генеральный директор");
    EXPECT_EQ(f.at("signer_post_gen"), "генерального директора");
    EXPECT_EQ(f.at("signer_name"), "Петров Пётр Павлович");
    EXPECT_EQ(f.at("signer_name_gen"), "Петрова Петра Павловича");
    EXPECT_EQ(f.at("signer_basis_gen"), "Устава");
    EXPECT_EQ(f.count("city"), 0U) << "данные объекта DaData не знает и не трогает";
}

/**
 * @brief Руководитель-организация (управляющая компания): подписант не заполняется.
 */
TEST(DaData, ManagerIsCompany) {
    auto data = legal_data();
    data["management"] = {{"name", "ООО \"УК\""}, {"post", "УПРАВЛЯЮЩАЯ ОРГАНИЗАЦИЯ"}};
    auto const f = party_fields(data, "org");
    EXPECT_EQ(f.count("signer_name"), 0U);
    EXPECT_EQ(f.at("name_short"), "ООО «ТД «ВОСХОД»»");
}

/**
 * @brief ИП: ФИО из частей, пол по отчеству, ОГРНИП.
 */
TEST(DaData, IndividualFields) {
    auto const f = party_fields(individual_data(), "ip");
    EXPECT_EQ(f.at("name"), "Сидорова Анна Сергеевна");
    EXPECT_EQ(f.at("gender"), "f");
    EXPECT_EQ(f.at("ogrnip"), "304500116000157");
    EXPECT_EQ(f.at("inn"), "500100732259");
    EXPECT_EQ(f.at("address"), "Московская обл, г Химки");
}

/**
 * @brief По ИНН найден не тот вид: организация для ИП и наоборот.
 */
TEST(DaData, WrongPartyType) {
    EXPECT_THROW_REASON((void)party_fields(individual_data(), "landlord"), UserError, "зарегистрирован индивидуальный предприниматель");
    EXPECT_THROW_REASON((void)party_fields(legal_data(), "ip"), UserError, "зарегистрирована организация");
}

/**
 * @brief Банк по БИК: наименование для платёжных документов и корреспондентский счёт; у банка без к/с поле пустое.
 */
TEST(DaData, Bank) {
    FakeDaData fake;
    auto const f = bank_fields(fake.client("good").find_bank("044525225"));
    EXPECT_EQ(f.at("bank"), "ПАО «Сбербанк»");
    EXPECT_EQ(f.at("corr_account"), "30101810400000000225");
    EXPECT_EQ(bank_fields(nlohmann::json::parse(R"({"name": {"payment": "УФК"}, "correspondent_account": null})")).at("corr_account"), "")
        << "у казначейства корр. счёта нет";
}

/**
 * @brief Ошибки: нет ключа, неверный ключ, ничего не найдено, неверный ИНН до запроса, сервис недоступен.
 */
TEST(DaData, Errors) {
    FakeDaData fake;
    EXPECT_THROW_REASON((void)fake.client("").find_party("7707083893"), UserError, "ARENDA_DADATA_KEY");
    EXPECT_THROW_REASON((void)fake.client("bad").find_party("7707083893"), ExternalError, "DaData отклонил ключ (HTTP 403)");
    EXPECT_THROW_REASON((void)fake.client("good").find_party("7728168971"), NotFoundError, "по ИНН 7728168971 в DaData ничего не найдено");
    EXPECT_THROW_REASON((void)fake.client("good").find_bank("044525999"), NotFoundError, "по БИК 044525999");
    EXPECT_THROW_REASON((void)fake.client("good").find_party("7707083894"), UserError, "с ошибкой");
    EXPECT_THROW_REASON((void)fake.client("good").find_bank(""), UserError, "не указан БИК");
    DaData const down{"http://127.0.0.1:1", "good"};
    EXPECT_THROW_REASON((void)down.find_party("7707083893"), ExternalError, "DaData не отвечает");
}
