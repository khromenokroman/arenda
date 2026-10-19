#include "schema.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace {

/// Миграции схемы. Применённую миграцию не менять: изменения только новой миграцией в конце.
constexpr std::array<std::string_view, 3> MIGRATIONS = {
    // 1: настройки ключ - значение
    R"sql(
CREATE TABLE settings (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
)sql",
    // 2: арендодатели; поля описаны в Landlords::specs()
    R"sql(
CREATE TABLE landlords (
    id               INTEGER PRIMARY KEY,
    name_full        TEXT NOT NULL,
    name_short       TEXT NOT NULL UNIQUE,
    ogrn             TEXT NOT NULL DEFAULT '',
    inn              TEXT NOT NULL DEFAULT '',
    kpp              TEXT NOT NULL DEFAULT '',
    address          TEXT NOT NULL DEFAULT '',
    phone            TEXT NOT NULL DEFAULT '',
    signer_post      TEXT NOT NULL DEFAULT '',
    signer_post_gen  TEXT NOT NULL DEFAULT '',
    signer_name      TEXT NOT NULL DEFAULT '',
    signer_name_gen  TEXT NOT NULL DEFAULT '',
    signer_basis_gen TEXT NOT NULL DEFAULT '',
    account          TEXT NOT NULL DEFAULT '',
    bank             TEXT NOT NULL DEFAULT '',
    bik              TEXT NOT NULL DEFAULT '',
    object_name_gen  TEXT NOT NULL DEFAULT '',
    object_address   TEXT NOT NULL DEFAULT '',
    city             TEXT NOT NULL DEFAULT '',
    region_gen       TEXT NOT NULL DEFAULT '',
    municipality_gen TEXT NOT NULL DEFAULT ''
);
)sql",
    // 3: арендаторы трёх видов в одной таблице; поля описаны в Tenants::kinds()
    R"sql(
CREATE TABLE tenants (
    id               INTEGER PRIMARY KEY,
    kind             TEXT NOT NULL CHECK (kind IN ('ip', 'org', 'person')),
    name             TEXT NOT NULL DEFAULT '',
    gender           TEXT NOT NULL DEFAULT '' CHECK (gender IN ('', 'm', 'f')),
    ogrnip           TEXT NOT NULL DEFAULT '',
    inn              TEXT NOT NULL DEFAULT '',
    address          TEXT NOT NULL DEFAULT '',
    phone            TEXT NOT NULL DEFAULT '',
    account          TEXT NOT NULL DEFAULT '',
    bank             TEXT NOT NULL DEFAULT '',
    bik              TEXT NOT NULL DEFAULT '',
    name_full        TEXT NOT NULL DEFAULT '',
    name_short       TEXT NOT NULL DEFAULT '',
    ogrn             TEXT NOT NULL DEFAULT '',
    kpp              TEXT NOT NULL DEFAULT '',
    signer_post      TEXT NOT NULL DEFAULT '',
    signer_post_gen  TEXT NOT NULL DEFAULT '',
    signer_name      TEXT NOT NULL DEFAULT '',
    signer_name_gen  TEXT NOT NULL DEFAULT '',
    signer_basis_gen TEXT NOT NULL DEFAULT '',
    birth_date       TEXT NOT NULL DEFAULT '',
    passport         TEXT NOT NULL DEFAULT '',
    passport_issued  TEXT NOT NULL DEFAULT '',
    passport_date    TEXT NOT NULL DEFAULT '',
    passport_code    TEXT NOT NULL DEFAULT ''
);
)sql",
};

} // namespace

int schema_version() { return static_cast<int>(MIGRATIONS.size()); }

int current_schema_version(Database &db) {
    auto stmt = db.prepare("PRAGMA user_version");
    stmt.step();
    return static_cast<int>(stmt.column_int(0));
}

int migrate(Database &db, int target) {
    auto const current = current_schema_version(db);
    if (current > schema_version()) {
        throw DbError("база создана более новой версией программы: схема " + std::to_string(current) + ", поддерживается " +
                      std::to_string(schema_version()));
    }
    target = std::min(target, schema_version());
    if (current >= target) {
        return current;
    }
    // процедура изменения схемы SQLite: внешние ключи выключаются вне транзакции, чтобы пересборка
    // таблицы (DROP + RENAME) не запускала каскадное удаление; целостность проверяется перед COMMIT
    db.exec("PRAGMA foreign_keys = OFF");
    try {
        for (auto v = current; v < target; ++v) {
            Transaction tx{db};
            db.exec(MIGRATIONS.at(static_cast<std::size_t>(v)));
            auto check = db.prepare("PRAGMA foreign_key_check");
            if (check.step()) {
                throw DbError("миграция " + std::to_string(v + 1) + " нарушила ссылочную целостность в таблице " + check.column_text(0));
            }
            db.exec("PRAGMA user_version = " + std::to_string(v + 1));
            tx.commit();
        }
    } catch (...) {
        db.exec("PRAGMA foreign_keys = ON");
        throw;
    }
    db.exec("PRAGMA foreign_keys = ON");
    return target;
}
