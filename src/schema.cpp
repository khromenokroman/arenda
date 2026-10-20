#include "schema.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace {

/// Миграции схемы. Применённую миграцию не менять: изменения только новой миграцией в конце.
constexpr std::array<std::string_view, 9> MIGRATIONS = {
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
    // 4: режим работы объекта и условия договоров у арендодателя; значения по умолчанию из образца договора
    R"sql(
ALTER TABLE landlords ADD COLUMN hours_weekdays TEXT NOT NULL DEFAULT 'с 09:00 до 18:00';
ALTER TABLE landlords ADD COLUMN hours_weekends TEXT NOT NULL DEFAULT 'с 09:00 до 16:00';
ALTER TABLE landlords ADD COLUMN days_off       TEXT NOT NULL DEFAULT '1 и 2 января, Пасха';
ALTER TABLE landlords ADD COLUMN fire_fine      TEXT NOT NULL DEFAULT '5 000';
ALTER TABLE landlords ADD COLUMN contract_limit TEXT NOT NULL DEFAULT '100 000';
ALTER TABLE landlords ADD COLUMN lease_months   TEXT NOT NULL DEFAULT '11';
)sql",
    // 5: арендуемое место и оборудование арендатора (приложение 2 договора)
    R"sql(
ALTER TABLE tenants ADD COLUMN area               TEXT NOT NULL DEFAULT '';
ALTER TABLE tenants ADD COLUMN trade_area         TEXT NOT NULL DEFAULT '';
ALTER TABLE tenants ADD COLUMN floor              TEXT NOT NULL DEFAULT '';
ALTER TABLE tenants ADD COLUMN purpose            TEXT NOT NULL DEFAULT '';
ALTER TABLE tenants ADD COLUMN activity_gen       TEXT NOT NULL DEFAULT '';
ALTER TABLE tenants ADD COLUMN premises_condition TEXT NOT NULL DEFAULT '';

CREATE TABLE tenant_equipment (
    id        INTEGER PRIMARY KEY,
    tenant_id INTEGER NOT NULL REFERENCES tenants (id) ON DELETE CASCADE,
    pos       INTEGER NOT NULL,
    name      TEXT    NOT NULL,
    model     TEXT    NOT NULL DEFAULT '',
    power_c   INTEGER NOT NULL CHECK (power_c >= 0),
    qty       INTEGER NOT NULL CHECK (qty > 0),
    UNIQUE (tenant_id, pos)
);
)sql",
    // 6: договоры; карточки сторон и оборудование копируются в JSON на момент создания
    R"sql(
CREATE TABLE contracts (
    id             INTEGER PRIMARY KEY,
    number         TEXT    NOT NULL UNIQUE,
    landlord_id    INTEGER NOT NULL REFERENCES landlords (id),
    tenant_id      INTEGER NOT NULL REFERENCES tenants (id),
    date_start     TEXT    NOT NULL,
    date_end       TEXT    NOT NULL,
    months         INTEGER NOT NULL CHECK (months > 0),
    rent_kop       INTEGER NOT NULL CHECK (rent_kop > 0),
    landlord_json  TEXT    NOT NULL,
    tenant_json    TEXT    NOT NULL,
    equipment_json TEXT    NOT NULL,
    created_at     TEXT    NOT NULL DEFAULT (datetime('now', 'localtime'))
);
CREATE INDEX contracts_tenant ON contracts (tenant_id);
CREATE INDEX contracts_landlord ON contracts (landlord_id);
)sql",
    // 7: пользователи (пароль PBKDF2) и сессии по хешу токена
    R"sql(
CREATE TABLE users (
    id        INTEGER PRIMARY KEY,
    login     TEXT    NOT NULL UNIQUE COLLATE NOCASE,
    name      TEXT    NOT NULL DEFAULT '',
    pass_hash TEXT    NOT NULL,
    role      TEXT    NOT NULL CHECK (role IN ('admin', 'user')),
    blocked   INTEGER NOT NULL DEFAULT 0 CHECK (blocked IN (0, 1))
);
CREATE TABLE sessions (
    token_hash TEXT PRIMARY KEY,
    user_id    INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    expires    TEXT    NOT NULL
);
)sql",
    // 8: пени и штрафы п. 5.1 в карточке арендодателя; значения по умолчанию из образца договора
    R"sql(
ALTER TABLE landlords ADD COLUMN penalty_rent_pct   TEXT NOT NULL DEFAULT '0,3';
ALTER TABLE landlords ADD COLUMN penalty_return_pct TEXT NOT NULL DEFAULT '0,5';
ALTER TABLE landlords ADD COLUMN fine_sign          TEXT NOT NULL DEFAULT '2 000';
ALTER TABLE landlords ADD COLUMN fine_control       TEXT NOT NULL DEFAULT '500';
ALTER TABLE landlords ADD COLUMN fine_transfer      TEXT NOT NULL DEFAULT '2 000';
ALTER TABLE landlords ADD COLUMN fine_waste         TEXT NOT NULL DEFAULT '2 000';
)sql",
    // 9: корреспондентский счёт банка сторон
    R"sql(
ALTER TABLE landlords ADD COLUMN corr_account TEXT NOT NULL DEFAULT '';
ALTER TABLE tenants   ADD COLUMN corr_account TEXT NOT NULL DEFAULT '';
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
