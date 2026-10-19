#include "json_io.hpp"

#include "errors.hpp"

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
        out.push_back({{"key", spec.key}, {"label", spec.label}, {"group", spec.group}, {"hint", spec.hint}, {"required", spec.required}});
    }
    return out;
}

nlohmann::json to_json(Landlord const &landlord) {
    nlohmann::json out = {{"id", landlord.id}};
    for (auto const &[key, value] : landlord.fields) {
        out[key] = value;
    }
    return out;
}
