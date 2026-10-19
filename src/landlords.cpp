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
    m_store.remove(id);
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
