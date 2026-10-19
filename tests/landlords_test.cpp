#include "landlords.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <set>
#include <string>

#include "db.hpp"
#include "errors.hpp"
#include "schema.hpp"
#include "test_util.hpp"

namespace {

/// Арендодатель с верными реквизитами.
Fields sample() {
    return {
        {"name_full", "Общество с ограниченной ответственностью «Ромашка»"},
        {"name_short", "ООО «Ромашка»"},
        {"ogrn", "1027700132195"},
        {"inn", "7707083893"},
        {"kpp", "773601001"},
        {"signer_name", "Иванов Иван Иванович"},
        {"account", "40702810538000123456"},
        {"bik", "044525225"},
    };
}

class LandlordsTest : public testing::Test {
   protected:
    LandlordsTest() { migrate(m_db); }

    Database m_db{":memory:"};
    Landlords m_landlords{m_db};
};

} // namespace

/**
 * @brief Каждому полю из описания соответствует колонка таблицы, и лишних колонок нет.
 */
TEST_F(LandlordsTest, SpecsMatchTable) {
    std::set<std::string> columns;
    auto stmt = m_db.prepare("SELECT name FROM pragma_table_info('landlords') WHERE name <> 'id'");
    while (stmt.step()) {
        columns.insert(stmt.column_text(0));
    }
    std::set<std::string> keys;
    for (auto const &spec : Landlords::specs()) {
        keys.emplace(spec.key);
    }
    EXPECT_EQ(keys, columns) << "описание полей в landlords.cpp и миграция в schema.cpp разошлись";
}

/**
 * @brief Созданный арендодатель читается с теми же полями, незаданные поля пустые, пробелы по краям убраны.
 */
TEST_F(LandlordsTest, CreateAndGet) {
    auto fields = sample();
    fields["phone"] = "  +7 900 000-00-00 ";
    auto const id = m_landlords.create(fields);
    auto const got = m_landlords.get(id);
    EXPECT_EQ(got.id, id);
    EXPECT_EQ(got.fields.at("name_short"), "ООО «Ромашка»");
    EXPECT_EQ(got.fields.at("phone"), "+7 900 000-00-00") << "пробелы по краям должны убираться";
    EXPECT_EQ(got.fields.at("city"), "") << "незаданное поле";
    EXPECT_EQ(got.fields.size(), Landlords::specs().size()) << "в карточке все поля из описания";
}

/**
 * @brief Список упорядочен по краткому наименованию.
 */
TEST_F(LandlordsTest, ListSortedByShortName) {
    auto b = sample();
    b["name_short"] = "ООО «Берёза»";
    auto a = sample();
    a["name_short"] = "АО «Астра»";
    m_landlords.create(b);
    m_landlords.create(a);
    auto const list = m_landlords.list();
    ASSERT_EQ(list.size(), 2U);
    EXPECT_EQ(list[0].fields.at("name_short"), "АО «Астра»");
    EXPECT_EQ(list[1].fields.at("name_short"), "ООО «Берёза»");
}

/**
 * @brief Изменение трогает только переданные поля.
 */
TEST_F(LandlordsTest, UpdateChangesOnlyGivenFields) {
    auto const id = m_landlords.create(sample());
    m_landlords.update(id, {{"city", "г. Химки"}, {"inn", ""}});
    auto const got = m_landlords.get(id);
    EXPECT_EQ(got.fields.at("city"), "г. Химки");
    EXPECT_EQ(got.fields.at("inn"), "") << "ИНН можно очистить";
    EXPECT_EQ(got.fields.at("ogrn"), "1027700132195") << "непереданное поле не меняется";
}

/**
 * @brief Отказы: обязательные поля, неизвестное поле, неверные реквизиты, дубликат, длина.
 */
TEST_F(LandlordsTest, Rejects) {
    auto fields = sample();
    fields["name_full"] = "  ";
    EXPECT_THROW_REASON(m_landlords.create(fields), UserError, "не заполнено поле «Полное наименование»");

    fields = sample();
    fields["color"] = "красный";
    EXPECT_THROW_REASON(m_landlords.create(fields), UserError, "неизвестное поле: color");

    fields = sample();
    fields["inn"] = "7707083894";
    EXPECT_THROW_REASON(m_landlords.create(fields), UserError, "ИНН «7707083894» с ошибкой");

    // арендодатель - организация, ИНН из 12 цифр не подходит
    fields = sample();
    fields["inn"] = "500100732259";
    EXPECT_THROW_REASON(m_landlords.create(fields), UserError, "ИНН организации должен состоять из 10 цифр");

    fields = sample();
    fields["bik"] = "044525226";
    EXPECT_THROW_REASON(m_landlords.create(fields), UserError, "не подходит к БИК");

    fields = sample();
    fields["name_full"] = std::string(1001, 'x');
    EXPECT_THROW_REASON(m_landlords.create(fields), UserError, "«Полное наименование» длиннее 1000 байт");

    auto const id = m_landlords.create(sample());
    EXPECT_THROW_REASON(m_landlords.create(sample()), UserError, "арендодатель «ООО «Ромашка»» уже есть");
    EXPECT_NO_THROW(m_landlords.update(id, {{"phone", "1"}})) << "своё же наименование не дубликат";
    EXPECT_TRUE(m_landlords.list().size() == 1) << "отклонённые не должны сохраниться";
}

/**
 * @brief Удаление; несуществующий id даёт NotFoundError.
 */
TEST_F(LandlordsTest, RemoveAndNotFound) {
    auto const id = m_landlords.create(sample());
    m_landlords.remove(id);
    EXPECT_TRUE(m_landlords.list().empty());
    EXPECT_THROW_REASON((void)m_landlords.get(id), NotFoundError, "нет арендодателя с id");
    EXPECT_THROW_REASON(m_landlords.update(id, {}), NotFoundError, "нет арендодателя");
    EXPECT_THROW_REASON(m_landlords.remove(id), NotFoundError, "нет арендодателя");
}

/**
 * @brief Условия договоров: новая карточка получает значения по умолчанию, неверные отклоняются.
 */
TEST_F(LandlordsTest, Terms) {
    auto const id = m_landlords.create(sample());
    auto const got = m_landlords.get(id).fields;
    EXPECT_EQ(got.at("fire_fine"), "5 000");
    EXPECT_EQ(got.at("contract_limit"), "100 000");
    EXPECT_EQ(got.at("lease_months"), "11");
    EXPECT_EQ(got.at("days_off"), "1 и 2 января, Пасха");

    EXPECT_THROW_REASON(m_landlords.update(id, {{"lease_months", "12"}}), UserError, "срок аренды: нужно число от 1 до 11");
    EXPECT_THROW_REASON(m_landlords.update(id, {{"contract_limit", "0"}}), UserError, "сумма должна быть больше нуля");
    EXPECT_THROW_REASON(m_landlords.update(id, {{"fire_fine", "пять тысяч"}}), UserError, "нужно число");
    EXPECT_THROW_REASON(m_landlords.update(id, {{"contract_limit", ""}}), UserError, "не заполнено поле «Наибольшая сумма одного договора, руб.»");
    m_landlords.update(id, {{"contract_limit", "150000,50"}});
    EXPECT_EQ(m_landlords.get(id).fields.at("contract_limit"), "150000,50");
}

/**
 * @brief Пени и штрафы п. 5.1: значения из образца по умолчанию, проценты от 0 до 100.
 */
TEST_F(LandlordsTest, Fines) {
    auto const id = m_landlords.create(sample());
    auto const got = m_landlords.get(id).fields;
    EXPECT_EQ(got.at("penalty_rent_pct"), "0,3");
    EXPECT_EQ(got.at("penalty_return_pct"), "0,5");
    EXPECT_EQ(got.at("fine_sign"), "2 000");
    EXPECT_EQ(got.at("fine_control"), "500");
    EXPECT_EQ(got.at("fine_transfer"), "2 000");
    EXPECT_EQ(got.at("fine_waste"), "2 000");
    EXPECT_THROW_REASON(m_landlords.update(id, {{"penalty_rent_pct", "0"}}), UserError, "процент должен быть больше нуля");
    EXPECT_THROW_REASON(m_landlords.update(id, {{"penalty_rent_pct", "150"}}), UserError, "не больше 100");
    EXPECT_THROW_REASON(m_landlords.update(id, {{"fine_control", "-1"}}), UserError, "нужно число");
    EXPECT_NO_THROW(m_landlords.update(id, {{"penalty_rent_pct", "0,25"}, {"fine_control", "1000"}}));
}
