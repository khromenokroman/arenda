#include "contract_print.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "errors.hpp"
#include "test_util.hpp"

using contract_print::fill;
using contract_print::render;
using contract_print::Values;
using testing::HasSubstr;
using testing::Not;

namespace {

/// Договор с заполненными карточками; tenant - поля арендатора поверх общих.
Contract sample(Fields tenant) {
    Contract c;
    c.id = 1;
    c.number = "2026-10-19-500100732259";
    c.start = "2026-10-19";
    c.end = "2027-02-18";
    c.months = 4;
    c.rent_kop = 2'000'000;
    c.landlord = {{"name_full", "Общество с ограниченной ответственностью «Ромашка»"},
                  {"name_short", "ООО «Ромашка»"},
                  {"ogrn", "1027700132195"},
                  {"inn", "7707083893"},
                  {"kpp", "773601001"},
                  {"signer_post", "Директор"},
                  {"signer_post_gen", "директора"},
                  {"signer_name", "Иванова Мария Петровна"},
                  {"signer_name_gen", "Ивановой Марии Петровны"},
                  {"signer_basis_gen", "Устава"},
                  {"city", "г. Химки"},
                  {"object_name_gen", "торгового центра «Ромашка»"},
                  {"object_address", "Московская обл., г. Химки, ул. Ленина, д. 1"},
                  {"region_gen", "Московской области"},
                  {"municipality_gen", "городского округа Химки"},
                  {"hours_weekdays", "с 10:00 до 20:00"},
                  {"hours_weekends", "с 10:00 до 18:00"},
                  {"days_off", "1 января"},
                  {"fire_fine", "7000"}};
    tenant.emplace("area", "12,5");
    tenant.emplace("floor", "2");
    tenant.emplace("activity_gen", "металлоремонта");
    c.tenant = std::move(tenant);
    c.equipment = {{"Станок <для ключей>", "Silca", 55, 2}};
    return c;
}

Fields ip() { return {{"kind", "ip"}, {"name", "Петров Пётр Петрович"}, {"gender", "m"}, {"inn", "500100732259"}, {"ogrnip", "304500116000157"}}; }

} // namespace

/**
 * @brief Метки: текст экранируется, пустой - прочерк, HTML вставляется как есть; метка без значения - ошибка кода.
 */
TEST(ContractPrint, Fill) {
    Values const v{.text = {{"a", "<b>"}, {"empty", ""}}, .html = {{"h", "<i>x</i>"}}};
    EXPECT_EQ(fill("[{{a}}] [{{empty}}] [{{{h}}}]", v), "[&lt;b&gt;] [____________] [<i>x</i>]");
    EXPECT_THROW((void)fill("{{nope}}", v), std::logic_error);
    EXPECT_THROW((void)fill("{{a", v), std::logic_error);
}

/**
 * @brief ИП: вступление, «именуемый», сумма прописью, даты, условия арендодателя, подписант-женщина «действующей».
 */
TEST(ContractPrint, IndividualEntrepreneur) {
    auto const html = render({sample(ip())});
    EXPECT_THAT(html, HasSubstr("Индивидуальный предприниматель Петров Пётр Петрович (ОГРНИП 304500116000157, ИНН 500100732259), именуемый в "
                                "дальнейшем «Арендатор»"));
    EXPECT_THAT(html, HasSubstr("в лице директора Ивановой Марии Петровны, действующей на основании Устава, именуемое"));
    EXPECT_THAT(html, HasSubstr("4.1. Арендная плата составляет 20 000 (Двадцать тысяч) рублей 00 копеек в месяц."));
    EXPECT_THAT(html, HasSubstr("Договор действует с «19» октября 2026 г. по «18» февраля 2027 г."));
    EXPECT_THAT(html, HasSubstr("на 2 этаже торгового центра «Ромашка» (далее — «Универмаг», «Объект»)"));
    EXPECT_THAT(html, HasSubstr("необходимые для металлоремонта, только из перечня, согласованного с Арендодателем (Приложение № 1)"));
    EXPECT_THAT(html, HasSubstr("штрафа 7 000 рублей."));
    EXPECT_THAT(html, HasSubstr("Арбитражный суд Московской области"));
    EXPECT_THAT(html, HasSubstr("понедельник — пятница: с 10:00 до 20:00"));
    EXPECT_THAT(html, HasSubstr("Директор ____________ / М. П. Иванова /"));
    EXPECT_THAT(html, HasSubstr("____________ / П. П. Петров /<br>М. П. (при наличии)"));
    EXPECT_THAT(html, HasSubstr("<td>Станок &lt;для ключей&gt;</td><td>Silca</td><td>0,55</td><td>2</td>")) << "оборудование экранировано";
    EXPECT_THAT(html, HasSubstr("Суммарная мощность: 1,10 кВт"));
    EXPECT_THAT(html, HasSubstr("<title>Договор аренды № 2026-10-19-500100732259</title>"));
    EXPECT_THAT(html, Not(HasSubstr("{{"))) << "остались неподставленные метки";
    EXPECT_THAT(html, Not(HasSubstr("Приложение № 3"))) << "приложение со схемой убрано, остальные перенумерованы";
}

/**
 * @brief Организация-арендатор: в лице подписанта, «именуемое», КПП в реквизитах.
 */
TEST(ContractPrint, Organization) {
    auto const html = render({sample({{"kind", "org"},
                                      {"name_full", "Общество с ограниченной ответственностью «Берёза»"},
                                      {"name_short", "ООО «Берёза»"},
                                      {"inn", "7707083893"},
                                      {"kpp", "773601001"},
                                      {"ogrn", "1027700132195"},
                                      {"signer_post", "Генеральный директор"},
                                      {"signer_post_gen", "генерального директора"},
                                      {"signer_name", "Сидоров Олег Ильич"},
                                      {"signer_name_gen", "Сидорова Олега Ильича"},
                                      {"signer_basis_gen", "Устава"}})});
    EXPECT_THAT(html, HasSubstr("«Берёза» (ОГРН 1027700132195, ИНН 7707083893), в лице генерального директора Сидорова Олега Ильича, "
                                "действующего на основании Устава, именуемое в дальнейшем «Арендатор»"));
    EXPECT_THAT(html, HasSubstr("ИНН/КПП: 7707083893/773601001"));
    EXPECT_THAT(html, HasSubstr("Генеральный директор ____________ / О. И. Сидоров /"));
}

/**
 * @brief Физлицо: паспорт и регистрация во вступлении, «именуемая»; без счёта банк не печатается; пустое - прочерк.
 */
TEST(ContractPrint, Person) {
    auto const html = render({sample({{"kind", "person"},
                                      {"name", "Сидорова Анна Сергеевна"},
                                      {"gender", "f"},
                                      {"birth_date", "1990-05-17"},
                                      {"passport", "4510 123456"},
                                      {"passport_issued", "ГУ МВД России по Московской области"},
                                      {"passport_date", "2010-06-01"},
                                      {"passport_code", "770-001"},
                                      {"inn", "526317984689"}})});
    EXPECT_THAT(html, HasSubstr("Сидорова Анна Сергеевна, 17.05.1990 года рождения, паспорт 4510 123456, выдан ГУ МВД России по Московской "
                                "области 01.06.2010, код подразделения 770-001, зарегистрированная по адресу: ____________, ИНН 526317984689, "
                                "именуемая в дальнейшем «Арендатор»"));
    EXPECT_THAT(html,
                Not(HasSubstr("Р/с: ____________<br>Банк: ____________<br>БИК: ____________<br>Тел.: ____________<br><div class=\"sign\">____")))
        << "у физлица без счёта строки банка не печатаются";
}

/**
 * @brief Пачка: каждый договор отдельным листом, заголовок с числом договоров; пустая пачка - отказ.
 */
TEST(ContractPrint, Batch) {
    auto second = sample(ip());
    second.number = "2027-02-19-500100732259";
    second.start = "2027-02-19";
    auto const html = render({sample(ip()), second});
    std::size_t sheets = 0;
    for (auto pos = html.find("<div class=\"sheet\">"); pos != std::string::npos; pos = html.find("<div class=\"sheet\">", pos + 1)) {
        ++sheets;
    }
    EXPECT_EQ(sheets, 2U);
    EXPECT_THAT(html, HasSubstr("<title>Договоры аренды: 2</title>"));
    EXPECT_THAT(html, HasSubstr("№ 2027-02-19-500100732259"));
    EXPECT_THROW_REASON((void)render({}), UserError, "не выбрано ни одного договора");
}
