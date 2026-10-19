#!/bin/bash
# Сценарий внутри контейнера с systemd (запускает tests/deb_install.sh): установка пакета из /deb,
# служба, вход, права файла настроек, ключ DaData из настроек, обновление с сохранением настроек и данных,
# выключенная служба, remove, purge, повторная установка.
set -u
fail=0
ok() { echo "ok    $*"; }
bad() { echo "FAIL  $*"; fail=1; }
wait_http() { for _ in $(seq 50); do curl -sf "http://127.0.0.1:$1/api/version" >/dev/null && return 0; sleep 0.2; done; return 1; }
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null
apt-get install -y -qq /deb/*.deb >/dev/null 2>&1 || bad "apt install"
systemctl is-active -q arenda && ok "служба запущена" || { bad "служба не запущена"; journalctl -u arenda --no-pager | tail -5; }
systemctl is-enabled -q arenda && ok "служба включена" || bad "служба не включена"
wait_http 8080 && ok "отвечает на 8080: $(curl -s http://127.0.0.1:8080/api/version)" || bad "нет ответа на 8080"
ss -ltn | grep -q '0.0.0.0:8080' && ok "слушает 0.0.0.0:8080" || bad "адрес: $(ss -ltn | grep 8080)"
[ "$(systemctl show -p User --value arenda)" = arenda ] && ok "под пользователем arenda" || bad "пользователь службы"
[ "$(stat -c %U:%a /var/lib/arenda)" = arenda:750 ] && ok "/var/lib/arenda arenda 750" || bad "права каталога: $(stat -c %U:%a /var/lib/arenda)"
[ "$(stat -c %U:%a /etc/default/arenda)" = root:640 ] && ok "/etc/default/arenda root 640 (ключ DaData)" || bad "права настроек: $(stat -c %U:%a /etc/default/arenda)"
curl -sf http://127.0.0.1:8080/ | grep -q '<title>' && ok "веб-страница отдаётся" || bad "нет страницы"
curl -sf http://127.0.0.1:8080/api/auth | grep -q '"setup":true' && ok "новая база ждёт администратора" || bad "нет первого запуска"
[ "$(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:8080/api/landlords)" = 401 ] && ok "без входа данные закрыты" || bad "данные без входа"
# сессия хранится в базе, поэтому cookie действует и после обновлений и переустановки
COOKIE=$(curl -sf -D - -o /dev/null -X POST http://127.0.0.1:8080/api/auth/setup -H 'Content-Type: application/json' \
    -d '{"login":"admin","password":"secret1"}' | sed -n 's/^Set-Cookie: \([^;]*\).*/\1/Ip')
[ -n "$COOKIE" ] && ok "создан администратор" || bad "не создан администратор"
curl -s -H "Cookie: $COOKIE" 'http://127.0.0.1:8080/api/lookup/bank?bik=044525225' | grep -q ARENDA_DADATA_KEY \
    && ok "без ключа DaData понятная ошибка" || bad "без ключа: $(curl -s -H "Cookie: $COOKIE" 'http://127.0.0.1:8080/api/lookup/bank?bik=044525225')"

curl -sf -X POST http://127.0.0.1:8080/api/landlords -H 'Content-Type: application/json' -H "Cookie: $COOKIE" \
    -d '{"name_full":"ООО «Ромашка»","name_short":"Ромашка"}' >/dev/null && ok "запись в базу" || bad "запись в базу"
landlords() { curl -s -H "Cookie: $COOKIE" "http://127.0.0.1:$1/api/landlords"; }

sed -i 's/--port 8080/--port 8081/; s/^ARENDA_DADATA_KEY=""/ARENDA_DADATA_KEY="test-key"/' /etc/default/arenda
apt-get install -y -qq --reinstall /deb/*.deb >/dev/null 2>&1 || bad "переустановка"
grep -q 8081 /etc/default/arenda && grep -q test-key /etc/default/arenda && ok "настройки сохранены при обновлении" || bad "настройки затёрты"
[ "$(stat -c %a /etc/default/arenda)" = 640 ] && ok "права настроек после обновления 640" || bad "права после обновления: $(stat -c %a /etc/default/arenda)"
wait_http 8081 && ok "после обновления перезапущена на 8081" || bad "нет ответа на 8081"
[ "$(landlords 8081 | grep -c Ромашка)" = 1 ] && ok "данные на месте после обновления" || bad "данные пропали"
# ключ из настроек дошёл до службы: ошибка уже не «не задан ключ» (DaData отклонит ключ или недоступен - 502)
curl -s -H "Cookie: $COOKIE" 'http://127.0.0.1:8081/api/lookup/bank?bik=044525225' | grep -q 'не задан ключ' && bad "ключ DaData из настроек не передан" \
    || ok "ключ DaData передан службе"

systemctl disable -q --now arenda
apt-get install -y -qq --reinstall /deb/*.deb >/dev/null 2>&1
systemctl is-enabled -q arenda && bad "выключенная служба включилась при обновлении" || ok "выключенная администратором остаётся выключенной"
systemctl enable -q --now arenda

apt-get remove -y -qq arenda >/dev/null 2>&1 || bad "remove"
systemctl is-active -q arenda && bad "служба работает после удаления" || ok "remove: служба остановлена"
[ -f /var/lib/arenda/arenda.db ] && ok "remove: база осталась" || bad "remove: база удалена"
apt-get purge -y -qq arenda >/dev/null 2>&1 || bad "purge"
[ ! -e /etc/default/arenda ] && ok "purge: настройки удалены" || bad "purge: настройки остались"
[ -f /var/lib/arenda/arenda.db ] && ok "purge: база осталась" || bad "purge: база удалена"

apt-get install -y -qq /deb/*.deb >/dev/null 2>&1 || bad "повторная установка"
wait_http 8080 && [ "$(landlords 8080 | grep -c Ромашка)" = 1 ] && ok "повторная установка: служба и прежние данные" || bad "повторная установка"
exit $fail
