#pragma once
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <vector>

#include "card_store.hpp"
#include "equipment.hpp"
#include "fields.hpp"

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

/// Описание полей для веб-формы: [{"key", "label", "group", "hint", "required", "input", "default", "choices": [{"value", "label"}]}, ...].
[[nodiscard]] nlohmann::json to_json(std::span<FieldSpec const> specs);

/// {"id": N, поля...}.
[[nodiscard]] nlohmann::json to_json(Card const &card);

/// Строка оборудования: {"name", "model", "power": "1,50", "qty": 2}; мощность строкой в кВт.
[[nodiscard]] nlohmann::json to_json(EquipmentRow const &row);

/**
 * @brief Перечень оборудования из [{"name", "model", "power": "1,5", "qty": 2}, ...].
 * @throws UserError не массив, неверные типы или мощность
 */
[[nodiscard]] std::vector<EquipmentRow> equipment_from_json(nlohmann::json const &j);
