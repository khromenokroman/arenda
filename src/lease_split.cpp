#include "lease_split.hpp"

#include <algorithm>

#include "dates.hpp"
#include "errors.hpp"

std::vector<LeasePart> split_lease(std::string const &start, int total_months, std::int64_t rent_kop, std::int64_t limit_kop) {
    check_date(start);
    if (total_months < 1) {
        throw UserError("срок аренды должен быть не меньше месяца");
    }
    if (rent_kop <= 0) {
        throw UserError("плата в месяц должна быть больше нуля");
    }
    if (limit_kop <= 0) {
        throw UserError("наибольшая сумма договора должна быть больше нуля");
    }
    auto const fit = std::max<std::int64_t>(1, std::min<std::int64_t>(total_months, limit_kop / rent_kop));
    auto const count = static_cast<int>((total_months + fit - 1) / fit);
    auto const base = total_months / count;
    auto const longer = total_months % count;

    std::vector<LeasePart> parts;
    int offset = 0;
    for (int i = 0; i < count; ++i) {
        auto const months = base + (i < longer ? 1 : 0);
        parts.push_back({add_months(start, offset), add_days(add_months(start, offset + months), -1), months});
        offset += months;
    }
    return parts;
}
