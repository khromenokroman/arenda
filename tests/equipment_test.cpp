#include "equipment.hpp"

#include <gtest/gtest.h>

#include "db.hpp"
#include "errors.hpp"
#include "schema.hpp"
#include "tenants.hpp"
#include "test_util.hpp"

namespace {

class EquipmentTest : public testing::Test {
   protected:
    EquipmentTest() {
        migrate(m_db);
        m_tenant = Tenants{m_db}.create({{"kind", "ip"}, {"name", "Петров Пётр Петрович"}, {"gender", "m"}, {"inn", "500100732259"}});
    }

    Database m_db{":memory:"};
    Equipment m_equipment{m_db};
    std::int64_t m_tenant = 0;
};

} // namespace

/**
 * @brief Перечень сохраняется по порядку, пробелы по краям убираются; замена удаляет прежние строки.
 */
TEST_F(EquipmentTest, ReplaceAndList) {
    m_equipment.replace(m_tenant, {{" Станок для ключей ", "Silca Bravo", 55, 1}, {"Точило", "", 25, 2}});
    auto rows = m_equipment.list(m_tenant);
    ASSERT_EQ(rows.size(), 2U);
    EXPECT_EQ(rows[0].name, "Станок для ключей");
    EXPECT_EQ(rows[0].model, "Silca Bravo");
    EXPECT_EQ(rows[1].qty, 2);
    EXPECT_EQ(total_power(rows), 55 + (25 * 2)) << "суммарная мощность с учётом количества";

    m_equipment.replace(m_tenant, {{"Лампа", "", 10, 1}});
    rows = m_equipment.list(m_tenant);
    ASSERT_EQ(rows.size(), 1U) << "прежние строки должны удалиться";
    EXPECT_EQ(rows[0].name, "Лампа");

    m_equipment.replace(m_tenant, {});
    EXPECT_TRUE(m_equipment.list(m_tenant).empty()) << "пустой перечень: «не используется» в приложении";
}

/**
 * @brief Отказы называют строку и причину; отказ не меняет сохранённый перечень.
 */
TEST_F(EquipmentTest, Rejects) {
    m_equipment.replace(m_tenant, {{"Лампа", "", 10, 1}});
    EXPECT_THROW_REASON(m_equipment.replace(m_tenant, {{"Лампа", "", 10, 1}, {"  ", "", 10, 1}}), UserError,
                        "оборудование, строка 2: не заполнено наименование");
    EXPECT_THROW_REASON(m_equipment.replace(m_tenant, {{"Лампа", "", 0, 1}}), UserError, "строка 1: мощность должна быть больше нуля");
    EXPECT_THROW_REASON(m_equipment.replace(m_tenant, {{"Лампа", "", 10, 0}}), UserError, "количество от 1 до 1000");
    std::vector<EquipmentRow> const many(Equipment::MAX_ROWS + 1, EquipmentRow{"Лампа", "", 10, 1});
    EXPECT_THROW_REASON(m_equipment.replace(m_tenant, many), UserError, "больше 50 строк");
    EXPECT_EQ(m_equipment.list(m_tenant).size(), 1U) << "после отказа перечень прежний";
    EXPECT_THROW_REASON((void)m_equipment.list(999), NotFoundError, "нет арендатора с id 999");
}

/**
 * @brief Удаление арендатора удаляет его оборудование.
 */
TEST_F(EquipmentTest, DeletedWithTenant) {
    m_equipment.replace(m_tenant, {{"Лампа", "", 10, 1}});
    Tenants{m_db}.remove(m_tenant);
    auto stmt = m_db.prepare("SELECT count(*) FROM tenant_equipment");
    stmt.step();
    EXPECT_EQ(stmt.column_int(0), 0);
}
