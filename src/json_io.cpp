#include "json_io.hpp"

#include "errors.hpp"
#include "numbers.hpp"
#include "tenants.hpp"

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

nlohmann::json to_json(Contract const &contract) {
    auto equipment = nlohmann::json::array();
    for (auto const &row : contract.equipment) {
        equipment.push_back(to_json(row));
    }
    Tenant const tenant{contract.tenant, contract.tenant_id};
    return {{"id", contract.id},
            {"number", contract.number},
            {"landlord_id", contract.landlord_id},
            {"tenant_id", contract.tenant_id},
            {"start", contract.start},
            {"end", contract.end},
            {"months", contract.months},
            {"rent", numbers::format_hundredths(contract.rent_kop)},
            {"rent_kop", contract.rent_kop},
            {"total", numbers::format_hundredths(contract.rent_kop * contract.months)},
            {"landlord_title", contract.landlord.at("name_short")},
            {"tenant_title", Tenants::title(tenant)},
            {"kind_label", Tenants::kind(contract.tenant.at("kind")).label},
            {"landlord", contract.landlord},
            {"tenant", contract.tenant},
            {"equipment", equipment},
            {"total_power", numbers::format_hundredths(total_power(contract.equipment))}};
}

std::pair<std::int64_t, std::vector<ContractRequest>> contract_requests_from_json(nlohmann::json const &j) {
    if (!j.contains("landlord_id") || !j["landlord_id"].is_number_integer()) {
        throw UserError("выберите арендодателя (landlord_id)");
    }
    if (!j.contains("rows") || !j["rows"].is_array()) {
        throw UserError("rows должно быть массивом строк");
    }
    std::vector<ContractRequest> rows;
    for (std::size_t i = 0; i < j["rows"].size(); ++i) {
        auto const &item = j["rows"][i];
        auto const line = "строка " + std::to_string(i + 1) + ": ";
        if (!item.is_object() || !item.contains("tenant_id") || !item["tenant_id"].is_number_integer()) {
            throw UserError(line + "выберите арендатора");
        }
        if (!item.contains("start") || !item["start"].is_string()) {
            throw UserError(line + "укажите дату начала");
        }
        if (!item.contains("rent") || !item["rent"].is_string()) {
            throw UserError(line + "укажите плату в месяц строкой, например \"20 000\"");
        }
        rows.push_back({item["start"].get<std::string>(), item["rent"].get<std::string>(), item["tenant_id"].get<std::int64_t>()});
    }
    return {j["landlord_id"].get<std::int64_t>(), std::move(rows)};
}
