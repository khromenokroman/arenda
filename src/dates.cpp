#include "dates.hpp"

#include <chrono>
#include <cstddef>

#include "errors.hpp"

namespace {

/// Число из цифр text[pos, pos + count); -1, если там не только цифры.
int number(std::string const &text, std::size_t pos, std::size_t count) {
    int out = 0;
    for (std::size_t i = pos; i < pos + count; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return -1;
        }
        out = (out * 10) + (text[i] - '0');
    }
    return out;
}

} // namespace

void check_date(std::string const &date) {
    constexpr std::size_t SIZE = 10;
    if (date.size() != SIZE || date[4] != '-' || date[7] != '-') {
        throw UserError("дата должна быть в формате ГГГГ-ММ-ДД: «" + date + "»");
    }
    auto const year = number(date, 0, 4);
    auto const month = number(date, 5, 2);
    auto const day = number(date, 8, 2);
    if (year < 0 || month < 0 || day < 0) {
        throw UserError("дата должна быть в формате ГГГГ-ММ-ДД: «" + date + "»");
    }
    std::chrono::year_month_day const ymd{std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)},
                                          std::chrono::day{static_cast<unsigned>(day)}};
    if (!ymd.ok() || year < 1900) {
        throw UserError("нет такой даты: «" + date + "»");
    }
}
