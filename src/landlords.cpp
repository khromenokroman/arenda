#include "landlords.hpp"

#include <array>
#include <string>

#include "contracts.hpp"
#include "errors.hpp"
#include "numbers.hpp"
#include "requisites.hpp"

namespace {

constexpr std::string_view ORG = "Организация";
constexpr std::string_view SIGNER = "Подписант";
constexpr std::string_view BANK = "Банк";
constexpr std::string_view OBJECT = "Объект аренды";
constexpr std::string_view FINES = "Ответственность арендатора (п. 5.1)";
constexpr std::string_view TERMS = "Условия договоров";

constexpr std::size_t LONG_TEXT = 1000;

void check_rubles(std::string const &text) {
    if (numbers::parse_hundredths(text, "сумма") <= 0) {
        throw UserError("сумма должна быть больше нуля: «" + text + "»");
    }
}

/// Пени в процентах в день: больше нуля и не больше 100.
void check_percent(std::string const &text) {
    auto const value = numbers::parse_hundredths(text, "процент");
    if (value <= 0 || value > std::int64_t{100} * 100) {
        throw UserError("процент должен быть больше нуля и не больше 100: «" + text + "»");
    }
}

/// Договор аренды меньше года не регистрируется (п. 6.1 договора), поэтому не больше 11 месяцев.
void check_lease_months(std::string const &text) { (void)numbers::parse_int(text, "срок аренды", 1, MAX_LEASE_MONTHS); }

constexpr std::array SPECS = {
    FieldSpec{.key = "name_full",
              .label = "Полное наименование",
              .group = ORG,
              .hint = "Общество с ограниченной ответственностью «Ромашка»",
              .max_size = LONG_TEXT,
              .required = true},
    FieldSpec{.key = "name_short", .label = "Краткое наименование", .group = ORG, .hint = "ООО «Ромашка»", .required = true},
    FieldSpec{.key = "ogrn", .label = "ОГРН", .group = ORG, .hint = "13 цифр", .check = requisites::check_ogrn},
    FieldSpec{.key = "inn", .label = "ИНН", .group = ORG, .hint = "10 цифр", .check = requisites::check_inn_org},
    FieldSpec{.key = "kpp", .label = "КПП", .group = ORG, .hint = "9 знаков", .check = requisites::check_kpp},
    FieldSpec{.key = "address", .label = "Адрес", .group = ORG, .hint = "Московская обл., г. Химки, ул. Ленина, д. 1", .max_size = LONG_TEXT},
    FieldSpec{.key = "phone", .label = "Телефон", .group = ORG, .hint = "+7 900 000-00-00"},
    FieldSpec{.key = "signer_post", .label = "Должность", .group = SIGNER, .hint = "Директор"},
    FieldSpec{.key = "signer_post_gen", .label = "Должность в родительном падеже", .group = SIGNER, .hint = "директора"},
    FieldSpec{.key = "signer_name", .label = "ФИО", .group = SIGNER, .hint = "Иванов Иван Иванович"},
    FieldSpec{.key = "signer_name_gen", .label = "ФИО в родительном падеже", .group = SIGNER, .hint = "Иванова Ивана Ивановича"},
    FieldSpec{.key = "signer_basis_gen", .label = "Действует на основании (родительный падеж)", .group = SIGNER, .hint = "Устава"},
    FieldSpec{.key = "account", .label = "Расчётный счёт", .group = BANK, .hint = "20 цифр", .check = requisites::check_account},
    FieldSpec{.key = "bank", .label = "Банк", .group = BANK, .hint = "ПАО «Банк»"},
    FieldSpec{.key = "bik", .label = "БИК", .group = BANK, .hint = "9 цифр", .check = requisites::check_bik},
    FieldSpec{.key = "corr_account", .label = "Корреспондентский счёт", .group = BANK, .hint = "20 цифр", .check = requisites::check_corr_account},
    FieldSpec{.key = "object_name_gen", .label = "Объект в родительном падеже", .group = OBJECT, .hint = "торгового центра «Ромашка»"},
    FieldSpec{.key = "object_address",
              .label = "Адрес объекта",
              .group = OBJECT,
              .hint = "Московская обл., г. Химки, ул. Ленина, д. 1",
              .max_size = LONG_TEXT},
    FieldSpec{.key = "city", .label = "Место заключения договора", .group = OBJECT, .hint = "г. Химки"},
    FieldSpec{.key = "region_gen", .label = "Регион в родительном падеже (суд, особый режим)", .group = OBJECT, .hint = "Московской области"},
    FieldSpec{
        .key = "municipality_gen", .label = "Муниципальное образование в родительном падеже", .group = OBJECT, .hint = "городского округа Химки"},
    FieldSpec{.key = "hours_weekdays",
              .label = "Режим работы: понедельник - пятница",
              .group = OBJECT,
              .hint = "с 09:00 до 18:00",
              .default_value = "с 09:00 до 18:00"},
    FieldSpec{.key = "hours_weekends",
              .label = "Режим работы: суббота, воскресенье и праздничные дни",
              .group = OBJECT,
              .hint = "с 09:00 до 16:00",
              .default_value = "с 09:00 до 16:00"},
    FieldSpec{.key = "days_off", .label = "Нерабочие дни", .group = OBJECT, .hint = "1 и 2 января, Пасха", .default_value = "1 и 2 января, Пасха"},
    FieldSpec{.key = "penalty_rent_pct",
              .label = "Пени за просрочку арендной платы (п. 5.1.1), % в день",
              .group = FINES,
              .hint = "0,3",
              .check = check_percent,
              .required = true,
              .default_value = "0,3"},
    FieldSpec{.key = "penalty_return_pct",
              .label = "Пени за несвоевременный возврат помещения (п. 5.1.2), % в день",
              .group = FINES,
              .hint = "0,5",
              .check = check_percent,
              .required = true,
              .default_value = "0,5"},
    FieldSpec{.key = "fine_sign",
              .label = "Штраф за вывеску или рекламу без согласия (п. 5.1.3), руб.",
              .group = FINES,
              .hint = "2 000",
              .check = check_rubles,
              .required = true,
              .default_value = "2 000"},
    FieldSpec{.key = "fine_control",
              .label = "Штраф за воспрепятствование контролю (п. 5.1.4), руб.",
              .group = FINES,
              .hint = "500",
              .check = check_rubles,
              .required = true,
              .default_value = "500"},
    FieldSpec{.key = "fine_transfer",
              .label = "Штраф за передачу помещения третьим лицам (п. 5.1.6), руб.",
              .group = FINES,
              .hint = "2 000",
              .check = check_rubles,
              .required = true,
              .default_value = "2 000"},
    FieldSpec{.key = "fire_fine",
              .label = "Штраф за нарушение пожарной безопасности (п. 5.1.7), руб.",
              .group = FINES,
              .hint = "5 000",
              .check = check_rubles,
              .required = true,
              .default_value = "5 000"},
    FieldSpec{.key = "fine_waste",
              .label = "Штраф за отсутствие договора на вывоз отходов (п. 5.1.8), руб.",
              .group = FINES,
              .hint = "2 000",
              .check = check_rubles,
              .required = true,
              .default_value = "2 000"},
    FieldSpec{.key = "contract_limit",
              .label = "Наибольшая сумма одного договора, руб.",
              .group = TERMS,
              .hint = "100 000",
              .check = check_rubles,
              .required = true,
              .default_value = "100 000"},
    FieldSpec{.key = "lease_months",
              .label = "Общий срок аренды, мес. (делится на договоры)",
              .group = TERMS,
              .hint = "11",
              .check = check_lease_months,
              .required = true,
              .default_value = "11"},
};

} // namespace

Landlords::Landlords(Database &db) : m_store{db, "landlords", keys(SPECS), "name_short COLLATE NOCASE, id", "нет арендодателя"}, m_db{db} {}

std::span<FieldSpec const> Landlords::specs() { return SPECS; }

std::vector<Landlord> Landlords::list() const { return m_store.list(); }

Landlord Landlords::get(std::int64_t id) const { return m_store.get(id); }

std::int64_t Landlords::create(Fields const &fields) {
    Transaction tx{m_db};
    auto const id = m_store.insert(clean(fields, 0));
    tx.commit();
    return id;
}

void Landlords::update(std::int64_t id, Fields const &changes) {
    Transaction tx{m_db};
    auto values = get(id).fields;
    for (auto const &[key, value] : changes) {
        values[key] = value;
    }
    m_store.update(id, clean(values, id));
    tx.commit();
}

void Landlords::remove(std::int64_t id) {
    Transaction tx{m_db};
    auto const landlord = get(id);
    if (auto const n = Contracts::count_for(m_db, "landlord_id", id); n != 0) {
        throw UserError("у арендодателя «" + landlord.fields.at("name_short") + "» есть договоры (" + std::to_string(n) +
                        "), удалить нельзя: сначала удалите договоры");
    }
    m_store.remove(id);
    tx.commit();
}

Fields Landlords::clean(Fields const &fields, std::int64_t id) const {
    auto out = clean_fields(SPECS, fields);
    requisites::check_account_bik(out.at("account"), out.at("bik"));
    requisites::check_corr_account_bik(out.at("corr_account"), out.at("bik"));
    auto dup = m_db.prepare("SELECT count(*) FROM landlords WHERE name_short = ? AND id <> ?");
    dup.bind(1, out.at("name_short")).bind(2, id);
    dup.step();
    if (dup.column_int(0) != 0) {
        throw UserError("арендодатель «" + out.at("name_short") + "» уже есть");
    }
    return out;
}
