#pragma once
#include <cstdint>
#include <string>
#include <string_view>

/**
 * @brief Числа с двумя знаками после запятой (рубли и копейки, квадратные метры) в целых сотых долях.
 */
namespace numbers {

/**
 * @brief Разбирает «10 000», «12,5», «9090.91» в сотые доли: 1000000, 1250, 909091.
 *
 * Пробелы (в том числе неразрывные) между разрядами допустимы, разделитель - запятая или точка.
 * @param what название величины для текста ошибки
 * @throws UserError пусто, не число, больше двух знаков после запятой, отрицательное или слишком большое
 */
[[nodiscard]] std::int64_t parse_hundredths(std::string_view text, std::string_view what);

/// Сотые доли в «10 000,00» (разряды через пробел, всегда два знака после запятой).
[[nodiscard]] std::string format_hundredths(std::int64_t value);

/// Целое число от min до max. @throws UserError
[[nodiscard]] int parse_int(std::string_view text, std::string_view what, int min, int max);

} // namespace numbers
