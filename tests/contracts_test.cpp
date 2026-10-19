#include "contracts.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "db.hpp"
#include "errors.hpp"
#include "landlords.hpp"
#include "schema.hpp"
#include "tenants.hpp"
#include "test_util.hpp"

namespace {

class ContractsTest : public testing::Test {
   protected:
    ContractsTest() {
        migrate(m_db);
        m_landlord = Landlords{m_db}.create({{"name_full", "Общество с ограниченной ответственностью «Ромашка»"}, {"name_short", "ООО «Ромашка»"}});
        m_ip = Tenants{m_db}.create(
            {{"kind", "ip"}, {"name", "Петров Пётр Петрович"}, {"gender", "m"}, {"inn", "500100732259"}, {"area", "12,5"}, {"floor", "2"}});
        m_org = Tenants{m_db}.create({{"kind", "org"}, {"name_full", "ООО «Берёза»"}, {"name_short", "ООО «Берёза»"}, {"inn", "7707083893"}});
        Equipment{m_db}.replace(m_ip, {{"Станок", "Silca", 55, 1}});
    }

    static std::vector<std::string> numbers_of(std::vector<Contract> const &list) {
        std::vector<std::string> out;
        out.reserve(list.size());
        for (auto const &c : list) {
            out.push_back(c.number + " " + c.start + ".." + c.end);
        }
        return out;
    }

    Database m_db{":memory:"};
    Contracts m_contracts{m_db};
    std::int64_t m_landlord = 0;
    std::int64_t m_ip = 0;
    std::int64_t m_org = 0;
};

} // namespace

/**
 * @brief Расчёт: номера «дата-ИНН», части по сумме, копии карточек и оборудования; ничего не записывается.
 */
TEST_F(ContractsTest, Plan) {
    auto const plan = m_contracts.plan(m_landlord, {{"2026-10-19", "20 000", m_ip}, {"2026-11-01", "9000", m_org}});
    EXPECT_THAT(numbers_of(plan),
                testing::ElementsAre("2026-10-19-500100732259 2026-10-19..2027-02-18", "2027-02-19-500100732259 2027-02-19..2027-06-18",
                                     "2027-06-19-500100732259 2027-06-19..2027-09-18", "2026-11-01-7707083893 2026-11-01..2027-09-30"));
    EXPECT_EQ(plan[0].rent_kop, 2'000'000);
    EXPECT_EQ(plan[0].months, 4);
    EXPECT_EQ(plan[0].tenant.at("floor"), "2") << "место копируется из карточки арендатора";
    EXPECT_EQ(plan[0].landlord.at("name_short"), "ООО «Ромашка»");
    ASSERT_EQ(plan[0].equipment.size(), 1U);
    EXPECT_EQ(plan[0].equipment[0].name, "Станок");
    EXPECT_TRUE(m_contracts.list().empty()) << "расчёт ничего не записывает";
}

/**
 * @brief Условия арендодателя меняют деление: порог и общий срок.
 */
TEST_F(ContractsTest, LandlordTerms) {
    Landlords{m_db}.update(m_landlord, {{"contract_limit", "50 000"}, {"lease_months", "6"}});
    auto const plan = m_contracts.plan(m_landlord, {{"2026-10-19", "20 000", m_ip}});
    EXPECT_THAT(numbers_of(plan),
                testing::ElementsAre("2026-10-19-500100732259 2026-10-19..2026-12-18", "2026-12-19-500100732259 2026-12-19..2027-02-18",
                                     "2027-02-19-500100732259 2027-02-19..2027-04-18"));
}

/**
 * @brief Создание записывает план; копии в договоре не меняются при правке карточек, пока не обновить.
 */
TEST_F(ContractsTest, CreateAndRefresh) {
    auto const ids = m_contracts.create(m_landlord, {{"2026-10-19", "9000", m_ip}});
    ASSERT_EQ(ids.size(), 1U);
    Tenants{m_db}.update(m_ip, {{"floor", "3"}});
    Equipment{m_db}.replace(m_ip, {});
    auto c = m_contracts.get(ids[0]);
    EXPECT_EQ(c.number, "2026-10-19-500100732259");
    EXPECT_EQ(c.tenant.at("floor"), "2") << "договор хранит копию на момент создания";
    EXPECT_EQ(c.equipment.size(), 1U);

    m_contracts.refresh(ids[0]);
    c = m_contracts.get(ids[0]);
    EXPECT_EQ(c.tenant.at("floor"), "3") << "после обновления из карточек";
    EXPECT_TRUE(c.equipment.empty());
    EXPECT_EQ(c.number, "2026-10-19-500100732259") << "номер и сроки при обновлении не меняются";
}

/**
 * @brief Отказы с номером строки и арендатором; при отказе ничего не записывается.
 */
TEST_F(ContractsTest, Rejects) {
    EXPECT_THROW_REASON((void)m_contracts.plan(m_landlord, {}), UserError, "добавьте хотя бы одного арендатора");
    EXPECT_THROW_REASON((void)m_contracts.plan(m_landlord, {{"2026-10-19", "0", m_ip}}), UserError,
                        "строка 1 (ИП Петров Пётр Петрович): плата в месяц должна быть больше нуля");
    EXPECT_THROW_REASON((void)m_contracts.plan(m_landlord, {{"2026-10-19", "9000", m_ip}, {"2026-13-01", "9000", m_org}}), UserError,
                        "строка 2 (ООО «Берёза»): нет такой даты");
    EXPECT_THROW_REASON((void)m_contracts.plan(m_landlord, {{"2026-10-19", "9000", 999}}), UserError, "строка 1: нет арендатора с id 999");
    EXPECT_THROW_REASON((void)m_contracts.plan(999, {{"2026-10-19", "9000", m_ip}}), NotFoundError, "нет арендодателя с id 999");
    EXPECT_THROW_REASON(m_contracts.create(m_landlord, {{"2026-10-19", "9000", m_ip}, {"2026-10-19", "5000", m_ip}}), UserError,
                        "договор 2026-10-19-500100732259 уже есть");
    EXPECT_TRUE(m_contracts.list().empty()) << "при ошибке во второй строке первая не записывается";

    m_contracts.create(m_landlord, {{"2026-10-19", "9000", m_ip}});
    EXPECT_THROW_REASON(m_contracts.create(m_landlord, {{"2026-10-19", "9000", m_ip}}), UserError, "уже есть");
}

/**
 * @brief Сторону с договорами удалить нельзя; после удаления договоров можно.
 */
TEST_F(ContractsTest, PartiesWithContractsNotRemoved) {
    auto const ids = m_contracts.create(m_landlord, {{"2026-10-19", "9000", m_ip}});
    EXPECT_THROW_REASON(Landlords{m_db}.remove(m_landlord), UserError, "у арендодателя «ООО «Ромашка»» есть договоры (1)");
    EXPECT_THROW_REASON(Tenants{m_db}.remove(m_ip), UserError, "у арендатора «ИП Петров Пётр Петрович» есть договоры (1)");
    m_contracts.remove(ids[0]);
    EXPECT_NO_THROW(Tenants{m_db}.remove(m_ip));
    EXPECT_NO_THROW(Landlords{m_db}.remove(m_landlord));
    EXPECT_THROW_REASON((void)m_contracts.get(ids[0]), NotFoundError, "нет договора");
}

/**
 * @brief Список: новые по дате начала первыми.
 */
TEST_F(ContractsTest, ListOrder) {
    m_contracts.create(m_landlord, {{"2026-10-19", "9000", m_ip}, {"2026-11-01", "9000", m_org}});
    auto const list = m_contracts.list();
    ASSERT_EQ(list.size(), 2U);
    EXPECT_EQ(list[0].start, "2026-11-01");
    EXPECT_EQ(list[1].start, "2026-10-19");
}
