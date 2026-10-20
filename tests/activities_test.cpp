#include "activities.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "db.hpp"
#include "errors.hpp"
#include "schema.hpp"
#include "tenants.hpp"
#include "test_util.hpp"

namespace {

/// Вид деятельности со всеми текстами.
Fields sample() {
    return {
        {"name", "Металлоремонт"},
        {"purpose", "оказания услуг по металлоремонту"},
        {"activity_gen", "металлоремонта"},
        {"premises_condition", "удовлетворительное"},
    };
}

class ActivitiesTest : public testing::Test {
   protected:
    ActivitiesTest() { migrate(m_db); }

    Database m_db{":memory:"};
    Activities m_activities{m_db};
};

} // namespace

/**
 * @brief Поля справочника те же, что у арендатора: ключи и ограничения длины совпадают, иначе текст не скопируется.
 */
TEST(Activities, SpecsMatchTenantFields) {
    auto const specs = Activities::specs();
    ASSERT_EQ(specs.size(), 4U);
    EXPECT_EQ(specs[0].key, "name");
    for (auto const &kind : Tenants::kinds()) {
        for (auto const &spec : specs.subspan(1)) {
            SCOPED_TRACE(std::string(kind.code) + ": " + std::string(spec.key));
            auto const it = std::ranges::find(kind.specs, spec.key, &FieldSpec::key);
            ASSERT_NE(it, kind.specs.end()) << "у арендатора нет такого поля";
            EXPECT_EQ(it->max_size, spec.max_size);
            EXPECT_EQ(it->label, spec.label);
        }
    }
}

/**
 * @brief Создание, список по названию, изменение одного поля, удаление.
 */
TEST_F(ActivitiesTest, Crud) {
    auto const id = m_activities.create(sample());
    m_activities.create({{"name", "Автозапчасти"}, {"activity_gen", "торговли автозапчастями"}});
    auto const list = m_activities.list();
    ASSERT_EQ(list.size(), 2U);
    EXPECT_EQ(list[0].fields.at("name"), "Автозапчасти") << "по названию";
    EXPECT_EQ(list[1].fields.at("purpose"), "оказания услуг по металлоремонту");

    m_activities.update(id, {{"premises_condition", "хорошее"}});
    auto const got = m_activities.get(id).fields;
    EXPECT_EQ(got.at("premises_condition"), "хорошее");
    EXPECT_EQ(got.at("activity_gen"), "металлоремонта") << "непереданные поля не меняются";

    m_activities.remove(id);
    EXPECT_EQ(m_activities.list().size(), 1U);
    EXPECT_THROW_REASON((void)m_activities.get(id), NotFoundError, "нет вида деятельности");
    EXPECT_THROW_REASON(m_activities.remove(id), NotFoundError, "нет вида деятельности");
}

/**
 * @brief Отказы: без названия, повтор названия, слишком длинный текст, чужое поле.
 */
TEST_F(ActivitiesTest, Rejects) {
    auto f = sample();
    f["name"] = "  ";
    EXPECT_THROW_REASON(m_activities.create(f), UserError, "«Название»");

    auto const id = m_activities.create(sample());
    f = sample();
    f["name"] = " Металлоремонт ";
    EXPECT_THROW_REASON(m_activities.create(f), UserError, "вид деятельности «Металлоремонт» уже есть");
    EXPECT_NO_THROW(m_activities.update(id, {{"name", "Металлоремонт"}})) << "своё же название не дубликат";

    f = sample();
    f["name"] = "Другое";
    f["purpose"] = std::string(1001, 'x');
    EXPECT_THROW_REASON(m_activities.create(f), UserError, "длиннее 1000 байт");

    f = sample();
    f["name"] = "Другое";
    f["inn"] = "7707083893";
    EXPECT_THROW_REASON(m_activities.create(f), UserError, "inn");
    EXPECT_EQ(m_activities.list().size(), 1U) << "отклонённые не сохраняются";
}
