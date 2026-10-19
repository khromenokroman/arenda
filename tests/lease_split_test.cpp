#include "lease_split.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "dates.hpp"
#include "errors.hpp"
#include "test_util.hpp"

namespace {

constexpr std::int64_t LIMIT = 100'000'00;

std::string months_of(std::vector<LeasePart> const &parts) {
    std::string out;
    for (auto const &p : parts) {
        out += (out.empty() ? "" : "+") + std::to_string(p.months);
    }
    return out;
}

} // namespace

/**
 * @brief Месяцы по договорам при разной плате: таблица из обсуждения с 1С; каждый договор не больше порога.
 */
TEST(LeaseSplit, MonthsByRent) {
    struct Case {
        std::int64_t rent_rub;
        std::string expected;
    };
    std::vector<Case> const cases = {
        {9090, "11"},
        {9091, "6+5"},
        {10000, "6+5"},
        {15000, "6+5"},
        {16666, "6+5"},
        {16667, "4+4+3"},
        {20000, "4+4+3"},
        {25000, "4+4+3"},
        {25001, "3+3+3+2"},
        {33333, "3+3+3+2"},
        {33334, "2+2+2+2+2+1"},
        {50000, "2+2+2+2+2+1"},
        {50001, "1+1+1+1+1+1+1+1+1+1+1"},
        {150000, "1+1+1+1+1+1+1+1+1+1+1"},
    };
    for (auto const &c : cases) {
        auto const parts = split_lease("2026-10-19", 11, c.rent_rub * 100, LIMIT);
        EXPECT_EQ(months_of(parts), c.expected) << "плата " << c.rent_rub << " руб.";
        int total = 0;
        for (auto const &p : parts) {
            total += p.months;
            if (c.rent_rub * 100 <= LIMIT) {
                EXPECT_LE(c.rent_rub * 100 * p.months, LIMIT) << "плата " << c.rent_rub << ": договор " << p.start << " больше порога";
            }
        }
        EXPECT_EQ(total, 11) << "плата " << c.rent_rub << ": сумма месяцев";
    }
}

/**
 * @brief Даты частей: подряд без разрывов, конец - день перед началом следующей, весь срок 11 месяцев.
 */
TEST(LeaseSplit, Dates) {
    auto const parts = split_lease("2026-10-19", 11, 20'000'00, LIMIT);
    ASSERT_EQ(parts.size(), 3U);
    EXPECT_EQ(parts[0].start, "2026-10-19");
    EXPECT_EQ(parts[0].end, "2027-02-18");
    EXPECT_EQ(parts[1].start, "2027-02-19");
    EXPECT_EQ(parts[1].end, "2027-06-18");
    EXPECT_EQ(parts[2].start, "2027-06-19");
    EXPECT_EQ(parts[2].end, "2027-09-18") << "11 месяцев от 19.10.2026 заканчиваются 18.09.2027";
    for (std::size_t i = 1; i < parts.size(); ++i) {
        EXPECT_EQ(add_days(parts[i - 1].end, 1), parts[i].start) << "разрыв или нахлёст между частями " << i << " и " << i + 1;
    }
}

/**
 * @brief Начало 31-го числа: границы от исходной даты, после февраля не съезжают на 28-е (ошибка 1С).
 */
TEST(LeaseSplit, EndOfMonthStart) {
    auto const parts = split_lease("2026-01-31", 11, 50'000'00, LIMIT);
    ASSERT_GE(parts.size(), 3U);
    EXPECT_EQ(parts[0].start, "2026-01-31");
    EXPECT_EQ(parts[0].end, "2026-03-30");
    EXPECT_EQ(parts[1].start, "2026-03-31") << "в 1С было бы 28-е";
    EXPECT_EQ(parts[2].start, "2026-05-31");
    EXPECT_EQ(parts.back().end, "2026-12-30");
}

/**
 * @brief Срок меньше 11 месяцев и неверные данные.
 */
TEST(LeaseSplit, OtherTermsAndErrors) {
    EXPECT_EQ(months_of(split_lease("2026-10-19", 6, 20'000'00, LIMIT)), "3+3");
    EXPECT_EQ(months_of(split_lease("2026-10-19", 1, 9'000'00, LIMIT)), "1");
    EXPECT_THROW_REASON((void)split_lease("2026-10-19", 11, 0, LIMIT), UserError, "плата в месяц должна быть больше нуля");
    EXPECT_THROW_REASON((void)split_lease("2026-10-19", 0, 100, LIMIT), UserError, "не меньше месяца");
    EXPECT_THROW_REASON((void)split_lease("2026-10-19", 11, 100, 0), UserError, "наибольшая сумма");
    EXPECT_THROW_REASON((void)split_lease("19.10.2026", 11, 100, LIMIT), UserError, "ГГГГ-ММ-ДД");
}

/**
 * @brief Прибавление месяцев и дней: конец месяца, високосный год, переход года.
 */
TEST(Dates, Arithmetic) {
    EXPECT_EQ(add_months("2026-01-31", 1), "2026-02-28");
    EXPECT_EQ(add_months("2024-01-31", 1), "2024-02-29");
    EXPECT_EQ(add_months("2026-10-19", 11), "2027-09-19");
    EXPECT_EQ(add_months("2026-03-31", -1), "2026-02-28");
    EXPECT_EQ(add_days("2026-03-01", -1), "2026-02-28");
    EXPECT_EQ(add_days("2026-12-31", 1), "2027-01-01");
}
