#pragma once
#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Часть срока аренды: отдельный договор.
 */
struct LeasePart {
    std::string start; ///< первый день, ГГГГ-ММ-ДД
    std::string end;   ///< последний день включительно
    int months{};      ///< длительность в месяцах
};

/**
 * @brief Делит срок аренды на договоры так, чтобы плата за срок каждого не превышала limit.
 *
 * В договор помещается min(total_months, limit / rent) месяцев; число договоров - наименьшее при таком
 * ограничении, месяцы распределены между ними поровну (длинные части первыми). Если плата за месяц больше
 * limit, каждый договор на месяц. Границы считаются от start (не от предыдущей части), поэтому начало
 * 31-го числа не съезжает на 28-е после февраля.
 *
 * @param rent_kop плата в месяц в копейках, больше нуля
 * @param limit_kop наибольшая сумма договора в копейках, больше нуля
 * @throws UserError неверная дата, срок, плата или порог
 */
[[nodiscard]] std::vector<LeasePart> split_lease(std::string const &start, int total_months, std::int64_t rent_kop, std::int64_t limit_kop);
