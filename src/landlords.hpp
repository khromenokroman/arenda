#pragma once
#include <cstdint>
#include <span>
#include <vector>

#include "db.hpp"
#include "fields.hpp"

/**
 * @brief Арендодатель: организация с подписантом, банковскими реквизитами и сдаваемым объектом.
 */
struct Landlord {
    Fields fields; ///< значения по описанию Landlords::specs()
    std::int64_t id{};
};

/**
 * @brief Справочник арендодателей. Краткое наименование уникально. Методы не захватывают блокировку базы.
 */
class Landlords {
   public:
    explicit Landlords(Database &db);

    /// Описание полей карточки в порядке формы.
    [[nodiscard]] static std::span<FieldSpec const> specs();

    /// По краткому наименованию.
    [[nodiscard]] std::vector<Landlord> list() const;
    /// @throws NotFoundError
    [[nodiscard]] Landlord get(std::int64_t id) const;
    /// @throws UserError неверные поля или занятое краткое наименование
    std::int64_t create(Fields const &fields);
    /**
     * @brief Меняет переданные поля, остальные остаются прежними.
     * @throws UserError, NotFoundError
     */
    void update(std::int64_t id, Fields const &changes);
    /// @throws NotFoundError
    void remove(std::int64_t id);

   private:
    /// Проверенные поля; id - собственный id при изменении (для проверки дубликата), 0 при создании.
    [[nodiscard]] Fields clean(Fields const &fields, std::int64_t id) const;

    Database &m_db;
};
