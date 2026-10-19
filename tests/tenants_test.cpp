#include "tenants.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <set>
#include <string>

#include "db.hpp"
#include "errors.hpp"
#include "schema.hpp"
#include "test_util.hpp"

namespace {

Fields ip() {
    return {{"kind", "ip"},          {"name", "Петров Пётр Петрович"}, {"gender", "m"}, {"ogrnip", "304500116000157"},
            {"inn", "500100732259"}, {"phone", "+7 900 111-22-33"}};
}

Fields org() {
    return {{"kind", "org"},
            {"name_full", "Общество с ограниченной ответственностью «Берёза»"},
            {"name_short", "ООО «Берёза»"},
            {"ogrn", "1027700132195"},
            {"inn", "7707083893"},
            {"kpp", "773601001"}};
}

Fields person() {
    return {{"kind", "person"},          {"name", "Сидорова Анна Сергеевна"}, {"gender", "f"},        {"birth_date", "1990-05-17"},
            {"passport", "4510 123456"}, {"passport_code", "770-001"},        {"inn", "526317984689"}};
}

class TenantsTest : public testing::Test {
   protected:
    TenantsTest() { migrate(m_db); }

    Database m_db{":memory:"};
    Tenants m_tenants{m_db};
};

} // namespace

/**
 * @brief Колонки таблицы: kind и ключи полей всех видов, без лишних и без пропущенных.
 */
TEST_F(TenantsTest, SpecsMatchTable) {
    std::set<std::string> columns;
    auto stmt = m_db.prepare("SELECT name FROM pragma_table_info('tenants') WHERE name <> 'id'");
    while (stmt.step()) {
        columns.insert(stmt.column_text(0));
    }
    std::set<std::string> keys{"kind"};
    for (auto const &kind : Tenants::kinds()) {
        for (auto const &spec : kind.specs) {
            keys.emplace(spec.key);
        }
    }
    EXPECT_EQ(keys, columns) << "описание полей в tenants.cpp и миграция в schema.cpp разошлись";
}

/**
 * @brief Арендаторы всех трёх видов сохраняются; название для списка зависит от вида; список по названию.
 */
TEST_F(TenantsTest, CreateEachKind) {
    auto const a = m_tenants.create(ip());
    auto const b = m_tenants.create(org());
    auto const c = m_tenants.create(person());
    EXPECT_EQ(m_tenants.get(a).fields.at("ogrnip"), "304500116000157");
    EXPECT_EQ(m_tenants.get(b).fields.at("kpp"), "773601001");
    EXPECT_EQ(m_tenants.get(c).fields.at("passport"), "4510 123456");
    EXPECT_EQ(m_tenants.get(c).fields.at("ogrnip"), "") << "поля чужого вида пустые";

    EXPECT_EQ(Tenants::title(m_tenants.get(a)), "ИП Петров Пётр Петрович");
    EXPECT_EQ(Tenants::title(m_tenants.get(b)), "ООО «Берёза»");
    EXPECT_EQ(Tenants::title(m_tenants.get(c)), "Сидорова Анна Сергеевна");

    std::vector<std::string> titles;
    for (auto const &t : m_tenants.list()) {
        titles.push_back(Tenants::title(t));
    }
    EXPECT_THAT(titles, testing::ElementsAre("ООО «Берёза»", "ИП Петров Пётр Петрович", "Сидорова Анна Сергеевна"))
        << "сортировка по наименованию организации или ФИО, без «ИП»";
}

/**
 * @brief Отказы: вид, обязательные поля, поле чужого вида, реквизиты другого вида, пол, даты, паспорт.
 */
TEST_F(TenantsTest, Rejects) {
    auto f = ip();
    f.erase("kind");
    EXPECT_THROW_REASON(m_tenants.create(f), UserError, "не указан вид арендатора");

    f = ip();
    f["kind"] = "llc";
    EXPECT_THROW_REASON(m_tenants.create(f), UserError, "неизвестный вид арендатора «llc»");

    f = ip();
    f["name"] = "";
    EXPECT_THROW_REASON(m_tenants.create(f), UserError, "Индивидуальный предприниматель: не заполнено поле «ФИО»");

    f = person();
    f.erase("gender");
    EXPECT_THROW_REASON(m_tenants.create(f), UserError, "Физическое лицо: не заполнено поле «Пол»");

    f = ip();
    f["gender"] = "x";
    EXPECT_THROW_REASON(m_tenants.create(f), UserError, "поле «Пол»: недопустимое значение «x»");

    f = ip();
    f["kpp"] = "773601001";
    EXPECT_THROW_REASON(m_tenants.create(f), UserError, "Индивидуальный предприниматель: неизвестное поле: kpp");

    f = ip();
    f["inn"] = "7707083893";
    EXPECT_THROW_REASON(m_tenants.create(f), UserError, "ИНН физического лица должен состоять из 12 цифр");

    f = org();
    f["inn"] = "500100732259";
    EXPECT_THROW_REASON(m_tenants.create(f), UserError, "ИНН организации должен состоять из 10 цифр");

    f = person();
    f["birth_date"] = "17.05.1990";
    EXPECT_THROW_REASON(m_tenants.create(f), UserError, "в формате ГГГГ-ММ-ДД: «17.05.1990»");

    f = person();
    f["passport"] = "4510123456";
    EXPECT_THROW_REASON(m_tenants.create(f), UserError, "как «1234 567890»");

    f = org();
    f["account"] = "40702810538000123457";
    f["bik"] = "044525225";
    EXPECT_THROW_REASON(m_tenants.create(f), UserError, "не подходит к БИК");

    EXPECT_TRUE(m_tenants.list().empty()) << "отклонённые не должны сохраниться";
}

/**
 * @brief Изменение без смены вида трогает только переданные поля.
 */
TEST_F(TenantsTest, UpdateKeepsOtherFields) {
    auto const id = m_tenants.create(person());
    m_tenants.update(id, {{"phone", "+7 900 000-00-00"}});
    auto const got = m_tenants.get(id).fields;
    EXPECT_EQ(got.at("phone"), "+7 900 000-00-00");
    EXPECT_EQ(got.at("passport"), "4510 123456") << "непереданное поле не меняется";
    EXPECT_EQ(got.at("kind"), "person");
}

/**
 * @brief Смена вида: поля нового вида проверяются, общие сохраняются, поля прежнего вида очищаются.
 */
TEST_F(TenantsTest, ChangeKind) {
    auto const id = m_tenants.create(person());
    EXPECT_THROW_REASON(m_tenants.update(id, {{"kind", "org"}}), UserError, "Организация: не заполнено поле «Полное наименование»");
    EXPECT_EQ(m_tenants.get(id).fields.at("kind"), "person") << "неудачная смена вида ничего не меняет";

    m_tenants.update(id, {{"kind", "ip"}, {"ogrnip", "304500116000157"}});
    auto const got = m_tenants.get(id).fields;
    EXPECT_EQ(got.at("kind"), "ip");
    EXPECT_EQ(got.at("name"), "Сидорова Анна Сергеевна") << "ФИО есть у обоих видов и сохраняется";
    EXPECT_EQ(got.at("gender"), "f");
    EXPECT_EQ(got.at("ogrnip"), "304500116000157");
    EXPECT_EQ(got.at("passport"), "") << "паспорт есть только у физлица и очищается";
    EXPECT_EQ(got.at("birth_date"), "");
}

/**
 * @brief Удаление; несуществующий id даёт NotFoundError.
 */
TEST_F(TenantsTest, RemoveAndNotFound) {
    auto const id = m_tenants.create(ip());
    m_tenants.remove(id);
    EXPECT_TRUE(m_tenants.list().empty());
    EXPECT_THROW_REASON((void)m_tenants.get(id), NotFoundError, "нет арендатора с id");
    EXPECT_THROW_REASON(m_tenants.update(id, {}), NotFoundError, "нет арендатора");
    EXPECT_THROW_REASON(m_tenants.remove(id), NotFoundError, "нет арендатора");
}

/**
 * @brief ИНН обязателен у всех видов и не повторяется: по нему строится номер договора.
 */
TEST_F(TenantsTest, InnRequiredAndUnique) {
    auto f = person();
    f.erase("inn");
    EXPECT_THROW_REASON(m_tenants.create(f), UserError, "Физическое лицо: не заполнено поле «ИНН»");
    auto const id = m_tenants.create(ip());
    f = person();
    f["inn"] = "500100732259";
    EXPECT_THROW_REASON(m_tenants.create(f), UserError, "арендатор с ИНН 500100732259 уже есть: «ИП Петров Пётр Петрович»");
    EXPECT_NO_THROW(m_tenants.update(id, {{"phone", "1"}})) << "свой ИНН не дубликат";
}

/**
 * @brief Арендуемое место: площади и этаж проверяются, текстовые поля сохраняются.
 */
TEST_F(TenantsTest, Place) {
    auto f = ip();
    f["area"] = "12,5";
    f["trade_area"] = "10";
    f["floor"] = "2";
    f["purpose"] = "оказания услуг по металлоремонту";
    f["activity_gen"] = "металлоремонта";
    auto const id = m_tenants.create(f);
    EXPECT_EQ(m_tenants.get(id).fields.at("activity_gen"), "металлоремонта");
    EXPECT_THROW_REASON(m_tenants.update(id, {{"area", "0"}}), UserError, "площадь должна быть больше нуля");
    EXPECT_THROW_REASON(m_tenants.update(id, {{"trade_area", "десять"}}), UserError, "нужно число");
    EXPECT_THROW_REASON(m_tenants.update(id, {{"floor", "2,5"}}), UserError, "этаж: нужно целое число");
    EXPECT_THROW_REASON(m_tenants.update(id, {{"floor", "300"}}), UserError, "этаж: нужно число от -5 до 200");
}
