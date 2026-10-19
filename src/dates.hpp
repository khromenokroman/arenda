#pragma once
#include <string>

/**
 * @brief Проверяет дату в формате ГГГГ-ММ-ДД (как в input type=date), с учётом длины месяца и високосных лет.
 * @throws UserError неверный формат или несуществующая дата
 */
void check_date(std::string const &date);
