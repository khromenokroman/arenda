#include "names.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace names {

namespace {

/// Декодирует UTF-8 в кодовые точки; неверный байт переносится как есть (U+0000..U+00FF).
std::u32string decode(std::string_view text) {
    std::u32string out;
    for (std::size_t i = 0; i < text.size();) {
        auto const c = static_cast<unsigned char>(text[i]);
        int extra = 0;
        char32_t cp = c;
        if (c >= 0xF0) {
            extra = 3;
            cp = c & 0x07U;
        } else if (c >= 0xE0) {
            extra = 2;
            cp = c & 0x0FU;
        } else if (c >= 0xC0) {
            extra = 1;
            cp = c & 0x1FU;
        }
        // обрезанная последовательность в конце строки: байт переносится как есть
        if (extra != 0 && i + static_cast<std::size_t>(extra) >= text.size()) {
            out.push_back(c);
            ++i;
            continue;
        }
        for (int k = 1; k <= extra; ++k) {
            cp = (cp << 6U) | (static_cast<unsigned char>(text[i + static_cast<std::size_t>(k)]) & 0x3FU);
        }
        out.push_back(cp);
        i += static_cast<std::size_t>(extra) + 1;
    }
    return out;
}

std::string encode(std::u32string_view text) {
    std::string out;
    for (auto const cp : text) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0U | (cp >> 6U)));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0U | (cp >> 12U)));
            out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        } else {
            out.push_back(static_cast<char>(0xF0U | (cp >> 18U)));
            out.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        }
    }
    return out;
}

constexpr char32_t UPPER_A = U'А';
constexpr char32_t UPPER_YA = U'Я';
constexpr char32_t LOWER_A = U'а';
constexpr char32_t LOWER_YA = U'я';
constexpr char32_t CASE_SHIFT = LOWER_A - UPPER_A;

char32_t lower(char32_t c) {
    if (c >= UPPER_A && c <= UPPER_YA) {
        return c + CASE_SHIFT;
    }
    if (c == U'Ё') {
        return U'ё';
    }
    if (c >= U'A' && c <= U'Z') {
        return c + (U'a' - U'A');
    }
    return c;
}

char32_t upper(char32_t c) {
    if (c >= LOWER_A && c <= LOWER_YA) {
        return c - CASE_SHIFT;
    }
    if (c == U'ё') {
        return U'Ё';
    }
    if (c >= U'a' && c <= U'z') {
        return c - (U'a' - U'A');
    }
    return c;
}

bool is_lower(char32_t c) { return lower(c) == c && upper(c) != c; }

std::u32string to_lower(std::u32string text) {
    std::ranges::transform(text, text.begin(), lower);
    return text;
}

/// Слово заглавными буквами («ИВАНОВ») в обычном виде («Иванов»); иначе как есть.
std::u32string normalize_word(std::u32string word) {
    if (word.size() > 1 && std::ranges::none_of(word, is_lower)) {
        word = to_lower(std::move(word));
        word[0] = upper(word[0]);
    }
    return word;
}

/// Окончание слова без учёта регистра.
bool ends(std::u32string const &word, std::u32string_view tail) {
    if (word.size() < tail.size()) {
        return false;
    }
    for (std::size_t i = 0; i < tail.size(); ++i) {
        if (lower(word[word.size() - tail.size() + i]) != tail[i]) {
            return false;
        }
    }
    return true;
}

/// Заменяет последние count букв на tail.
std::u32string replace_tail(std::u32string word, std::size_t count, std::u32string_view tail) {
    word.resize(word.size() - count);
    word += tail;
    return word;
}

/// Буква перед последними skip буквами (в нижнем регистре); 0, если её нет.
char32_t before_tail(std::u32string const &word, std::size_t skip) { return word.size() > skip ? lower(word[word.size() - skip - 1]) : 0; }

bool is_vowel(char32_t c) { return std::u32string_view{U"аеёиоуыэюя"}.find(lower(c)) != std::u32string_view::npos; }

/// После г, к, х, ж, ч, ш, щ пишется «и», а не «ы».
bool needs_i(char32_t c) { return std::u32string_view{U"гкхжчшщ"}.find(c) != std::u32string_view::npos; }

/// Существительное на -а/-я в родительном падеже: «Никита» -> «Никиты», «Ольга» -> «Ольги», «Мария» -> «Марии».
std::u32string noun_a_genitive(std::u32string const &word) {
    if (ends(word, U"а")) {
        return replace_tail(word, 1, needs_i(before_tail(word, 1)) ? U"и" : U"ы");
    }
    return replace_tail(word, 1, U"и");
}

/// Мужские имена с беглой гласной: правило «+а» дало бы «Лева», «Пётра».
constexpr std::array<std::pair<std::u32string_view, std::u32string_view>, 4> FLEETING = {{
    {U"лев", U"Льва"},
    {U"пётр", U"Петра"},
    {U"петр", U"Петра"},
    {U"павел", U"Павла"},
}};

/// Мужское существительное или имя: «Иван» -> «Ивана», «Игорь» -> «Игоря», «Андрей» -> «Андрея».
std::u32string male_noun_genitive(std::u32string const &word) {
    if (word.empty()) {
        return word;
    }
    auto const low = to_lower(word);
    for (auto const &[name, genitive] : FLEETING) {
        if (low == name) {
            return std::u32string(genitive);
        }
    }
    if (ends(word, U"а") || ends(word, U"я")) {
        return noun_a_genitive(word);
    }
    if (ends(word, U"ь") || ends(word, U"й")) {
        return replace_tail(word, 1, U"я");
    }
    if (is_vowel(word.back())) {
        return word;
    }
    return word + U"а";
}

/// Мужское прилагательное: «генеральный» -> «генерального», «Пузыревский» -> «Пузыревского», «Крайний» -> «Крайнего».
std::u32string adjective_genitive(std::u32string const &word) {
    if (ends(word, U"ый") || ends(word, U"ой")) {
        return replace_tail(word, 2, U"ого");
    }
    auto const c = before_tail(word, 2);
    return replace_tail(word, 2, (c == U'г' || c == U'к' || c == U'х') ? U"ого" : U"его");
}

bool is_adjective(std::u32string const &word) { return ends(word, U"ый") || ends(word, U"ий") || ends(word, U"ой"); }

std::u32string male_surname_genitive(std::u32string const &word) {
    if (is_adjective(word) && word.size() > 3) {
        return adjective_genitive(word);
    }
    if (ends(word, U"их") || ends(word, U"ых")) {
        return word;
    }
    return male_noun_genitive(word);
}

std::u32string female_surname_genitive(std::u32string const &word) {
    for (auto const *tail : {U"ова", U"ева", U"ёва", U"ина", U"ына"}) {
        if (ends(word, tail)) {
            return replace_tail(word, 1, U"ой");
        }
    }
    if (ends(word, U"яя")) {
        return replace_tail(word, 2, U"ей");
    }
    if (ends(word, U"ая")) {
        return replace_tail(word, 2, U"ой");
    }
    if (ends(word, U"а") || ends(word, U"я")) {
        return noun_a_genitive(word);
    }
    return word;
}

std::u32string female_name_genitive(std::u32string const &word) {
    if (ends(word, U"а") || ends(word, U"я")) {
        return noun_a_genitive(word);
    }
    if (ends(word, U"ь")) {
        return replace_tail(word, 1, U"и");
    }
    return word;
}

std::u32string patronymic_genitive(std::u32string const &word) {
    if (ends(word, U"ич")) {
        return word + U"а";
    }
    if (ends(word, U"на")) {
        return replace_tail(word, 1, U"ы");
    }
    return word;
}

/// Склоняет части фамилии через дефис по отдельности: «Римский-Корсаков» -> «Римского-Корсакова».
std::u32string surname_genitive(std::u32string const &word, char gender) {
    std::u32string out;
    std::size_t start = 0;
    while (start <= word.size()) {
        auto const end = std::min(word.find(U'-', start), word.size());
        auto const part = word.substr(start, end - start);
        out += gender == 'f' ? female_surname_genitive(part) : male_surname_genitive(part);
        if (end < word.size()) {
            out += U'-';
        }
        start = end + 1;
    }
    return out;
}

std::vector<std::u32string> split_words(std::string_view text) {
    std::vector<std::u32string> out;
    std::u32string word;
    for (auto const c : decode(text)) {
        if (c == U' ' || c == U'\t' || c == U'\n' || c == U'\r') {
            if (!word.empty()) {
                out.push_back(std::move(word));
                word.clear();
            }
        } else {
            word.push_back(c);
        }
    }
    if (!word.empty()) {
        out.push_back(std::move(word));
    }
    return out;
}

std::string join_words(std::vector<std::u32string> const &words) {
    std::u32string out;
    for (auto const &word : words) {
        if (!out.empty()) {
            out += U' ';
        }
        out += word;
    }
    return encode(out);
}

} // namespace

char gender_by_patronymic(std::string_view full_name) {
    auto const words = split_words(full_name);
    if (words.size() < 3) {
        return 0;
    }
    auto const &last = words.back();
    if (ends(last, U"ич") || ends(last, U"оглы") || ends(last, U"улы")) {
        return 'm';
    }
    if (ends(last, U"на") || ends(last, U"кызы")) {
        return 'f';
    }
    return 0;
}

std::string full_name_genitive(std::string_view full_name, char gender) {
    if (gender != 'm' && gender != 'f') {
        gender = gender_by_patronymic(full_name) == 'f' ? 'f' : 'm';
    }
    auto words = split_words(full_name);
    for (std::size_t i = 0; i < words.size(); ++i) {
        auto const word = normalize_word(words[i]);
        if (i == 0) {
            words[i] = surname_genitive(word, gender);
        } else if (i == 1) {
            words[i] = gender == 'f' ? female_name_genitive(word) : male_noun_genitive(word);
        } else {
            words[i] = patronymic_genitive(word);
        }
    }
    return join_words(words);
}

std::string normalize_post(std::string_view post) {
    auto text = decode(post);
    if (text.empty() || std::ranges::any_of(text, is_lower)) {
        return std::string(post);
    }
    text = to_lower(std::move(text));
    text[0] = upper(text[0]);
    return encode(text);
}

std::string post_genitive(std::string_view post) {
    auto words = split_words(post);
    for (auto &word : words) {
        auto const was_abbreviation = word.size() > 1 && std::ranges::none_of(word, is_lower);
        if (was_abbreviation) {
            continue;
        }
        if (is_adjective(word)) {
            word = adjective_genitive(word);
            continue;
        }
        word = male_noun_genitive(word);
        break;
    }
    if (!words.empty() && words[0].size() > 1 && is_lower(words[0][1])) {
        words[0][0] = lower(words[0][0]);
    }
    return join_words(words);
}

std::string normalize_full_name(std::string_view full_name) {
    auto words = split_words(full_name);
    for (auto &word : words) {
        word = normalize_word(std::move(word));
    }
    return join_words(words);
}

std::string initials_first(std::string_view full_name) {
    auto const words = split_words(full_name);
    if (words.empty()) {
        return {};
    }
    std::u32string out;
    for (std::size_t i = 1; i < words.size() && i < 3; ++i) {
        out += upper(words[i][0]);
        out += U". ";
    }
    out += normalize_word(words[0]);
    return encode(out);
}

} // namespace names
