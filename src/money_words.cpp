#include "money_words.hpp"

#include <array>
#include <string_view>

#include "dates.hpp"
#include "numbers.hpp"

namespace words {

namespace {

constexpr std::array<std::string_view, 20> UNITS_M = {
    "",       "один",        "два",        "три",        "четыре",       "пять",       "шесть",       "семь",       "восемь",       "девять",
    "десять", "одиннадцать", "двенадцать", "тринадцать", "четырнадцать", "пятнадцать", "шестнадцать", "семнадцать", "восемнадцать", "девятнадцать"};
constexpr std::array<std::string_view, 10> TENS = {"",          "",           "двадцать",  "тридцать",    "сорок",
                                                   "пятьдесят", "шестьдесят", "семьдесят", "восемьдесят", "девяносто"};
constexpr std::array<std::string_view, 10> HUNDREDS = {"",        "сто",      "двести",  "триста",    "четыреста",
                                                       "пятьсот", "шестьсот", "семьсот", "восемьсот", "девятьсот"};
constexpr std::array<std::string_view, 12> MONTHS_GEN = {"января", "февраля", "марта",    "апреля",  "мая",    "июня",
                                                         "июля",   "августа", "сентября", "октября", "ноября", "декабря"};

void append(std::string &out, std::string_view word) {
    if (word.empty()) {
        return;
    }
    if (!out.empty()) {
        out += ' ';
    }
    out += word;
}

/// Тройка разрядов 1..999 прописью.
std::string triad(int value, bool feminine) {
    std::string out;
    append(out, HUNDREDS.at(static_cast<std::size_t>(value / 100)));
    auto const rest = value % 100;
    if (rest < 20) {
        if (feminine && rest == 1) {
            append(out, "одна");
        } else if (feminine && rest == 2) {
            append(out, "две");
        } else {
            append(out, UNITS_M.at(static_cast<std::size_t>(rest)));
        }
        return out;
    }
    append(out, TENS.at(static_cast<std::size_t>(rest / 10)));
    auto const unit = rest % 10;
    if (feminine && unit == 1) {
        append(out, "одна");
    } else if (feminine && unit == 2) {
        append(out, "две");
    } else {
        append(out, UNITS_M.at(static_cast<std::size_t>(unit)));
    }
    return out;
}

/// Первая буква заглавная (кириллица в UTF-8: два байта).
std::string capitalize(std::string text) {
    if (text.size() >= 2 && static_cast<unsigned char>(text[0]) == 0xD0) {
        auto const second = static_cast<unsigned char>(text[1]);
        if (second >= 0xB0 && second <= 0xBF) {
            text[1] = static_cast<char>(second - 0x20);
        }
    } else if (text.size() >= 2 && static_cast<unsigned char>(text[0]) == 0xD1) {
        auto const second = static_cast<unsigned char>(text[1]);
        if (second >= 0x80 && second <= 0x8F) {
            text[0] = static_cast<char>(0xD0);
            text[1] = static_cast<char>(second + 0x20);
        }
    }
    return text;
}

} // namespace

std::string plural(std::int64_t value, std::string const &one, std::string const &few, std::string const &many) {
    auto const n = value < 0 ? -value : value;
    if (n % 100 >= 11 && n % 100 <= 14) {
        return many;
    }
    if (n % 10 == 1) {
        return one;
    }
    if (n % 10 >= 2 && n % 10 <= 4) {
        return few;
    }
    return many;
}

std::string number(std::int64_t value, bool feminine) {
    if (value == 0) {
        return "ноль";
    }
    std::string out;
    if (value < 0) {
        out = "минус";
        value = -value;
    }
    struct Scale {
        std::int64_t size;
        bool feminine;
        char const *one;
        char const *few;
        char const *many;
    };
    constexpr std::array<Scale, 3> SCALES = {{
        {.size = 1'000'000'000, .feminine = false, .one = "миллиард", .few = "миллиарда", .many = "миллиардов"},
        {.size = 1'000'000, .feminine = false, .one = "миллион", .few = "миллиона", .many = "миллионов"},
        {.size = 1'000, .feminine = true, .one = "тысяча", .few = "тысячи", .many = "тысяч"},
    }};
    for (auto const &scale : SCALES) {
        auto const count = value / scale.size;
        if (count != 0) {
            append(out, number(count, scale.feminine));
            append(out, plural(count, scale.one, scale.few, scale.many));
            value %= scale.size;
        }
    }
    if (value != 0) {
        append(out, triad(static_cast<int>(value), feminine));
    }
    return out;
}

std::string rubles(std::int64_t kop) {
    auto const whole = kop / 100;
    auto const cents = kop % 100;
    auto formatted = numbers::format_hundredths(whole * 100);
    formatted.resize(formatted.size() - 3);
    return formatted + " (" + capitalize(number(whole)) + ") " + plural(whole, "рубль", "рубля", "рублей") + " " + (cents < 10 ? "0" : "") +
           std::to_string(cents) + " " + plural(cents, "копейка", "копейки", "копеек");
}

std::string date_long(std::string const &date) {
    check_date(date);
    auto const month = std::stoi(date.substr(5, 2));
    return "«" + date.substr(8, 2) + "» " + std::string(MONTHS_GEN.at(static_cast<std::size_t>(month - 1))) + " " + date.substr(0, 4) + " г.";
}

std::string date_short(std::string const &date) {
    if (date.empty()) {
        return {};
    }
    check_date(date);
    return date.substr(8, 2) + "." + date.substr(5, 2) + "." + date.substr(0, 4);
}

} // namespace words
