#include "requisites.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "errors.hpp"

namespace requisites {

namespace {

bool all_digits(std::string_view text) {
    return std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; });
}

/// Проверяет, что text из count цифр, иначе отказ с названием реквизита.
void check_digits(std::string_view what, std::string const &text, std::size_t count) {
    if (text.size() != count || !all_digits(text)) {
        throw UserError(std::string(what) + " должен состоять из " + std::to_string(count) + " цифр: «" + text + "»");
    }
}

int digit(std::string const &text, std::size_t pos) { return text.at(pos) - '0'; }

/// Взвешенная сумма первых weights.size() цифр по модулю 11 и 10: контрольная цифра ИНН.
int inn_control(std::string const &inn, std::span<int const> weights) {
    int sum = 0;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        sum += digit(inn, i) * weights[i];
    }
    return sum % 11 % 10;
}

/// Остаток от деления числа из первых count цифр на divisor, без переполнения (по одной цифре).
int mod_prefix(std::string const &text, std::size_t count, int divisor) {
    std::int64_t rest = 0;
    for (std::size_t i = 0; i < count; ++i) {
        rest = ((rest * 10) + digit(text, i)) % divisor;
    }
    return static_cast<int>(rest);
}

[[noreturn]] void bad_control(std::string_view what, std::string const &text) {
    throw UserError(std::string(what) + " «" + text + "» с ошибкой: не сходится контрольная цифра");
}

} // namespace

void check_inn_org(std::string const &inn) {
    if (inn.empty()) {
        return;
    }
    check_digits("ИНН организации", inn, 10);
    constexpr std::array<int, 9> WEIGHTS = {2, 4, 10, 3, 5, 9, 4, 6, 8};
    if (inn_control(inn, WEIGHTS) != digit(inn, 9)) {
        bad_control("ИНН", inn);
    }
}

void check_inn_person(std::string const &inn) {
    if (inn.empty()) {
        return;
    }
    check_digits("ИНН физического лица", inn, 12);
    constexpr std::array<int, 10> WEIGHTS_11 = {7, 2, 4, 10, 3, 5, 9, 4, 6, 8};
    constexpr std::array<int, 11> WEIGHTS_12 = {3, 7, 2, 4, 10, 3, 5, 9, 4, 6, 8};
    if (inn_control(inn, WEIGHTS_11) != digit(inn, 10) || inn_control(inn, WEIGHTS_12) != digit(inn, 11)) {
        bad_control("ИНН", inn);
    }
}

void check_ogrn(std::string const &ogrn) {
    if (ogrn.empty()) {
        return;
    }
    check_digits("ОГРН", ogrn, 13);
    if (mod_prefix(ogrn, 12, 11) % 10 != digit(ogrn, 12)) {
        bad_control("ОГРН", ogrn);
    }
}

void check_ogrnip(std::string const &ogrnip) {
    if (ogrnip.empty()) {
        return;
    }
    check_digits("ОГРНИП", ogrnip, 15);
    if (mod_prefix(ogrnip, 14, 13) % 10 != digit(ogrnip, 14)) {
        bad_control("ОГРНИП", ogrnip);
    }
}

void check_kpp(std::string const &kpp) {
    if (kpp.empty()) {
        return;
    }
    auto const code_char = [](char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'); };
    if (kpp.size() != 9 || !all_digits(std::string_view(kpp).substr(0, 4)) || !code_char(kpp[4]) || !code_char(kpp[5]) ||
        !all_digits(std::string_view(kpp).substr(6))) {
        throw UserError("КПП должен состоять из 9 знаков (4 цифры, 2 цифры или латинские буквы, 3 цифры): «" + kpp + "»");
    }
}

void check_bik(std::string const &bik) {
    if (bik.empty()) {
        return;
    }
    check_digits("БИК", bik, 9);
}

void check_account(std::string const &account) {
    if (account.empty()) {
        return;
    }
    check_digits("Расчётный счёт", account, 20);
}

void check_account_bik(std::string const &account, std::string const &bik) {
    if (account.empty() || bik.empty()) {
        return;
    }
    check_account(account);
    check_bik(bik);
    auto const key = bik.substr(6) + account;
    constexpr std::array<int, 3> WEIGHTS = {7, 1, 3};
    int sum = 0;
    for (std::size_t i = 0; i < key.size(); ++i) {
        sum += (digit(key, i) * WEIGHTS.at(i % WEIGHTS.size())) % 10;
    }
    if (sum % 10 != 0) {
        throw UserError("расчётный счёт «" + account + "» не подходит к БИК «" + bik + "»: не сходится контрольный ключ, проверьте счёт и БИК");
    }
}

} // namespace requisites
