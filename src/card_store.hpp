#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "db.hpp"
#include "fields.hpp"

/**
 * @brief Карточка справочника: id и значения полей по ключу.
 */
struct Card {
    Fields fields;
    std::int64_t id{};
};

/**
 * @brief Хранение карточек справочника в таблице: колонка id и текстовые колонки полей.
 *
 * Проверок значений нет, их делает справочник до записи. Методы не захватывают блокировку базы.
 */
class CardStore {
   public:
    /**
     * @param table таблица; имя и колонки берутся только из кода, не из запроса
     * @param columns текстовые колонки полей
     * @param order_by выражение сортировки списка
     * @param not_found начало текста ошибки для несуществующего id, например «нет арендодателя»
     */
    CardStore(Database &db, std::string table, std::vector<std::string> columns, std::string order_by, std::string not_found);

    [[nodiscard]] std::vector<Card> list() const;
    /// @throws NotFoundError
    [[nodiscard]] Card get(std::int64_t id) const;
    /// Добавляет карточку; отсутствующие в fields колонки пустые. @return id новой карточки
    std::int64_t insert(Fields const &fields);
    /// Записывает все колонки; отсутствующие в fields становятся пустыми. @throws NotFoundError
    void update(std::int64_t id, Fields const &fields);
    /// @throws NotFoundError
    void remove(std::int64_t id);

   private:
    void bind_fields(Statement &stmt, Fields const &fields) const;

    std::vector<std::string> m_columns;
    std::string m_table;
    std::string m_select; ///< SELECT id и колонок без условия
    std::string m_order_by;
    std::string m_not_found;
    Database &m_db;
};
