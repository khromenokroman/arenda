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
    EXPECT_THAT(table_names(db), testing::IsSupersetOf({"settings"}));
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
