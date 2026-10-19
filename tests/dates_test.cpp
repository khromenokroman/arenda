#include "dates.hpp"

#include <gtest/gtest.h>

#include "errors.hpp"
#include "test_util.hpp"

/**
 * @brief Существующие даты, в том числе 29 февраля високосного года.
 */
TEST(Dates, Valid) {
    EXPECT_NO_THROW(check_date("2026-10-19"));
    EXPECT_NO_THROW(check_date("2024-02-29"));
    EXPECT_NO_THROW(check_date("2000-02-29")) << "2000 високосный: делится на 400";
    EXPECT_NO_THROW(check_date("1900-01-01"));
}

/**
 * @brief Неверный формат и несуществующие даты: отказ с самим значением в тексте.
 */
TEST(Dates, Invalid) {
    EXPECT_THROW_REASON(check_date("19.10.2026"), UserError, "в формате ГГГГ-ММ-ДД: «19.10.2026»");
    EXPECT_THROW_REASON(check_date("2026-1-19"), UserError, "в формате ГГГГ-ММ-ДД");
    EXPECT_THROW_REASON(check_date("2026-10-1x"), UserError, "в формате ГГГГ-ММ-ДД");
    EXPECT_THROW_REASON(check_date("2026-02-29"), UserError, "нет такой даты: «2026-02-29»");
    EXPECT_THROW_REASON(check_date("1900-02-29"), UserError, "нет такой даты"); // 1900 не високосный
    EXPECT_THROW_REASON(check_date("2026-13-01"), UserError, "нет такой даты");
    EXPECT_THROW_REASON(check_date("2026-04-31"), UserError, "нет такой даты");
    EXPECT_THROW_REASON(check_date("2026-00-10"), UserError, "нет такой даты");
    EXPECT_THROW_REASON(check_date("1899-12-31"), UserError, "нет такой даты");
}
