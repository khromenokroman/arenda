#include "landlords.hpp"

#include <array>
#include <string>

#include "errors.hpp"
#include "requisites.hpp"

namespace {

constexpr std::string_view ORG = "Организация";
constexpr std::string_view SIGNER = "Подписант";
constexpr std::string_view BANK = "Банк";
constexpr std::string_view OBJECT = "Объект аренды";

constexpr std::size_t LONG_TEXT = 1000;

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
};

/// Список колонок полей через запятую в порядке SPECS.
std::string columns() {
    std::string out;
    for (auto const &spec : SPECS) {
        out += out.empty() ? "" : ", ";
        out += spec.key;
    }
    return out;
}

Landlord read(Statement const &stmt) {
    Landlord landlord;
    landlord.id = stmt.column_int(0);
    for (std::size_t i = 0; i < SPECS.size(); ++i) {
        landlord.fields.emplace(SPECS.at(i).key, stmt.column_text(static_cast<int>(i) + 1));
    }
    return landlord;
}

/// Привязывает значения полей к параметрам 1..N в порядке SPECS.
void bind_fields(Statement &stmt, Fields const &fields) {
    for (std::size_t i = 0; i < SPECS.size(); ++i) {
        stmt.bind(static_cast<int>(i) + 1, fields.at(std::string(SPECS.at(i).key)));
    }
}

} // namespace

Landlords::Landlords(Database &db) : m_db{db} {}

std::span<FieldSpec const> Landlords::specs() { return SPECS; }

std::vector<Landlord> Landlords::list() const {
    auto stmt = m_db.prepare("SELECT id, " + columns() + " FROM landlords ORDER BY name_short COLLATE NOCASE, id");
    std::vector<Landlord> out;
    while (stmt.step()) {
        out.push_back(read(stmt));
    }
    return out;
}

Landlord Landlords::get(std::int64_t id) const {
    auto stmt = m_db.prepare("SELECT id, " + columns() + " FROM landlords WHERE id = ?");
    stmt.bind(1, id);
    if (!stmt.step()) {
        throw NotFoundError("нет арендодателя с id " + std::to_string(id));
    }
    return read(stmt);
}

std::int64_t Landlords::create(Fields const &fields) {
    Transaction tx{m_db};
    auto const clean_values = clean(fields, 0);
    std::string params;
    for (std::size_t i = 0; i < SPECS.size(); ++i) {
        params += i == 0 ? "?" : ", ?";
    }
    auto stmt = m_db.prepare("INSERT INTO landlords (" + columns() + ") VALUES (" + params + ")");
    bind_fields(stmt, clean_values);
    stmt.step();
    auto const id = m_db.last_insert_id();
    tx.commit();
    return id;
}

void Landlords::update(std::int64_t id, Fields const &changes) {
    Transaction tx{m_db};
    auto values = get(id).fields;
    for (auto const &[key, value] : changes) {
        values[key] = value;
    }
    auto const clean_values = clean(values, id);
    std::string assignments;
    for (auto const &spec : SPECS) {
        assignments += assignments.empty() ? "" : ", ";
        assignments += std::string(spec.key) + " = ?";
    }
    auto stmt = m_db.prepare("UPDATE landlords SET " + assignments + " WHERE id = ?");
    bind_fields(stmt, clean_values);
    stmt.bind(static_cast<int>(SPECS.size()) + 1, id);
    stmt.step();
    tx.commit();
}

void Landlords::remove(std::int64_t id) {
    Transaction tx{m_db};
    (void)get(id);
    auto stmt = m_db.prepare("DELETE FROM landlords WHERE id = ?");
    stmt.bind(1, id);
    stmt.step();
    tx.commit();
}

Fields Landlords::clean(Fields const &fields, std::int64_t id) const {
    auto out = clean_fields(SPECS, fields);
    requisites::check_account_bik(out.at("account"), out.at("bik"));
    auto dup = m_db.prepare("SELECT count(*) FROM landlords WHERE name_short = ? AND id <> ?");
    dup.bind(1, out.at("name_short")).bind(2, id);
    dup.step();
    if (dup.column_int(0) != 0) {
        throw UserError("арендодатель «" + out.at("name_short") + "» уже есть");
    }
    return out;
}
