#pragma once
#include <string>

/**
 * @brief Проверки реквизитов. Пустая строка допустима везде: незаполненный реквизит печатается прочерком.
 *
 * Все функции бросают UserError с текстом для пользователя: что за реквизит, какое значение и что с ним не так.
 */
namespace requisites {

/// ИНН организации: 10 цифр, контрольная цифра.
void check_inn_org(std::string const &inn);
/// ИНН физического лица или ИП: 12 цифр, две контрольные цифры.
void check_inn_person(std::string const &inn);
/// ОГРН организации: 13 цифр, контрольная цифра (остаток от деления на 11).
void check_ogrn(std::string const &ogrn);
/// ОГРНИП: 15 цифр, контрольная цифра (остаток от деления на 13).
void check_ogrnip(std::string const &ogrnip);
/// КПП: 9 знаков, 4 цифры, 2 цифры или заглавные латинские буквы, 3 цифры.
void check_kpp(std::string const &kpp);
/// БИК: 9 цифр.
void check_bik(std::string const &bik);
/// Расчётный счёт: 20 цифр.
void check_account(std::string const &account);
/**
 * @brief Контрольный ключ расчётного счёта вместе с БИК банка (три последние цифры БИК + счёт).
 *
 * Проверяется, только если заполнены оба реквизита.
 */
void check_account_bik(std::string const &account, std::string const &bik);

/// Серия и номер паспорта РФ: «1234 567890».
void check_passport(std::string const &passport);
/// Код подразделения, выдавшего паспорт: «123-456».
void check_passport_code(std::string const &code);

} // namespace requisites
