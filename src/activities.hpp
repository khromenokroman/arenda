#pragma once
#include <cstdint>
#include <span>
#include <vector>

#include "card_store.hpp"
#include "db.hpp"
#include "fields.hpp"

/// Наибольшая длина длинных текстов договора в байтах UTF-8.
constexpr std::size_t ACTIVITY_LONG_TEXT = 1000;

/// Назначение помещения: договор, п. 1.3 «для ...».
inline constexpr FieldSpec PURPOSE_SPEC{.key = "purpose",
                                        .label = "Назначение помещения (п. 1.3: «для ...»)",
                                        .group = "Арендуемое место",
                                        .hint = "оказания услуг по металлоремонту (изготовление ключей, ремонт мелких металлических изделий)",
                                        .max_size = ACTIVITY_LONG_TEXT};
/// Вид деятельности коротко: договор, п. 2.1.2 «необходимые для ...».
inline constexpr FieldSpec ACTIVITY_SPEC{.key = "activity_gen",
                                         .label = "Вид деятельности коротко (п. 2.1.2: «необходимые для ...»)",
                                         .group = "Арендуемое место",
                                         .hint = "металлоремонта"};
/// Состояние помещения при передаче: акт приёма-передачи.
inline constexpr FieldSpec CONDITION_SPEC{.key = "premises_condition",
                                          .label = "Состояние помещения при передаче",
                                          .group = "Арендуемое место",
                                          .hint = "удовлетворительное, пригодно для использования по назначению",
                                          .max_size = ACTIVITY_LONG_TEXT};

/// Вид деятельности: название и тексты договора, которые копируются в карточку арендатора.
using Activity = Card;

/**
 * @brief Справочник видов деятельности: типовые тексты назначения, вида деятельности и состояния помещения.
 *
 * Карточка арендатора хранит копию текстов, поэтому правка и удаление вида деятельности арендаторов и договоры
 * не меняют. Название уникально. Методы не захватывают блокировку базы.
 */
class Activities {
   public:
    explicit Activities(Database &db);

    /// Описание полей карточки в порядке формы.
    [[nodiscard]] static std::span<FieldSpec const> specs();

    /// По названию.
    [[nodiscard]] std::vector<Activity> list() const;
    /// @throws NotFoundError
    [[nodiscard]] Activity get(std::int64_t id) const;
    /// @throws UserError неверные поля или занятое название
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

    CardStore m_store;
    Database &m_db;
};
