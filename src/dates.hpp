#pragma once
#include <string>

/**
 * @brief Проверяет дату в формате ГГГГ-ММ-ДД (как в input type=date), с учётом длины месяца и високосных лет.
 * @throws UserError неверный формат или несуществующая дата
 */
void check_date(std::string const &date);

/**
 * @brief Дата через months месяцев; если такого числа в месяце нет - последний день месяца.
 *
 * «2026-01-31» + 1 = «2026-02-28». @throws UserError неверная дата
 */
[[nodiscard]] std::string add_months(std::string const &date, int months);

/// Дата через days дней (отрицательное - назад). @throws UserError неверная дата
[[nodiscard]] std::string add_days(std::string const &date, int days);
