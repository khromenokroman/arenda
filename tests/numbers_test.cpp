#include "numbers.hpp"

#include <gtest/gtest.h>

#include "errors.hpp"
#include "test_util.hpp"

using namespace numbers;

/**
 * @brief Разбор сумм и площадей: разряды через пробел, запятая или точка, один или два знака после запятой.
 */
TEST(Numbers, ParseHundredths) {
    EXPECT_EQ(parse_hundredths("10 000", "сумма"), 1'000'000);
    EXPECT_EQ(parse_hundredths("10 000,5", "сумма"), 1'000'050) << "неразрывный пробел между разрядами";
    EXPECT_EQ(parse_hundredths("9090.91", "сумма"), 909'091);
    EXPECT_EQ(parse_hundredths(" 12,05 ", "площадь"), 1205);
    EXPECT_EQ(parse_hundredths("0", "сумма"), 0);
    EXPECT_EQ(parse_hundredths("7,", "сумма"), 700) << "запятая без дробной части";
}

/**
 * @brief Ошибки разбора называют величину, причину и само значение.
 */
TEST(Numbers, ParseErrors) {
    EXPECT_THROW_REASON((void)parse_hundredths("", "Плата"), UserError, "Плата: не заполнено");
    EXPECT_THROW_REASON((void)parse_hundredths("10 тыс", "Плата"), UserError, "Плата: нужно число, например 10 000 или 12,5: «10 тыс»");
    EXPECT_THROW_REASON((void)parse_hundredths("-5", "Плата"), UserError, "нужно число");
    EXPECT_THROW_REASON((void)parse_hundredths("1,234", "Площадь"), UserError, "не больше двух знаков после запятой");
    EXPECT_THROW_REASON((void)parse_hundredths("1,2,3", "Площадь"), UserError, "нужно число");
    EXPECT_THROW_REASON((void)parse_hundredths("99999999999999", "Плата"), UserError, "слишком большое");
}

/**
 * @brief Вывод: разряды через пробел, всегда два знака после запятой.
 */
TEST(Numbers, Format) {
    EXPECT_EQ(format_hundredths(1'000'000), "10 000,00");
    EXPECT_EQ(format_hundredths(909'091), "9 090,91");
    EXPECT_EQ(format_hundredths(5), "0,05");
    EXPECT_EQ(format_hundredths(123'456'789'00), "123 456 789,00");
    EXPECT_EQ(format_hundredths(-1250), "-12,50");
}

/**
 * @brief Целые числа с границами.
 */
TEST(Numbers, ParseInt) {
    EXPECT_EQ(parse_int("11", "срок", 1, 11), 11);
    EXPECT_EQ(parse_int(" 3 ", "этаж", -5, 200), 3);
    EXPECT_EQ(parse_int("-1", "этаж", -5, 200), -1);
    EXPECT_THROW_REASON((void)parse_int("12", "срок", 1, 11), UserError, "срок: нужно число от 1 до 11: «12»");
    EXPECT_THROW_REASON((void)parse_int("1,5", "срок", 1, 11), UserError, "нужно целое число");
    EXPECT_THROW_REASON((void)parse_int("", "срок", 1, 11), UserError, "нужно целое число");
}
