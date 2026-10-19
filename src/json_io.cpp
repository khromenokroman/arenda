#include "json_io.hpp"

#include "errors.hpp"
#include "numbers.hpp"

nlohmann::json parse_object(std::string const &body) {
    auto j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded()) {
        throw UserError("тело запроса не JSON");
    }
    if (!j.is_object()) {
        throw UserError("тело запроса должно быть JSON-объектом");
    }
    return j;
}

Fields fields_from_json(nlohmann::json const &j) {
    Fields out;
    for (auto const &[key, value] : j.items()) {
        if (key == "id") {
            continue;
        }
        if (!value.is_string()) {
            throw UserError("поле " + key + " должно быть строкой");
        }
        out.emplace(key, value.get<std::string>());
    }
    return out;
}

nlohmann::json to_json(std::span<FieldSpec const> specs) {
    auto out = nlohmann::json::array();
    for (auto const &spec : specs) {
        auto choices = nlohmann::json::array();
        for (auto const &choice : spec.choices) {
            choices.push_back({{"value", choice.value}, {"label", choice.label}});
        }
        out.push_back({{"key", spec.key},
                       {"label", spec.label},
                       {"group", spec.group},
                       {"hint", spec.hint},
                       {"required", spec.required},
                       {"input", to_string(spec.input)},
                       {"default", spec.default_value},
                       {"choices", choices}});
    }
    return out;
}

nlohmann::json to_json(Card const &card) {
    nlohmann::json out = {{"id", card.id}};
    for (auto const &[key, value] : card.fields) {
        out[key] = value;
    }
    return out;
}

nlohmann::json to_json(EquipmentRow const &row) {
    return {{"name", row.name}, {"model", row.model}, {"power", numbers::format_hundredths(row.power_c)}, {"qty", row.qty}};
}

std::vector<EquipmentRow> equipment_from_json(nlohmann::json const &j) {
    if (!j.is_array()) {
        throw UserError("перечень оборудования должен быть JSON-массивом");
    }
    std::vector<EquipmentRow> out;
    for (std::size_t i = 0; i < j.size(); ++i) {
        auto const &item = j[i];
        auto const line = "оборудование, строка " + std::to_string(i + 1) + ": ";
        if (!item.is_object()) {
            throw UserError(line + "нужен объект");
        }
        EquipmentRow row;
        auto const text = [&](char const *key) -> std::string {
            if (!item.contains(key)) {
                return {};
            }
            if (!item[key].is_string()) {
                throw UserError(line + key + " должно быть строкой");
            }
            return item[key].get<std::string>();
        };
        row.name = text("name");
        row.model = text("model");
        auto const power = text("power");
        row.power_c = numbers::parse_hundredths(power, line + "мощность, кВт");
        if (item.contains("qty")) {
            if (!item["qty"].is_number_integer()) {
                throw UserError(line + "qty должно быть целым числом");
            }
            row.qty = item["qty"].get<int>();
        }
        out.push_back(std::move(row));
    }
    return out;
}
