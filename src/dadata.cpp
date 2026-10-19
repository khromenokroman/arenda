#include "dadata.hpp"

#include <httplib.h>

#include <utility>

#include "errors.hpp"
#include "names.hpp"
#include "requisites.hpp"

namespace {

constexpr int HTTP_OK = 200;
constexpr int HTTP_UNAUTHORIZED = 401;
constexpr int HTTP_FORBIDDEN = 403;
constexpr int HTTP_TOO_MANY = 429;
constexpr time_t TIMEOUT_SECONDS = 5;

/// Строковое поле по пути из ключей; пустая строка, если его нет или там не строка (null).
std::string text_at(nlohmann::json const &j, std::initializer_list<char const *> path) {
    nlohmann::json const *node = &j;
    for (auto const *key : path) {
        if (!node->is_object() || !node->contains(key)) {
            return {};
        }
        node = &(*node)[key];
    }
    return node->is_string() ? node->get<std::string>() : std::string{};
}

/// Прямые кавычки "..." в ёлочки «...»: открывающая и закрывающая чередуются.
std::string guillemets(std::string const &text) {
    std::string out;
    bool open = true;
    for (auto const c : text) {
        if (c == '"') {
            out += open ? "«" : "»";
            open = !open;
        } else {
            out += c;
        }
    }
    return out;
}

/// «ООО «Название»» из формы собственности и названия; без формы - название с формой из DaData.
std::string legal_name(nlohmann::json const &data, char const *opf_kind, char const *name_kind, char const *with_opf) {
    auto const opf = text_at(data, {"opf", opf_kind});
    auto name = text_at(data, {"name", name_kind});
    if (name.empty()) {
        name = text_at(data, {"name", "full"});
    }
    if (opf.empty() || name.empty()) {
        return guillemets(text_at(data, {"name", with_opf}));
    }
    return opf + " «" + guillemets(name) + "»";
}

/// Схема, хост и порт из адреса (для httplib::Client) и путь после них.
std::pair<std::string, std::string> split_url(std::string const &url) {
    auto const scheme_end = url.find("://");
    auto const path_start = url.find('/', scheme_end == std::string::npos ? 0 : scheme_end + 3);
    if (path_start == std::string::npos) {
        return {url, ""};
    }
    return {url.substr(0, path_start), url.substr(path_start)};
}

} // namespace

DaData::DaData(std::string url, std::string key) : m_url{std::move(url)}, m_key{std::move(key)} {}

nlohmann::json DaData::find_party(std::string const &inn) const {
    if (inn.size() == 10) {
        requisites::check_inn_org(inn);
    } else {
        requisites::check_inn_person(inn);
    }
    return find_by_id("party", inn, "по ИНН " + inn);
}

nlohmann::json DaData::find_bank(std::string const &bik) const {
    if (bik.empty()) {
        throw UserError("не указан БИК");
    }
    requisites::check_bik(bik);
    return find_by_id("bank", bik, "по БИК " + bik);
}

nlohmann::json DaData::find_by_id(std::string_view method, std::string const &query, std::string const &what) const {
    if (m_key.empty()) {
        throw UserError("не задан ключ DaData: укажите API-ключ из личного кабинета dadata.ru в переменной окружения ARENDA_DADATA_KEY");
    }
    auto const [host, prefix] = split_url(m_url);
    httplib::Client cli{host};
    cli.set_connection_timeout(TIMEOUT_SECONDS);
    cli.set_read_timeout(TIMEOUT_SECONDS);
    nlohmann::json body = {{"query", query}};
    if (method == "party") {
        body["branch_type"] = "MAIN";
    }
    httplib::Headers const headers = {{"Authorization", "Token " + m_key}, {"Accept", "application/json"}};
    auto const res = cli.Post(prefix + "/findById/" + std::string(method), headers, body.dump(), "application/json");
    if (!res) {
        throw ExternalError("DaData не отвечает (" + httplib::to_string(res.error()) + "), заполните поля вручную");
    }
    if (res->status == HTTP_UNAUTHORIZED || res->status == HTTP_FORBIDDEN) {
        throw ExternalError("DaData отклонил ключ (HTTP " + std::to_string(res->status) + "): проверьте ARENDA_DADATA_KEY");
    }
    if (res->status == HTTP_TOO_MANY) {
        throw ExternalError("исчерпан лимит запросов DaData (HTTP 429), попробуйте позже");
    }
    if (res->status != HTTP_OK) {
        throw ExternalError("DaData ответил ошибкой HTTP " + std::to_string(res->status));
    }
    auto const j = nlohmann::json::parse(res->body, nullptr, false);
    if (j.is_discarded() || !j.contains("suggestions") || !j["suggestions"].is_array()) {
        throw ExternalError("DaData вернул ответ в неожиданном формате");
    }
    auto const &suggestions = j["suggestions"];
    if (suggestions.empty() || !suggestions[0].contains("data") || !suggestions[0]["data"].is_object()) {
        throw NotFoundError(what + " в DaData ничего не найдено");
    }
    return suggestions[0]["data"];
}

Fields party_fields(nlohmann::json const &data, std::string_view target) {
    auto const type = text_at(data, {"type"});
    auto const want_legal = target != "ip";
    if (want_legal && type != "LEGAL") {
        throw UserError("по этому ИНН зарегистрирован индивидуальный предприниматель, а нужна организация");
    }
    if (!want_legal && type != "INDIVIDUAL") {
        throw UserError("по этому ИНН зарегистрирована организация, а нужен индивидуальный предприниматель");
    }
    Fields out;
    out["inn"] = text_at(data, {"inn"});
    if (want_legal) {
        out["name_full"] = legal_name(data, "full", "full", "full_with_opf");
        out["name_short"] = legal_name(data, "short", "short", "short_with_opf");
        out["ogrn"] = text_at(data, {"ogrn"});
        out["kpp"] = text_at(data, {"kpp"});
        auto address = text_at(data, {"address", "unrestricted_value"});
        out["address"] = address.empty() ? text_at(data, {"address", "value"}) : address;
        auto const post = names::normalize_post(text_at(data, {"management", "post"}));
        auto const head = names::normalize_full_name(text_at(data, {"management", "name"}));
        // руководитель может быть организацией (управляющая компания): тогда подписанта заполняют вручную
        if (!post.empty() && !head.empty() && names::gender_by_patronymic(head) != 0) {
            out["signer_post"] = post;
            out["signer_post_gen"] = names::post_genitive(post);
            out["signer_name"] = head;
            out["signer_name_gen"] = names::full_name_genitive(head);
            out["signer_basis_gen"] = "Устава";
        }
        return out;
    }
    auto name = text_at(data, {"fio", "surname"}) + " " + text_at(data, {"fio", "name"}) + " " + text_at(data, {"fio", "patronymic"});
    name = names::normalize_full_name(name);
    if (name.empty()) {
        name = names::normalize_full_name(text_at(data, {"name", "full"}));
    }
    out["name"] = name;
    auto const gender = names::gender_by_patronymic(name);
    if (gender != 0) {
        out["gender"] = std::string(1, gender);
    }
    out["ogrnip"] = text_at(data, {"ogrn"});
    out["address"] = text_at(data, {"address", "value"});
    return out;
}

Fields bank_fields(nlohmann::json const &data) {
    auto name = text_at(data, {"name", "payment"});
    if (name.empty()) {
        name = text_at(data, {"value"});
    }
    return {{"bank", guillemets(name)}};
}
