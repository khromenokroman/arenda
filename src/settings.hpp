#pragma once
#include <string>

#include "db.hpp"

/**
 * @brief Настройки программы ключ - значение в таблице settings. Методы не захватывают блокировку базы.
 */
class Settings {
   public:
    explicit Settings(Database &db);

    /// Значение или fallback, если настройки нет.
    [[nodiscard]] std::string get(std::string const &key, std::string const &fallback = {}) const;

    void set(std::string const &key, std::string const &value);

    /// Удаляет настройку; нет такой - ничего не делает.
    void remove(std::string const &key);

   private:
    Database &m_db;
};
