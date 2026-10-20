#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <utility>

#include "fields.hpp"

/**
 * @brief Клиент API подсказок DaData: организация или ИП по ИНН, банк по БИК.
 *
 * Ключ передаётся в заголовке «Authorization: Token ...». Методы потокобезопасны: каждый запрос
 * создаёт своё соединение.
 */
class DaData {
   public:
    /// Адрес API подсказок по умолчанию.
    static constexpr std::string_view DEFAULT_URL = "https://suggestions.dadata.ru/suggestions/api/4_1/rs";

    DaData() = default;

    /**
     * @param url адрес API подсказок (схема, хост, порт и путь)
     * @param key API-ключ; пустой - запросы отклоняются с подсказкой, как его задать
     */
    DaData(std::string url, std::string key);

    /// Тот же адрес с другим ключом.
    [[nodiscard]] DaData with_key(std::string key) const { return DaData{m_url, std::move(key)}; }

    [[nodiscard]] std::string const &key() const { return m_key; }

    /**
     * @brief Данные основной организации или ИП по ИНН: поле data первой подсказки.
     * @throws UserError нет ключа; NotFoundError ничего не найдено; ExternalError сервис недоступен или отказал
     */
    [[nodiscard]] nlohmann::json find_party(std::string const &inn) const;

    /**
     * @brief Данные банка по БИК: поле data первой подсказки.
     * @throws UserError, NotFoundError, ExternalError как find_party
     */
    [[nodiscard]] nlohmann::json find_bank(std::string const &bik) const;

   private:
    /// POST {"query": query, ...extra} на метод findById/<method>; data первой подсказки.
    [[nodiscard]] nlohmann::json find_by_id(std::string_view method, std::string const &query, std::string const &what) const;

    std::string m_url{DEFAULT_URL};
    std::string m_key;
};

/**
 * @brief Поля карточки из данных DaData об организации или ИП.
 *
 * @param target для кого заполняем: "landlord" (арендодатель), "org" или "ip" (арендатор)
 * @throws UserError если по ИНН найдено не то: ИП вместо организации или наоборот
 * @return только поля, которые есть у target; родительный падеж - черновик по правилам names
 */
[[nodiscard]] Fields party_fields(nlohmann::json const &data, std::string_view target);

/// Поля банка из данных DaData: {"bank": наименование для платёжных документов, "corr_account": корр. счёт или пусто}.
[[nodiscard]] Fields bank_fields(nlohmann::json const &data);
