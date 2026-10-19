#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "card_store.hpp"
#include "db.hpp"
#include "fields.hpp"

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
    /// @throws NotFoundError
    void remove(std::int64_t id);

   private:
    /// Проверенные поля вида kind вместе с самим kind.
    [[nodiscard]] static Fields clean(std::string const &kind, Fields const &fields);

    CardStore m_store;
    Database &m_db;
};
