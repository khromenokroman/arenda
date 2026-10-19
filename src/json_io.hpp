#pragma once
#include <nlohmann/json.hpp>
#include <span>
#include <string>

#include "fields.hpp"
#include "landlords.hpp"

/**
 * @brief Разбирает тело запроса как JSON-объект.
 * @throws UserError если это не JSON или не объект
 */
[[nodiscard]] nlohmann::json parse_object(std::string const &body);

/**
 * @brief Поля карточки из JSON-объекта {"ключ": "значение", ...}; ключ id пропускается.
 * @throws UserError если значение не строка
 */
[[nodiscard]] Fields fields_from_json(nlohmann::json const &j);

/// Описание полей для веб-формы: [{"key", "label", "group", "hint", "required"}, ...].
[[nodiscard]] nlohmann::json to_json(std::span<FieldSpec const> specs);

/// {"id": N, поля...}.
[[nodiscard]] nlohmann::json to_json(Landlord const &landlord);
