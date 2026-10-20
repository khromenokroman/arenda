#include "activities.hpp"

#include <array>

#include "errors.hpp"

namespace {

constexpr std::string_view MAIN = "Вид деятельности";
constexpr std::string_view TEXTS = "Тексты договора";

/// Поле арендатора в разделе формы справочника.
constexpr FieldSpec in_group(FieldSpec spec, std::string_view group) {
    spec.group = group;
    return spec;
}

constexpr std::array SPECS = {
    FieldSpec{.key = "name", .label = "Название", .group = MAIN, .hint = "Металлоремонт", .required = true},
    in_group(PURPOSE_SPEC, TEXTS),
    in_group(ACTIVITY_SPEC, TEXTS),
    in_group(CONDITION_SPEC, TEXTS),
};

} // namespace

Activities::Activities(Database &db) : m_store{db, "activities", keys(SPECS), "name COLLATE NOCASE, id", "нет вида деятельности"}, m_db{db} {}

std::span<FieldSpec const> Activities::specs() { return SPECS; }

std::vector<Activity> Activities::list() const { return m_store.list(); }

Activity Activities::get(std::int64_t id) const { return m_store.get(id); }

std::int64_t Activities::create(Fields const &fields) {
    Transaction tx{m_db};
    auto const id = m_store.insert(clean(fields, 0));
    tx.commit();
    return id;
}

void Activities::update(std::int64_t id, Fields const &changes) {
    Transaction tx{m_db};
    auto values = get(id).fields;
    for (auto const &[key, value] : changes) {
        values[key] = value;
    }
    m_store.update(id, clean(values, id));
    tx.commit();
}

void Activities::remove(std::int64_t id) { m_store.remove(id); }

Fields Activities::clean(Fields const &fields, std::int64_t id) const {
    auto out = clean_fields(SPECS, fields);
    auto dup = m_db.prepare("SELECT count(*) FROM activities WHERE name = ? AND id <> ?");
    dup.bind(1, out.at("name")).bind(2, id);
    dup.step();
    if (dup.column_int(0) != 0) {
        throw UserError("вид деятельности «" + out.at("name") + "» уже есть");
    }
    return out;
}
