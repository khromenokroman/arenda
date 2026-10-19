#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "card_store.hpp"
#include "db.hpp"
#include "fields.hpp"

/// Границы этажа: подвальные этажи отрицательные.
constexpr int MIN_FLOOR = -5;
constexpr int MAX_FLOOR = 200;

/// Арендатор: поле kind (вид) и поля по описанию этого вида из Tenants::kinds().
using Tenant = Card;

/**
 * @brief Вид арендатора и его поля.
 */
struct TenantKind {
    std::string_view code;  ///< значение поля kind: ip, org, person
    std::string_view label; ///< название вида
    std::span<FieldSpec const> specs;
};

/**
 * @brief Справочник арендаторов: индивидуальные предприниматели, организации, физические лица.
 *
 * Все виды лежат в одной таблице; колонки чужого вида пустые. Методы не захватывают блокировку базы.
 */
class Tenants {
   public:
    explicit Tenants(Database &db);

    /// Виды арендаторов в порядке выбора в форме.
    [[nodiscard]] static std::span<TenantKind const> kinds();

    /// @throws UserError неизвестный вид
    [[nodiscard]] static TenantKind const &kind(std::string_view code);

    /// Название для списков: краткое наименование организации, «ИП ФИО», ФИО физлица.
    [[nodiscard]] static std::string title(Tenant const &tenant);

    /// По названию.
    [[nodiscard]] std::vector<Tenant> list() const;
    /// @throws NotFoundError
    [[nodiscard]] Tenant get(std::int64_t id) const;
    /**
     * @brief Создаёт арендатора из поля kind и полей этого вида.
     * @throws UserError неизвестный вид, поле чужого вида, неверные значения
     */
    std::int64_t create(Fields const &fields);
    /**
     * @brief Меняет переданные поля, остальные остаются прежними.
     *
     * При смене вида поля, которых у нового вида нет, очищаются.
     * @throws UserError, NotFoundError
     */
    void update(std::int64_t id, Fields const &changes);
    /// @throws NotFoundError; UserError если у арендатора есть договоры
    void remove(std::int64_t id);

   private:
    /// @throws UserError если ИНН занят другим арендатором (по ИНН строится номер договора)
    void check_unique_inn(std::string const &inn, std::int64_t id) const;

    /// Проверенные поля вида kind вместе с самим kind.
    [[nodiscard]] static Fields clean(std::string const &kind, Fields const &fields);

    CardStore m_store;
    Database &m_db;
};
