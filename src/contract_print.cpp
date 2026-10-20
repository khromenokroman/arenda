#include "contract_print.hpp"

#include <stdexcept>

#include "contract_template.hpp"
#include "errors.hpp"
#include "money_words.hpp"
#include "names.hpp"
#include "numbers.hpp"
#include "tenants.hpp"

namespace contract_print {

namespace {

/// Место для заполнения от руки вместо пустого значения.
constexpr std::string_view BLANK = "____________";

constexpr std::string_view BEGIN_MARK = "<!-- CONTRACT -->";
constexpr std::string_view END_MARK = "<!-- /CONTRACT -->";

std::string escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (auto const c : text) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            default:
                out += c;
        }
    }
    return out;
}

/// Значение для текста договора: экранированное или прочерк.
std::string text_or_blank(std::string const &value) { return value.empty() ? std::string(BLANK) : escape(value); }

std::string field(Fields const &fields, char const *key) {
    auto const it = fields.find(key);
    return it == fields.end() ? std::string{} : it->second;
}

/// Сумма из карточки («5 000», «5000,5») для текста: «5 000» или «5 000,50»; неразборчивое - как есть.
std::string rubles_short(std::string const &text) {
    try {
        auto formatted = numbers::format_hundredths(numbers::parse_hundredths(text, "сумма"));
        if (formatted.ends_with(",00")) {
            formatted.resize(formatted.size() - 3);
        }
        return formatted;
    } catch (UserError const &) {
        return text;
    }
}

/// Процент из карточки («0,3», «0,50») для текста: «0,3», «0,5», «1»; неразборчивое - как есть.
std::string percent_short(std::string const &text) {
    try {
        auto formatted = numbers::format_hundredths(numbers::parse_hundredths(text, "процент"));
        while (formatted.ends_with('0')) {
            formatted.pop_back();
        }
        if (formatted.ends_with(',')) {
            formatted.pop_back();
        }
        return formatted;
    } catch (UserError const &) {
        return text;
    }
}

/// «действующего» или «действующей» по отчеству подписанта.
std::string acting(std::string const &signer_name) { return names::gender_by_patronymic(signer_name) == 'f' ? "действующей" : "действующего"; }

/// Строка реквизитов «Подпись: значение»; пустое значение - прочерк.
std::string line(std::string_view label, std::string const &value) { return std::string(label) + ": " + text_or_blank(value) + "<br>"; }

std::string landlord_requisites(Fields const &l) {
    auto const inn_kpp = field(l, "inn").empty() && field(l, "kpp").empty() ? std::string{} : field(l, "inn") + "/" + field(l, "kpp");
    return "<span class=\"party\">АРЕНДОДАТЕЛЬ:</span><br>" + text_or_blank(field(l, "name_short")) + "<br>" + line("Адрес", field(l, "address")) +
           line("ОГРН", field(l, "ogrn")) + line("ИНН/КПП", inn_kpp) + line("Р/с", field(l, "account")) + line("Банк", field(l, "bank")) +
           line("БИК", field(l, "bik")) + line("К/с", field(l, "corr_account")) + line("Тел.", field(l, "phone")) + "<div class=\"sign\">" +
           text_or_blank(field(l, "signer_post")) + " ____________ / " + text_or_blank(names::initials_first(field(l, "signer_name"))) +
           " /<br>М. П.</div>";
}

std::string tenant_requisites(Tenant const &tenant) {
    auto const &t = tenant.fields;
    auto const kind = field(t, "kind");
    std::string out = "<span class=\"party\">АРЕНДАТОР:</span><br>" + text_or_blank(Tenants::title(tenant)) + "<br>";
    std::string sign;
    if (kind == "org") {
        auto const inn_kpp = field(t, "inn") + "/" + field(t, "kpp");
        out += line("Адрес", field(t, "address")) + line("ОГРН", field(t, "ogrn")) + line("ИНН/КПП", inn_kpp);
        sign = text_or_blank(field(t, "signer_post")) + " ____________ / " + text_or_blank(names::initials_first(field(t, "signer_name"))) +
               " /<br>М. П. (при наличии)";
    } else if (kind == "ip") {
        out += line("Адрес", field(t, "address")) + line("ОГРНИП", field(t, "ogrnip")) + line("ИНН", field(t, "inn"));
        sign = "____________ / " + text_or_blank(names::initials_first(field(t, "name"))) + " /<br>М. П. (при наличии)";
    } else {
        out += line("Адрес регистрации", field(t, "address")) + line("Паспорт", field(t, "passport")) +
               line("Выдан",
                    field(t, "passport_issued") + (field(t, "passport_date").empty() ? "" : " " + words::date_short(field(t, "passport_date")))) +
               line("Код подразделения", field(t, "passport_code")) + line("ИНН", field(t, "inn"));
        sign = "____________ / " + text_or_blank(names::initials_first(field(t, "name"))) + " /";
    }
    if (kind != "person" || !field(t, "account").empty()) {
        out +=
            line("Р/с", field(t, "account")) + line("Банк", field(t, "bank")) + line("БИК", field(t, "bik")) + line("К/с", field(t, "corr_account"));
    }
    out += line("Тел.", field(t, "phone"));
    return out + "<div class=\"sign\">" + sign + "</div>";
}

/// Вступление об арендаторе до «именуемый в дальнейшем».
std::string tenant_preamble(Fields const &t) {
    auto const kind = field(t, "kind");
    if (kind == "org") {
        return text_or_blank(field(t, "name_full")) + " (ОГРН " + text_or_blank(field(t, "ogrn")) + ", ИНН " + text_or_blank(field(t, "inn")) +
               "), в лице " + text_or_blank(field(t, "signer_post_gen")) + " " + text_or_blank(field(t, "signer_name_gen")) + ", " +
               acting(field(t, "signer_name")) + " на основании " + text_or_blank(field(t, "signer_basis_gen"));
    }
    if (kind == "ip") {
        return "Индивидуальный предприниматель " + text_or_blank(field(t, "name")) + " (ОГРНИП " + text_or_blank(field(t, "ogrnip")) + ", ИНН " +
               text_or_blank(field(t, "inn")) + ")";
    }
    auto const female = field(t, "gender") == "f";
    return text_or_blank(field(t, "name")) + ", " + text_or_blank(words::date_short(field(t, "birth_date"))) + " года рождения, паспорт " +
           text_or_blank(field(t, "passport")) + ", выдан " + text_or_blank(field(t, "passport_issued")) + " " +
           text_or_blank(words::date_short(field(t, "passport_date"))) + ", код подразделения " + text_or_blank(field(t, "passport_code")) + ", " +
           (female ? "зарегистрированная" : "зарегистрированный") + " по адресу: " + text_or_blank(field(t, "address")) + ", ИНН " +
           text_or_blank(field(t, "inn"));
}

std::string equipment_rows(std::vector<EquipmentRow> const &rows) {
    if (rows.empty()) {
        return "<tr><td class=\"n\">1</td><td colspan=\"4\">не используется</td></tr>";
    }
    std::string out;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        out += "<tr><td class=\"n\">" + std::to_string(i + 1) + "</td><td>" + escape(rows[i].name) + "</td><td>" + escape(rows[i].model) +
               "</td><td>" + numbers::format_hundredths(rows[i].power_c) + "</td><td>" + std::to_string(rows[i].qty) + "</td></tr>";
    }
    return out;
}

} // namespace

std::string fill(std::string_view tmpl, Values const &values) {
    std::string out;
    out.reserve(tmpl.size() * 2);
    std::size_t pos = 0;
    while (true) {
        auto const open = tmpl.find("{{", pos);
        if (open == std::string_view::npos) {
            out += tmpl.substr(pos);
            return out;
        }
        out += tmpl.substr(pos, open - pos);
        auto const raw = tmpl.substr(open, 3) == "{{{";
        auto const start = open + (raw ? 3 : 2);
        auto const close = tmpl.find(raw ? "}}}" : "}}", start);
        if (close == std::string_view::npos) {
            throw std::logic_error("шаблон договора: незакрытая метка с позиции " + std::to_string(open));
        }
        auto const key = tmpl.substr(start, close - start);
        auto const &map = raw ? values.html : values.text;
        auto const it = map.find(key);
        if (it == map.end()) {
            throw std::logic_error("шаблон договора: нет значения для метки " + std::string(key));
        }
        out += raw ? it->second : text_or_blank(it->second);
        pos = close + (raw ? 3 : 2);
    }
}

Values values(Contract const &c) {
    auto const &l = c.landlord;
    auto const &t = c.tenant;
    Tenant const tenant{t, c.tenant_id};
    auto const kind = field(t, "kind");
    Values v;
    v.text = {
        {"number", c.number},
        {"city", field(l, "city")},
        {"date_long", words::date_long(c.start)},
        {"date_start_long", words::date_long(c.start)},
        {"date_end_long", words::date_long(c.end)},
        {"landlord_name_full", field(l, "name_full")},
        {"landlord_ogrn", field(l, "ogrn")},
        {"landlord_inn", field(l, "inn")},
        {"landlord_signer_post_gen", field(l, "signer_post_gen")},
        {"landlord_signer_name_gen", field(l, "signer_name_gen")},
        {"landlord_acting", acting(field(l, "signer_name"))},
        {"landlord_signer_basis_gen", field(l, "signer_basis_gen")},
        {"landlord_signer_short", names::initials_first(field(l, "signer_name"))},
        {"tenant_named", kind == "org" ? "именуемое" : (field(t, "gender") == "f" ? "именуемая" : "именуемый")},
        {"tenant_signer_short", names::initials_first(field(t, kind == "org" ? "signer_name" : "name"))},
        {"area", field(t, "area")},
        {"trade_area", field(t, "trade_area")},
        {"floor", field(t, "floor")},
        {"object_name_gen", field(l, "object_name_gen")},
        {"object_address", field(l, "object_address")},
        {"purpose", field(t, "purpose")},
        {"premises_condition", field(t, "premises_condition")},
        {"activity_gen", field(t, "activity_gen")},
        {"hours_weekdays", field(l, "hours_weekdays")},
        {"hours_weekends", field(l, "hours_weekends")},
        {"days_off", field(l, "days_off")},
        {"region_gen", field(l, "region_gen")},
        {"municipality_gen", field(l, "municipality_gen")},
        {"rent_words", words::rubles(c.rent_kop)},
        {"fire_fine", rubles_short(field(l, "fire_fine"))},
        {"fine_sign", rubles_short(field(l, "fine_sign"))},
        {"fine_control", rubles_short(field(l, "fine_control"))},
        {"fine_transfer", rubles_short(field(l, "fine_transfer"))},
        {"fine_waste", rubles_short(field(l, "fine_waste"))},
        {"penalty_rent_pct", percent_short(field(l, "penalty_rent_pct"))},
        {"penalty_return_pct", percent_short(field(l, "penalty_return_pct"))},
        {"total_power", numbers::format_hundredths(total_power(c.equipment))},
    };
    v.html = {
        {"tenant_preamble", tenant_preamble(t)},
        {"requisites",
         "<table class=\"requisites\"><tr><td>" + landlord_requisites(l) + "</td><td>" + tenant_requisites(tenant) + "</td></tr></table>"},
        {"equipment_rows", equipment_rows(c.equipment)},
    };
    return v;
}

std::string render(std::vector<Contract> const &contracts) {
    if (contracts.empty()) {
        throw UserError("не выбрано ни одного договора для печати");
    }
    auto const tmpl = contract_template();
    auto const begin = tmpl.find(BEGIN_MARK);
    auto const end = tmpl.find(END_MARK);
    if (begin == std::string_view::npos || end == std::string_view::npos || end < begin) {
        throw std::logic_error("шаблон договора: нет меток CONTRACT и /CONTRACT");
    }
    auto const title = contracts.size() == 1 ? "Договор аренды № " + contracts[0].number : "Договоры аренды: " + std::to_string(contracts.size());
    Values const page{.text = {{"title", title}}, .html = {}};
    auto out = fill(tmpl.substr(0, begin), page);
    auto const sheet = tmpl.substr(begin + BEGIN_MARK.size(), end - begin - BEGIN_MARK.size());
    for (auto const &c : contracts) {
        out += fill(sheet, values(c));
    }
    out += fill(tmpl.substr(end + END_MARK.size()), page);
    return out;
}

} // namespace contract_print
