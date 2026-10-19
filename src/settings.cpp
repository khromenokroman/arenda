#include "settings.hpp"

Settings::Settings(Database &db) : m_db{db} {}

std::string Settings::get(std::string const &key, std::string const &fallback) const {
    auto stmt = m_db.prepare("SELECT value FROM settings WHERE key = ?");
    stmt.bind(1, key);
    return stmt.step() ? stmt.column_text(0) : fallback;
}

void Settings::set(std::string const &key, std::string const &value) {
    auto stmt = m_db.prepare("INSERT INTO settings (key, value) VALUES (?, ?) ON CONFLICT (key) DO UPDATE SET value = excluded.value");
    stmt.bind(1, key).bind(2, value);
    stmt.step();
}

void Settings::remove(std::string const &key) {
    auto stmt = m_db.prepare("DELETE FROM settings WHERE key = ?");
    stmt.bind(1, key);
    stmt.step();
}
