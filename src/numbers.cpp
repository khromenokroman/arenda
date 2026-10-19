#include "numbers.hpp"

#include <charconv>

#include "errors.hpp"

namespace numbers {

namespace {

/// Наибольшее значение в сотых: триллион рублей, с запасом от переполнения при умножении на месяцы.
constexpr std::int64_t MAX_HUNDREDTHS = std::int64_t{100'000'000'000'000};

/// Убирает пробелы и неразрывные пробелы (U+00A0, U+202F) между разрядами.
std::string strip_spaces(std::string_view text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        auto const c = static_cast<unsigned char>(text[i]);
        if (c == ' ') {
            continue;
        }
        if (c == 0xC2 && i + 1 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0xA0) {
            ++i;
            continue;
        }
        if (c == 0xE2 && i + 2 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0x80 && static_cast<unsigned char>(text[i + 2]) == 0xAF) {
            i += 2;
            continue;
        }
        out += text[i];
    }
    return out;
}

[[noreturn]] void bad(std::string_view what, std::string_view text, std::string_view why) {
    throw UserError(std::string(what) + ": " + std::string(why) + ": «" + std::string(text) + "»");
}

bool all_digits(std::string_view text) {
    for (auto const c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return !text.empty();
}

} // namespace

std::int64_t parse_hundredths(std::string_view text, std::string_view what) {
    auto const plain = strip_spaces(text);
    if (plain.empty()) {
        bad(what, text, "не заполнено");
    }
    auto const sep = plain.find_first_of(",.");
    std::string_view const whole = std::string_view{plain}.substr(0, sep);
    std::string_view const fraction = sep == std::string::npos ? std::string_view{} : std::string_view{plain}.substr(sep + 1);
    if (!all_digits(whole) || (!fraction.empty() && !all_digits(fraction))) {
        bad(what, text, "нужно число, например 10 000 или 12,5");
    }
    if (fraction.size() > 2) {
        bad(what, text, "не больше двух знаков после запятой");
    }
    if (whole.size() > 13) {
        bad(what, text, "слишком большое число");
    }
    std::int64_t value = 0;
    std::from_chars(whole.data(), whole.data() + whole.size(), value);
    value *= 100;
    if (!fraction.empty()) {
        value += std::int64_t{fraction[0] - '0'} * 10;
        if (fraction.size() == 2) {
            value += fraction[1] - '0';
        }
    }
    if (value > MAX_HUNDREDTHS) {
        bad(what, text, "слишком большое число");
    }
    return value;
}

std::string format_hundredths(std::int64_t value) {
    auto const negative = value < 0;
    auto const abs = negative ? -value : value;
    auto whole = std::to_string(abs / 100);
    std::string grouped;
    for (std::size_t i = 0; i < whole.size(); ++i) {
        if (i != 0 && (whole.size() - i) % 3 == 0) {
            grouped += ' ';
        }
        grouped += whole[i];
    }
    auto const cents = abs % 100;
    return (negative ? "-" : "") + grouped + "," + (cents < 10 ? "0" : "") + std::to_string(cents);
}

int parse_int(std::string_view text, std::string_view what, int min, int max) {
    auto const plain = strip_spaces(text);
    int value = 0;
    auto const [ptr, ec] = std::from_chars(plain.data(), plain.data() + plain.size(), value);
    if (plain.empty() || ec != std::errc{} || ptr != plain.data() + plain.size()) {
        bad(what, text, "нужно целое число");
    }
    if (value < min || value > max) {
        bad(what, text, "нужно число от " + std::to_string(min) + " до " + std::to_string(max));
    }
    return value;
}

} // namespace numbers
