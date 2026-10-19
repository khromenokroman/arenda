#include "contracts.hpp"

#include <nlohmann/json.hpp>
#include <set>

#include "dates.hpp"
#include "errors.hpp"
#include "landlords.hpp"
#include "lease_split.hpp"
#include "numbers.hpp"
#include "tenants.hpp"

namespace {

/// Наибольшее число строк в документе создания: защита от случайной вставки огромного списка.
constexpr std::size_t MAX_ROWS = 500;

constexpr char const SELECT[] =
    "SELECT id, number, landlord_id, tenant_id, date_start, date_end, months, rent_kop, landlord_json, tenant_json, equipment_json "
    "FROM contracts";

std::string fields_json(Fields const &fields) { return nlohmann::json(fields).dump(); }

Fields fields_from(std::string const &text) {
    Fields out;
    // items() ссылается на объект: временный из parse уничтожился бы до цикла
    auto const j = nlohmann::json::parse(text);
    for (auto const &[key, value] : j.items()) {
        out.emplace(key, value.get<std::string>());
    }
    return out;
}

std::string equipment_json(std::vector<EquipmentRow> const &rows) {
    auto out = nlohmann::json::array();
    for (auto const &row : rows) {
        out.push_back({{"name", row.name}, {"model", row.model}, {"power_c", row.power_c}, {"qty", row.qty}});
    }
    return out.dump();
}

std::vector<EquipmentRow> equipment_from(std::string const &text) {
    std::vector<EquipmentRow> out;
    auto const j = nlohmann::json::parse(text);
    for (auto const &item : j) {
        out.push_back({item.at("name").get<std::string>(), item.at("model").get<std::string>(), item.at("power_c").get<std::int64_t>(),
                       item.at("qty").get<int>()});
    }
    return out;
}

Contract read(Statement const &stmt) {
    Contract c;
    c.id = stmt.column_int(0);
    c.number = stmt.column_text(1);
    c.landlord_id = stmt.column_int(2);
    c.tenant_id = stmt.column_int(3);
    c.start = stmt.column_text(4);
    c.end = stmt.column_text(5);
    c.months = static_cast<int>(stmt.column_int(6));
    c.rent_kop = stmt.column_int(7);
    c.landlord = fields_from(stmt.column_text(8));
    c.tenant = fields_from(stmt.column_text(9));
    c.equipment = equipment_from(stmt.column_text(10));
    return c;
}

bool number_taken(Database &db, std::string const &number) {
    auto stmt = db.prepare("SELECT 1 FROM contracts WHERE number = ?");
    stmt.bind(1, number);
    return stmt.step();
}

} // namespace

Contracts::Contracts(Database &db) : m_db{db} {}

std::int64_t Contracts::count_for(Database &db, char const *column, std::int64_t id) {
    auto stmt = db.prepare(std::string("SELECT count(*) FROM contracts WHERE ") + column + " = ?");
    stmt.bind(1, id);
    stmt.step();
    return stmt.column_int(0);
}

std::vector<Contract> Contracts::plan(std::int64_t landlord_id, std::vector<ContractRequest> const &rows) const {
    if (rows.empty()) {
        throw UserError("добавьте хотя бы одного арендатора");
    }
    if (rows.size() > MAX_ROWS) {
        throw UserError("в документе больше " + std::to_string(MAX_ROWS) + " строк");
    }
    auto const landlord = Landlords{m_db}.get(landlord_id);
    auto const limit = numbers::parse_hundredths(landlord.fields.at("contract_limit"), "наибольшая сумма договора у арендодателя");
    auto const total_months = numbers::parse_int(landlord.fields.at("lease_months"), "срок аренды у арендодателя", 1, MAX_LEASE_MONTHS);
    Tenants const tenants{m_db};
    Equipment const equipment{m_db};
    std::vector<Contract> out;
    std::set<std::string> numbers_in_plan;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        auto const &row = rows[i];
        std::string prefix = "строка " + std::to_string(i + 1) + ": ";
        try {
            auto const tenant = tenants.get(row.tenant_id);
            prefix = "строка " + std::to_string(i + 1) + " (" + Tenants::title(tenant) + "): ";
            auto const &inn = tenant.fields.at("inn");
            if (inn.empty()) {
                throw UserError("у арендатора не заполнен ИНН, он нужен для номера договора");
            }
            auto const rent = numbers::parse_hundredths(row.rent, "плата в месяц");
            auto const items = equipment.list(row.tenant_id);
            for (auto const &part : split_lease(row.start, total_months, rent, limit)) {
                Contract c;
                c.number = part.start + "-" + inn;
                if (!numbers_in_plan.insert(c.number).second || number_taken(m_db, c.number)) {
                    throw UserError("договор " + c.number + " уже есть: у арендатора не может быть двух договоров с одной датой начала");
                }
                c.start = part.start;
                c.end = part.end;
                c.months = part.months;
                c.rent_kop = rent;
                c.landlord_id = landlord_id;
                c.tenant_id = row.tenant_id;
                c.landlord = landlord.fields;
                c.tenant = tenant.fields;
                c.equipment = items;
                out.push_back(std::move(c));
            }
        } catch (UserError const &e) {
            throw UserError(prefix + e.what());
        } catch (NotFoundError const &e) {
            throw UserError(prefix + e.what());
        }
    }
    return out;
}

std::vector<std::int64_t> Contracts::create(std::int64_t landlord_id, std::vector<ContractRequest> const &rows) {
    Transaction tx{m_db};
    std::vector<std::int64_t> ids;
    auto insert = m_db.prepare(
        "INSERT INTO contracts (number, landlord_id, tenant_id, date_start, date_end, months, rent_kop, landlord_json, tenant_json, equipment_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    for (auto const &c : plan(landlord_id, rows)) {
        insert.bind(1, c.number)
            .bind(2, c.landlord_id)
            .bind(3, c.tenant_id)
            .bind(4, c.start)
            .bind(5, c.end)
            .bind(6, static_cast<std::int64_t>(c.months))
            .bind(7, c.rent_kop)
            .bind(8, fields_json(c.landlord))
            .bind(9, fields_json(c.tenant))
            .bind(10, equipment_json(c.equipment));
        insert.step();
        insert.reset();
        ids.push_back(m_db.last_insert_id());
    }
    tx.commit();
    return ids;
}

std::vector<Contract> Contracts::list(ContractFilter const &filter) const {
    if (!filter.from.empty()) {
        check_date(filter.from);
    }
    if (!filter.to.empty()) {
        check_date(filter.to);
    }
    if (!filter.from.empty() && !filter.to.empty() && filter.from > filter.to) {
        throw UserError("начало периода " + filter.from + " позже конца " + filter.to);
    }
    // незаданное условие отключается проверкой параметра на пустоту или NULL: текст запроса один
    auto stmt = m_db.prepare(std::string(SELECT) +
                             " WHERE (?1 = '' OR date_end >= ?1) AND (?2 = '' OR date_start <= ?2)"
                             " AND (?3 IS NULL OR landlord_id = ?3) AND (?4 IS NULL OR tenant_id = ?4)"
                             " ORDER BY date_start DESC, number");
    stmt.bind(1, filter.from).bind(2, filter.to);
    if (filter.landlord_id) {
        stmt.bind(3, *filter.landlord_id);
    } else {
        stmt.bind_null(3);
    }
    if (filter.tenant_id) {
        stmt.bind(4, *filter.tenant_id);
    } else {
        stmt.bind_null(4);
    }
    std::vector<Contract> out;
    while (stmt.step()) {
        out.push_back(read(stmt));
    }
    return out;
}

Contract Contracts::get(std::int64_t id) const {
    auto stmt = m_db.prepare(std::string(SELECT) + " WHERE id = ?");
    stmt.bind(1, id);
    if (!stmt.step()) {
        throw NotFoundError("нет договора с id " + std::to_string(id));
    }
    return read(stmt);
}

void Contracts::refresh(std::int64_t id) {
    Transaction tx{m_db};
    auto const c = get(id);
    auto stmt = m_db.prepare("UPDATE contracts SET landlord_json = ?, tenant_json = ?, equipment_json = ? WHERE id = ?");
    stmt.bind(1, fields_json(Landlords{m_db}.get(c.landlord_id).fields))
        .bind(2, fields_json(Tenants{m_db}.get(c.tenant_id).fields))
        .bind(3, equipment_json(Equipment{m_db}.list(c.tenant_id)))
        .bind(4, id);
    stmt.step();
    tx.commit();
}

void Contracts::remove(std::int64_t id) {
    Transaction tx{m_db};
    (void)get(id);
    auto stmt = m_db.prepare("DELETE FROM contracts WHERE id = ?");
    stmt.bind(1, id);
    stmt.step();
    tx.commit();
}
