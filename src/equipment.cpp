#include "equipment.hpp"

#include "errors.hpp"
#include "fields.hpp"

namespace {

constexpr std::size_t MAX_TEXT = 300;

void check_tenant(Database &db, std::int64_t tenant_id) {
    auto stmt = db.prepare("SELECT 1 FROM tenants WHERE id = ?");
    stmt.bind(1, tenant_id);
    if (!stmt.step()) {
        throw NotFoundError("нет арендатора с id " + std::to_string(tenant_id));
    }
}

} // namespace

std::int64_t total_power(std::vector<EquipmentRow> const &rows) {
    std::int64_t sum = 0;
    for (auto const &row : rows) {
        sum += row.power_c * row.qty;
    }
    return sum;
}

Equipment::Equipment(Database &db) : m_db{db} {}

std::vector<EquipmentRow> Equipment::list(std::int64_t tenant_id) const {
    check_tenant(m_db, tenant_id);
    auto stmt = m_db.prepare("SELECT name, model, power_c, qty FROM tenant_equipment WHERE tenant_id = ? ORDER BY pos");
    stmt.bind(1, tenant_id);
    std::vector<EquipmentRow> out;
    while (stmt.step()) {
        out.push_back({stmt.column_text(0), stmt.column_text(1), stmt.column_int(2), static_cast<int>(stmt.column_int(3))});
    }
    return out;
}

void Equipment::replace(std::int64_t tenant_id, std::vector<EquipmentRow> const &rows) {
    Transaction tx{m_db};
    check_tenant(m_db, tenant_id);
    if (rows.size() > MAX_ROWS) {
        throw UserError("в перечне оборудования больше " + std::to_string(MAX_ROWS) + " строк");
    }
    auto del = m_db.prepare("DELETE FROM tenant_equipment WHERE tenant_id = ?");
    del.bind(1, tenant_id);
    del.step();
    auto insert = m_db.prepare("INSERT INTO tenant_equipment (tenant_id, pos, name, model, power_c, qty) VALUES (?, ?, ?, ?, ?, ?)");
    for (std::size_t i = 0; i < rows.size(); ++i) {
        auto const line = "оборудование, строка " + std::to_string(i + 1) + ": ";
        auto const name = trim(rows[i].name);
        auto const model = trim(rows[i].model);
        if (name.empty()) {
            throw UserError(line + "не заполнено наименование");
        }
        if (name.size() > MAX_TEXT || model.size() > MAX_TEXT) {
            throw UserError(line + "наименование или модель длиннее " + std::to_string(MAX_TEXT) + " байт");
        }
        if (rows[i].power_c <= 0) {
            throw UserError(line + "мощность должна быть больше нуля");
        }
        if (rows[i].qty < 1 || rows[i].qty > MAX_QTY) {
            throw UserError(line + "количество от 1 до " + std::to_string(MAX_QTY));
        }
        insert.bind(1, tenant_id)
            .bind(2, static_cast<std::int64_t>(i))
            .bind(3, name)
            .bind(4, model)
            .bind(5, rows[i].power_c)
            .bind(6, static_cast<std::int64_t>(rows[i].qty));
        insert.step();
        insert.reset();
    }
    tx.commit();
}
