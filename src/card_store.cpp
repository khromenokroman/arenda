#include "card_store.hpp"

#include <utility>

#include "errors.hpp"

namespace {

std::string join(std::vector<std::string> const &items, std::string const &suffix) {
    std::string out;
    for (auto const &item : items) {
        out += out.empty() ? "" : ", ";
        out += item + suffix;
    }
    return out;
}

} // namespace

CardStore::CardStore(Database &db, std::string table, std::vector<std::string> columns, std::string order_by, std::string not_found)
    : m_columns{std::move(columns)},
      m_table{std::move(table)},
      m_select{"SELECT id, " + join(m_columns, "") + " FROM " + m_table},
      m_order_by{std::move(order_by)},
      m_not_found{std::move(not_found)},
      m_db{db} {}

std::vector<Card> CardStore::list() const {
    auto stmt = m_db.prepare(m_select + " ORDER BY " + m_order_by);
    std::vector<Card> out;
    while (stmt.step()) {
        Card card;
        card.id = stmt.column_int(0);
        for (std::size_t i = 0; i < m_columns.size(); ++i) {
            card.fields.emplace(m_columns[i], stmt.column_text(static_cast<int>(i) + 1));
        }
        out.push_back(std::move(card));
    }
    return out;
}

Card CardStore::get(std::int64_t id) const {
    auto stmt = m_db.prepare(m_select + " WHERE id = ?");
    stmt.bind(1, id);
    if (!stmt.step()) {
        throw NotFoundError(m_not_found + " с id " + std::to_string(id));
    }
    Card card;
    card.id = id;
    for (std::size_t i = 0; i < m_columns.size(); ++i) {
        card.fields.emplace(m_columns[i], stmt.column_text(static_cast<int>(i) + 1));
    }
    return card;
}

std::int64_t CardStore::insert(Fields const &fields) {
    std::string params;
    for (std::size_t i = 0; i < m_columns.size(); ++i) {
        params += i == 0 ? "?" : ", ?";
    }
    auto stmt = m_db.prepare("INSERT INTO " + m_table + " (" + join(m_columns, "") + ") VALUES (" + params + ")");
    bind_fields(stmt, fields);
    stmt.step();
    return m_db.last_insert_id();
}

void CardStore::update(std::int64_t id, Fields const &fields) {
    (void)get(id);
    auto stmt = m_db.prepare("UPDATE " + m_table + " SET " + join(m_columns, " = ?") + " WHERE id = ?");
    bind_fields(stmt, fields);
    stmt.bind(static_cast<int>(m_columns.size()) + 1, id);
    stmt.step();
}

void CardStore::remove(std::int64_t id) {
    (void)get(id);
    auto stmt = m_db.prepare("DELETE FROM " + m_table + " WHERE id = ?");
    stmt.bind(1, id);
    stmt.step();
}

void CardStore::bind_fields(Statement &stmt, Fields const &fields) const {
    for (std::size_t i = 0; i < m_columns.size(); ++i) {
        auto const it = fields.find(m_columns[i]);
        stmt.bind(static_cast<int>(i) + 1, it == fields.end() ? std::string_view{} : std::string_view{it->second});
    }
}
