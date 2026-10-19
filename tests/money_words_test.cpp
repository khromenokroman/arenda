#include "money_words.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "errors.hpp"
#include "test_util.hpp"

using namespace words;

namespace {

struct Case {
    std::int64_t value;
    std::string expected;
};

} // namespace

/**
 * @brief Числа прописью: разряды, 11-19, род единиц у тысяч, миллионы.
 */
TEST(Words, Number) {
    std::vector<Case> const cases = {
        {0, "ноль"},
        {1, "один"},
        {12, "двенадцать"},
        {21, "двадцать один"},
        {100, "сто"},
        {215, "двести пятнадцать"},
        {1000, "одна тысяча"},
        {2000, "две тысячи"},
        {5000, "пять тысяч"},
        {11000, "одиннадцать тысяч"},
        {21000, "двадцать одна тысяча"},
        {22500, "двадцать две тысячи пятьсот"},
        {100000, "сто тысяч"},
        {1000001, "один миллион один"},
        {2345678, "два миллиона триста сорок пять тысяч шестьсот семьдесят восемь"},
    };
    for (auto const &c : cases) {
        auto const got = number(c.value);
        EXPECT_TRUE(got == c.expected) << c.value << "\n  ожидалось: " << c.expected << "\n  получено:  " << got;
    }
    EXPECT_EQ(number(2, true), "две") << "женский род: две копейки";
}

/**
 * @brief Сумма для договора: число с разрядами, прописью с заглавной, рубли и копейки в нужной форме.
 */
TEST(Words, Rubles) {
    std::vector<Case> const cases = {
        {1'000'000, "10 000 (Десять тысяч) рублей 00 копеек"},
        {2'000'000, "20 000 (Двадцать тысяч) рублей 00 копеек"},
        {2'100'101, "21 001 (Двадцать одна тысяча один) рубль 01 копейка"},
        {909'091, "9 090 (Девять тысяч девяносто) рублей 91 копейка"},
        {302, "3 (Три) рубля 02 копейки"},
        {1'100, "11 (Одиннадцать) рублей 00 копеек"},
        {50, "0 (Ноль) рублей 50 копеек"},
    };
    for (auto const &c : cases) {
        auto const got = rubles(c.value);
        EXPECT_TRUE(got == c.expected) << c.value << " коп.\n  ожидалось: " << c.expected << "\n  получено:  " << got;
    }
}

/**
 * @brief Формы слова по числу.
 */
TEST(Words, Plural) {
    EXPECT_EQ(plural(1, "день", "дня", "дней"), "день");
    EXPECT_EQ(plural(3, "день", "дня", "дней"), "дня");
    EXPECT_EQ(plural(11, "день", "дня", "дней"), "дней");
    EXPECT_EQ(plural(112, "день", "дня", "дней"), "дней");
    EXPECT_EQ(plural(101, "день", "дня", "дней"), "день");
}

/**
 * @brief Даты для текста договора.
 */
TEST(Words, Dates) {
    EXPECT_TRUE(date_long("2026-10-19") == "«19» октября 2026 г.") << date_long("2026-10-19");
    EXPECT_TRUE(date_long("2027-03-01") == "«01» марта 2027 г.") << date_long("2027-03-01");
    EXPECT_EQ(date_short("1990-05-17"), "17.05.1990");
    EXPECT_EQ(date_short(""), "");
    EXPECT_THROW_REASON((void)date_long("2026-02-30"), UserError, "нет такой даты");
}
