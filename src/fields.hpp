#pragma once
#include <cstddef>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// Значения полей карточки по ключу поля.
using Fields = std::map<std::string, std::string, std::less<>>;

/**
 * @brief Описание поля карточки справочника: по нему строятся колонки SQL, JSON, проверки и веб-форма.
 */
struct FieldSpec {
    std::string_view key;                         ///< имя колонки в базе и поля в JSON
    std::string_view label;                       ///< подпись в форме
    std::string_view group;                       ///< раздел формы
    std::string_view hint;                        ///< пример значения в пустом поле
    void (*check)(std::string const &) = nullptr; ///< проверка непустого значения; бросает UserError
    std::size_t max_size = 300;                   ///< наибольшая длина в байтах UTF-8
    bool required = false;                        ///< пустое значение запрещено
};

/**
 * @brief Значения полей по описанию: пробелы по краям убраны, отсутствующие поля пустые.
 *
 * @throws UserError неизвестное поле, пустое обязательное, слишком длинное или не прошедшее проверку
 */
[[nodiscard]] Fields clean_fields(std::span<FieldSpec const> specs, Fields const &values);

/// Ключи полей в порядке описания.
[[nodiscard]] std::vector<std::string> keys(std::span<FieldSpec const> specs);

/// Убирает пробельные символы по краям.
[[nodiscard]] std::string trim(std::string_view text);
