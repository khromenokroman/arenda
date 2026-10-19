#include "tenants.hpp"

#include <algorithm>
#include <array>

#include "dates.hpp"
#include "errors.hpp"
#include "numbers.hpp"
#include "requisites.hpp"

namespace {

constexpr std::string_view MAIN = "Основное";
constexpr std::string_view PASSPORT = "Паспорт";
constexpr std::string_view SIGNER = "Подписант";
constexpr std::string_view BANK = "Банк";
constexpr std::string_view PLACE = "Арендуемое место";

constexpr std::size_t LONG_TEXT = 1000;

constexpr std::array GENDERS = {Choice{.value = "m", .label = "мужской"}, Choice{.value = "f", .label = "женский"}};

constexpr FieldSpec PERSON_NAME{.key = "name", .label = "ФИО", .group = MAIN, .hint = "Иванов Иван Иванович", .required = true};
constexpr FieldSpec GENDER{.key = "gender", .label = "Пол", .group = MAIN, .hint = "", .choices = GENDERS, .input = Input::SELECT, .required = true};
constexpr FieldSpec PHONE{.key = "phone", .label = "Телефон", .group = MAIN, .hint = "+7 900 000-00-00"};
constexpr FieldSpec ACCOUNT{.key = "account", .label = "Расчётный счёт", .group = BANK, .hint = "20 цифр", .check = requisites::check_account};
constexpr FieldSpec BANK_NAME{.key = "bank", .label = "Банк", .group = BANK, .hint = "ПАО «Банк»"};
constexpr FieldSpec BIK{.key = "bik", .label = "БИК", .group = BANK, .hint = "9 цифр", .check = requisites::check_bik};

/// Площадь в кв. м: положительное число с двумя знаками после запятой.
void check_area(std::string const &text) {
    if (numbers::parse_hundredths(text, "площадь") <= 0) {
        throw UserError("площадь должна быть больше нуля: «" + text + "»");
    }
}

void check_floor(std::string const &text) { (void)numbers::parse_int(text, "этаж", MIN_FLOOR, MAX_FLOOR); }

constexpr FieldSpec AREA{.key = "area", .label = "Площадь по напольной плитке, кв. м", .group = PLACE, .hint = "12,5", .check = check_area};
constexpr FieldSpec TRADE_AREA{
    .key = "trade_area", .label = "Площадь для торговли (гл. 26.5 НК РФ), кв. м", .group = PLACE, .hint = "10", .check = check_area};
constexpr FieldSpec FLOOR{.key = "floor", .label = "Этаж", .group = PLACE, .hint = "1", .check = check_floor};
constexpr FieldSpec PURPOSE{.key = "purpose",
                            .label = "Назначение помещения (п. 1.3: «для ...»)",
                            .group = PLACE,
                            .hint = "оказания услуг по металлоремонту (изготовление ключей, ремонт мелких металлических изделий)",
                            .max_size = LONG_TEXT};
constexpr FieldSpec ACTIVITY{
    .key = "activity_gen", .label = "Вид деятельности коротко (п. 2.1.2: «необходимые для ...»)", .group = PLACE, .hint = "металлоремонта"};
constexpr FieldSpec CONDITION{.key = "premises_condition",
                              .label = "Состояние помещения при передаче",
                              .group = PLACE,
                              .hint = "удовлетворительное, пригодно для использования по назначению",
                              .max_size = LONG_TEXT};

constexpr std::array IP_SPECS = {
    PERSON_NAME,
    GENDER,
    FieldSpec{.key = "ogrnip", .label = "ОГРНИП", .group = MAIN, .hint = "15 цифр", .check = requisites::check_ogrnip},
    FieldSpec{.key = "inn", .label = "ИНН", .group = MAIN, .hint = "12 цифр", .check = requisites::check_inn_person, .required = true},
    FieldSpec{.key = "address", .label = "Адрес", .group = MAIN, .hint = "Московская обл., г. Химки, ул. Ленина, д. 1, кв. 1", .max_size = LONG_TEXT},
    PHONE,
    ACCOUNT,
    BANK_NAME,
    BIK,
    AREA,
    TRADE_AREA,
    FLOOR,
    PURPOSE,
    ACTIVITY,
    CONDITION,
};

constexpr std::array ORG_SPECS = {
    FieldSpec{.key = "name_full",
              .label = "Полное наименование",
              .group = MAIN,
              .hint = "Общество с ограниченной ответственностью «Ромашка»",
              .max_size = LONG_TEXT,
              .required = true},
    FieldSpec{.key = "name_short", .label = "Краткое наименование", .group = MAIN, .hint = "ООО «Ромашка»", .required = true},
    FieldSpec{.key = "ogrn", .label = "ОГРН", .group = MAIN, .hint = "13 цифр", .check = requisites::check_ogrn},
    FieldSpec{.key = "inn", .label = "ИНН", .group = MAIN, .hint = "10 цифр", .check = requisites::check_inn_org, .required = true},
    FieldSpec{.key = "kpp", .label = "КПП", .group = MAIN, .hint = "9 знаков", .check = requisites::check_kpp},
    FieldSpec{.key = "address", .label = "Адрес", .group = MAIN, .hint = "Московская обл., г. Химки, ул. Ленина, д. 1", .max_size = LONG_TEXT},
    PHONE,
    FieldSpec{.key = "signer_post", .label = "Должность", .group = SIGNER, .hint = "Директор"},
    FieldSpec{.key = "signer_post_gen", .label = "Должность в родительном падеже", .group = SIGNER, .hint = "директора"},
    FieldSpec{.key = "signer_name", .label = "ФИО", .group = SIGNER, .hint = "Иванов Иван Иванович"},
    FieldSpec{.key = "signer_name_gen", .label = "ФИО в родительном падеже", .group = SIGNER, .hint = "Иванова Ивана Ивановича"},
    FieldSpec{.key = "signer_basis_gen", .label = "Действует на основании (родительный падеж)", .group = SIGNER, .hint = "Устава"},
    ACCOUNT,
    BANK_NAME,
    BIK,
    AREA,
    TRADE_AREA,
    FLOOR,
    PURPOSE,
    ACTIVITY,
    CONDITION,
};

constexpr std::array PERSON_SPECS = {
    PERSON_NAME,
    GENDER,
    FieldSpec{.key = "birth_date", .label = "Дата рождения", .group = MAIN, .hint = "", .check = check_date, .input = Input::DATE},
    FieldSpec{.key = "inn", .label = "ИНН", .group = MAIN, .hint = "12 цифр", .check = requisites::check_inn_person, .required = true},
    FieldSpec{.key = "address",
              .label = "Адрес регистрации",
              .group = MAIN,
              .hint = "Московская обл., г. Химки, ул. Ленина, д. 1, кв. 1",
              .max_size = LONG_TEXT},
    PHONE,
    FieldSpec{.key = "passport", .label = "Серия и номер", .group = PASSPORT, .hint = "1234 567890", .check = requisites::check_passport},
    FieldSpec{
        .key = "passport_issued", .label = "Кем выдан", .group = PASSPORT, .hint = "ГУ МВД России по Московской области", .max_size = LONG_TEXT},
    FieldSpec{.key = "passport_date", .label = "Дата выдачи", .group = PASSPORT, .hint = "", .check = check_date, .input = Input::DATE},
    FieldSpec{.key = "passport_code", .label = "Код подразделения", .group = PASSPORT, .hint = "123-456", .check = requisites::check_passport_code},
    ACCOUNT,
    BANK_NAME,
    BIK,
    AREA,
    TRADE_AREA,
    FLOOR,
    PURPOSE,
    ACTIVITY,
    CONDITION,
};

constexpr std::array KINDS = {
    TenantKind{.code = "ip", .label = "Индивидуальный предприниматель", .specs = IP_SPECS},
    TenantKind{.code = "org", .label = "Организация", .specs = ORG_SPECS},
    TenantKind{.code = "person", .label = "Физическое лицо", .specs = PERSON_SPECS},
};

/// Колонки таблицы: kind и ключи полей всех видов без повторов, в порядке первого появления.
std::vector<std::string> columns() {
    std::vector<std::string> out{"kind"};
    for (auto const &kind : KINDS) {
        for (auto const &spec : kind.specs) {
            if (std::ranges::find(out, spec.key) == out.end()) {
                out.emplace_back(spec.key);
            }
        }
    }
    return out;
}

} // namespace

Tenants::Tenants(Database &db)
    : m_store{db, "tenants", columns(), "CASE WHEN kind = 'org' THEN name_short ELSE name END COLLATE NOCASE, id", "нет арендатора"}, m_db{db} {}

std::span<TenantKind const> Tenants::kinds() { return KINDS; }

TenantKind const &Tenants::kind(std::string_view code) {
    auto const *const it = std::ranges::find(KINDS, code, &TenantKind::code);
    if (it == KINDS.end()) {
        throw UserError("неизвестный вид арендатора «" + std::string(code) + "»: нужен ip, org или person");
    }
    return *it;
}

std::string Tenants::title(Tenant const &tenant) {
    auto const &kind = tenant.fields.at("kind");
    if (kind == "org") {
        return tenant.fields.at("name_short");
    }
    if (kind == "ip") {
        return "ИП " + tenant.fields.at("name");
    }
    return tenant.fields.at("name");
}

std::vector<Tenant> Tenants::list() const { return m_store.list(); }

Tenant Tenants::get(std::int64_t id) const { return m_store.get(id); }

std::int64_t Tenants::create(Fields const &fields) {
    auto const kind_it = fields.find("kind");
    if (kind_it == fields.end()) {
        throw UserError("не указан вид арендатора (kind)");
    }
    auto input = fields;
    input.erase("kind");
    Transaction tx{m_db};
    auto const clean_values = clean(kind_it->second, input);
    check_unique_inn(clean_values.at("inn"), 0);
    auto const id = m_store.insert(clean_values);
    tx.commit();
    return id;
}

void Tenants::update(std::int64_t id, Fields const &changes) {
    Transaction tx{m_db};
    auto const old = get(id).fields;
    auto const kind_it = changes.find("kind");
    auto const &kind_code = kind_it == changes.end() ? old.at("kind") : kind_it->second;
    // прежние значения только полей нового вида: поля чужого вида при смене вида очищаются
    Fields input;
    for (auto const &spec : kind(kind_code).specs) {
        input.insert_or_assign(std::string(spec.key), old.at(std::string(spec.key)));
    }
    for (auto const &[key, value] : changes) {
        if (key != "kind") {
            input.insert_or_assign(key, value);
        }
    }
    auto const clean_values = clean(kind_code, input);
    check_unique_inn(clean_values.at("inn"), id);
    m_store.update(id, clean_values);
    tx.commit();
}

void Tenants::remove(std::int64_t id) {
    Transaction tx{m_db};
    m_store.remove(id);
    tx.commit();
}

Fields Tenants::clean(std::string const &kind_code, Fields const &fields) {
    auto const &info = kind(kind_code);
    Fields out;
    try {
        out = clean_fields(info.specs, fields);
    } catch (UserError const &e) {
        throw UserError(std::string(info.label) + ": " + e.what());
    }
    requisites::check_account_bik(out.at("account"), out.at("bik"));
    out.emplace("kind", kind_code);
    return out;
}

void Tenants::check_unique_inn(std::string const &inn, std::int64_t id) const {
    auto stmt = m_db.prepare("SELECT id FROM tenants WHERE inn = ? AND id <> ?");
    stmt.bind(1, inn).bind(2, id);
    if (stmt.step()) {
        throw UserError("арендатор с ИНН " + inn + " уже есть: «" + title(get(stmt.column_int(0))) + "»");
    }
}
