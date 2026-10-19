#include "fields.hpp"

#include <algorithm>

#include "errors.hpp"

std::string trim(std::string_view text) {
    constexpr std::string_view SPACES = " \t\r\n";
    auto const first = text.find_first_not_of(SPACES);
    if (first == std::string_view::npos) {
        return {};
    }
    return std::string(text.substr(first, text.find_last_not_of(SPACES) - first + 1));
}

Fields clean_fields(std::span<FieldSpec const> specs, Fields const &values) {
    for (auto const &[key, value] : values) {
        if (std::ranges::none_of(specs, [&key](FieldSpec const &spec) { return spec.key == key; })) {
            throw UserError("неизвестное поле: " + key);
        }
    }
    Fields out;
    for (auto const &spec : specs) {
        auto const it = values.find(spec.key);
        auto value = it == values.end() ? std::string{} : trim(it->second);
        std::string const label{spec.label};
        if (value.empty() && spec.required) {
            throw UserError("не заполнено поле «" + label + "»");
        }
        if (value.size() > spec.max_size) {
            throw UserError("поле «" + label + "» длиннее " + std::to_string(spec.max_size) + " байт");
        }
        if (!value.empty() && spec.check != nullptr) {
            spec.check(value);
        }
        out.emplace(spec.key, std::move(value));
    }
    return out;
}

std::vector<std::string> keys(std::span<FieldSpec const> specs) {
    std::vector<std::string> out;
    out.reserve(specs.size());
    for (auto const &spec : specs) {
        out.emplace_back(spec.key);
    }
    return out;
}
