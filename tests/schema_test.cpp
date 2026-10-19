#include "schema.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "db.hpp"
#include "test_util.hpp"

namespace {

std::vector<std::string> table_names(Database &db) {
    std::vector<std::string> names;
    auto stmt = db.prepare("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name");
    while (stmt.step()) {
        names.push_back(stmt.column_text(0));
    }
    return names;
}

} // namespace

/**
 * @brief Пустая база получает все таблицы и последнюю версию схемы.
 */
TEST(Schema, MigratesEmptyDatabase) {
    Database db{":memory:"};
    ASSERT_EQ(current_schema_version(db), 0) << "новая база должна быть пустой";
    EXPECT_EQ(migrate(db), schema_version());
    EXPECT_EQ(current_schema_version(db), schema_version()) << "user_version после миграции";
    EXPECT_THAT(table_names(db), testing::IsSupersetOf({"settings", "landlords", "tenants"}));
}

/**
 * @brief Повторная миграция ничего не меняет и не падает на уже созданных таблицах.
 */
TEST(Schema, MigrateIsIdempotent) {
    Database db{":memory:"};
    migrate(db);
    EXPECT_EQ(migrate(db), schema_version()) << "повторный запуск на актуальной базе";
}

/**
 * @brief База от более новой версии программы не открывается, чтобы не испортить её старой схемой.
 */
TEST(Schema, RejectsNewerDatabase) {
    Database db{":memory:"};
    db.exec("PRAGMA user_version = " + std::to_string(schema_version() + 1));
    EXPECT_THROW_REASON(migrate(db), DbError, "более новой версией");
}

/**
 * @brief Миграция 4 на базе с арендодателем: у существующих карточек условия договоров по умолчанию.
 */
TEST(Schema, LandlordTermsDefaultsForOldRows) {
    Database db{":memory:"};
    migrate(db, 3);
    db.exec("INSERT INTO landlords (name_full, name_short) VALUES ('ООО «Старое»', 'Старое')");
    migrate(db);
    auto stmt = db.prepare("SELECT fire_fine, contract_limit, lease_months, hours_weekdays FROM landlords");
    ASSERT_TRUE(stmt.step());
    EXPECT_EQ(stmt.column_text(0), "5 000");
    EXPECT_EQ(stmt.column_text(1), "100 000");
    EXPECT_EQ(stmt.column_text(2), "11");
    EXPECT_EQ(stmt.column_text(3), "с 09:00 до 18:00");
}
