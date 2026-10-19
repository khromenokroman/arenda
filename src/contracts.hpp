#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "db.hpp"
#include "equipment.hpp"
#include "fields.hpp"

/**
 * @brief Строка документа «Создание договоров»: арендатор, начало аренды, плата в месяц.
 */
struct ContractRequest {
    std::string start; ///< ГГГГ-ММ-ДД
    std::string rent;  ///< плата в месяц в рублях текстом: «20 000» или «9090,91»
    std::int64_t tenant_id{};
};

/**
 * @brief Договор аренды: часть срока со своим номером и копией карточек сторон на момент создания.
 */
struct Contract {
    std::string number; ///< «дата начала-ИНН арендатора»
    std::string start;
    std::string end;
    Fields landlord;                     ///< копия карточки арендодателя
    Fields tenant;                       ///< копия карточки арендатора (с kind)
    std::vector<EquipmentRow> equipment; ///< копия оборудования арендатора
    std::int64_t id{};
    std::int64_t landlord_id{};
    std::int64_t tenant_id{};
    std::int64_t rent_kop{}; ///< плата в месяц в копейках
    int months{};
};

/**
 * @brief Договоры. Методы не захватывают блокировку базы.
 */
class Contracts {
   public:
    explicit Contracts(Database &db);

    /**
     * @brief Договоры, которые создаст create, без записи: деление срока по условиям арендодателя.
     * @throws UserError пустой список, неверная строка (с номером строки и арендатором), номер уже занят; NotFoundError
     */
    [[nodiscard]] std::vector<Contract> plan(std::int64_t landlord_id, std::vector<ContractRequest> const &rows) const;

    /// Создаёт договоры по plan одной транзакцией. @return id в порядке plan
    std::vector<std::int64_t> create(std::int64_t landlord_id, std::vector<ContractRequest> const &rows);

    /// Все договоры: новые по дате начала первыми.
    [[nodiscard]] std::vector<Contract> list() const;
    /// @throws NotFoundError
    [[nodiscard]] Contract get(std::int64_t id) const;
    /// Обновляет копии карточек сторон и оборудования из справочников (номер и сроки не меняются). @throws NotFoundError
    void refresh(std::int64_t id);
    /// @throws NotFoundError
    void remove(std::int64_t id);

    /// Число договоров арендодателя или арендатора: удалять сторону с договорами нельзя.
    [[nodiscard]] static std::int64_t count_for(Database &db, char const *column, std::int64_t id);

   private:
    Database &m_db;
};
