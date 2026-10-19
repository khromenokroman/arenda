#pragma once
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "contracts.hpp"

/**
 * @brief Печатная форма договоров: HTML-страница A4 по шаблону templates/contract.html.
 */
namespace contract_print {

/// Значения меток шаблона: текст (экранируется, пустой - прочерк для заполнения от руки) и готовый HTML.
struct Values {
    std::map<std::string, std::string, std::less<>> text;
    std::map<std::string, std::string, std::less<>> html;
};

/**
 * @brief Подставляет значения в шаблон: {{имя}} - текст, {{{имя}}} - HTML.
 * @throws std::logic_error метка без значения (ошибка в шаблоне или коде, не в данных)
 */
[[nodiscard]] std::string fill(std::string_view tmpl, Values const &values);

/// Значения меток одного договора.
[[nodiscard]] Values values(Contract const &contract);

/**
 * @brief Страница с договорами по порядку: каждый с нового листа (нечётной страницы).
 * @throws UserError пустой список
 */
[[nodiscard]] std::string render(std::vector<Contract> const &contracts);

} // namespace contract_print
