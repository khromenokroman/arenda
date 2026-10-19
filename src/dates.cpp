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

namespace {

std::chrono::year_month_day parse_ymd(std::string const &date) {
    check_date(date);
    return std::chrono::year_month_day{std::chrono::year{number(date, 0, 4)}, std::chrono::month{static_cast<unsigned>(number(date, 5, 2))},
                                       std::chrono::day{static_cast<unsigned>(number(date, 8, 2))}};
}

std::string format_ymd(std::chrono::year_month_day const &ymd) {
    auto const two = [](unsigned v) { return std::string(v < 10 ? "0" : "") + std::to_string(v); };
    return std::to_string(static_cast<int>(ymd.year())) + "-" + two(static_cast<unsigned>(ymd.month())) + "-" + two(static_cast<unsigned>(ymd.day()));
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

std::string add_months(std::string const &date, int months) {
    auto ymd = parse_ymd(date) + std::chrono::months{months};
    if (!ymd.ok()) {
        ymd = std::chrono::year_month_day_last{ymd.year(), std::chrono::month_day_last{ymd.month()}};
    }
    return format_ymd(ymd);
}

std::string add_days(std::string const &date, int days) {
    return format_ymd(std::chrono::year_month_day{std::chrono::sys_days{parse_ymd(date)} + std::chrono::days{days}});
}
