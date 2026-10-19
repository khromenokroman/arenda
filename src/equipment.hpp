#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "db.hpp"

/**
 * @brief Строка перечня электрооборудования (приложение 2 договора).
 */
struct EquipmentRow {
    std::string name;       ///< наименование
    std::string model;      ///< марка, модель
    std::int64_t power_c{}; ///< мощность одной единицы в сотых долях кВт
    int qty = 1;            ///< количество
};

/// Суммарная мощность в сотых долях кВт: мощность × количество по всем строкам.
[[nodiscard]] std::int64_t total_power(std::vector<EquipmentRow> const &rows);

/**
 * @brief Оборудование арендатора. Методы не захватывают блокировку базы.
 */
class Equipment {
   public:
    /// Наибольшее число строк и единиц в строке: перечень печатается в приложении на одном листе.
    static constexpr std::size_t MAX_ROWS = 50;
    static constexpr int MAX_QTY = 1000;

    explicit Equipment(Database &db);

    /// Строки по порядку. @throws NotFoundError нет арендатора
    [[nodiscard]] std::vector<EquipmentRow> list(std::int64_t tenant_id) const;

    /**
     * @brief Заменяет перечень целиком.
     * @throws UserError пустое наименование, неверные мощность или количество, слишком много строк; NotFoundError
     */
    void replace(std::int64_t tenant_id, std::vector<EquipmentRow> const &rows);

   private:
    Database &m_db;
};
