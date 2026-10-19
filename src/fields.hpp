#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// Значения полей карточки по ключу поля.
using Fields = std::map<std::string, std::string, std::less<>>;

/**
 * @brief Вариант значения поля с выбором из списка.
 */
struct Choice {
    std::string_view value; ///< хранимое значение
    std::string_view label; ///< подпись в форме
};

/**
 * @brief Как вводится поле в веб-форме.
 */
enum class Input : std::uint8_t {
    TEXT,   ///< строка
    DATE,   ///< дата ГГГГ-ММ-ДД
    SELECT, ///< выбор из FieldSpec::choices
};

/**
 * @brief Описание поля карточки справочника: по нему строятся колонки SQL, JSON, проверки и веб-форма.
 */
struct FieldSpec {
    std::string_view key;                         ///< имя колонки в базе и поля в JSON
    std::string_view label;                       ///< подпись в форме
    std::string_view group;                       ///< раздел формы
    std::string_view hint;                        ///< пример значения в пустом поле
    void (*check)(std::string const &) = nullptr; ///< проверка непустого значения; бросает UserError
    std::span<Choice const> choices{};            ///< допустимые значения для Input::SELECT
    Input input = Input::TEXT;                    ///< вид поля в форме
    std::size_t max_size = 300;                   ///< наибольшая длина в байтах UTF-8
    bool required = false;                        ///< пустое значение запрещено
    std::string_view default_value{};             ///< значение нового поля, если оно не передано
};

/**
 * @brief Значения полей по описанию: пробелы по краям убраны, отсутствующие поля - значение по умолчанию.
 *
 * @throws UserError неизвестное поле, пустое обязательное, слишком длинное, не из списка или не прошедшее проверку
 */
[[nodiscard]] Fields clean_fields(std::span<FieldSpec const> specs, Fields const &values);

/// Ключи полей в порядке описания.
[[nodiscard]] std::vector<std::string> keys(std::span<FieldSpec const> specs);

/// Убирает пробельные символы по краям.
[[nodiscard]] std::string trim(std::string_view text);

/// Название вида поля для JSON: "text", "date", "select".
[[nodiscard]] std::string_view to_string(Input input);
