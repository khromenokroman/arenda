#pragma once
#include <cstdint>
#include <string>

/**
 * @brief Числа и суммы прописью для текста договора.
 */
namespace words {

/**
 * @brief Число прописью строчными буквами: 21000 -> «двадцать одна тысяча» (feminine) или «двадцать один ...».
 * @param feminine род единиц: true - «одна, две» (тысяча, копейка), false - «один, два» (рубль)
 */
[[nodiscard]] std::string number(std::int64_t value, bool feminine = false);

/// Форма слова по числу: 1 рубль, 2 рубля, 5 рублей, 11 рублей, 21 рубль.
[[nodiscard]] std::string plural(std::int64_t value, std::string const &one, std::string const &few, std::string const &many);

/**
 * @brief Сумма для договора: 2000000 копеек -> «20 000 (Двадцать тысяч) рублей 00 копеек».
 */
[[nodiscard]] std::string rubles(std::int64_t kop);

/**
 * @brief Дата ГГГГ-ММ-ДД для договора: «19» октября 2026 г. (кавычки-ёлочки вокруг числа).
 * @throws UserError неверная дата
 */
[[nodiscard]] std::string date_long(std::string const &date);

/// Дата ГГГГ-ММ-ДД как ДД.ММ.ГГГГ; пустая строка остаётся пустой. @throws UserError неверная дата
[[nodiscard]] std::string date_short(std::string const &date);

} // namespace words
