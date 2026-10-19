#!/bin/bash
# Проверка страниц веб-интерфейса в headless-браузере.
# Запускает сервер на временной базе, заполняет её через API и открывает каждую страницу:
# страница должна нарисовать заголовок <h2>, не показать ошибку и не содержать текста «null» и «undefined».
#
# Использование: tests/web_smoke.sh <путь к arenda> <каталог web>
# Браузер берётся из $CHROME, иначе chromium или google-chrome; нужен node (tests/e2e/drive.js).
set -euo pipefail

BIN=$1
WEB=$2
PORT=${PORT:-18099}
export CHROME=${CHROME:-$(command -v chromium || command -v google-chrome || true)}
DIR=$(cd "$(dirname "$0")" && pwd)
if [ -z "$CHROME" ]; then
    echo "web_smoke: не найден chromium или google-chrome" >&2
    exit 2
fi

TMP=$(mktemp -d)
"$BIN" --db "$TMP/smoke.db" --web "$WEB" --port "$PORT" >"$TMP/server.log" 2>&1 &
PID=$!
trap 'kill $PID 2>/dev/null || true; wait $PID 2>/dev/null || true; rm -rf "$TMP"' EXIT
for _ in $(seq 50); do
    curl -sf "http://127.0.0.1:$PORT/api/version" >/dev/null && break
    sleep 0.1
done

API=http://127.0.0.1:$PORT/api
dump_as() { COOKIE=$1 node "$DIR/e2e/drive.js" "http://127.0.0.1:$PORT/#/$2" --dump 2>/dev/null || true; }
SETUP_DOM=$(dump_as "" contracts)
ADMIN=$(curl -sf -D - -o /dev/null -X POST "$API/auth/setup" -H 'Content-Type: application/json' \
    -d '{"login":"admin","name":"Админ","password":"secret1"}' | sed -n 's/^Set-Cookie: \([^;]*\).*/\1/Ip')
dump_page() { dump_as "$ADMIN" "$1"; }
call() { curl -sf -X "$1" "$API/$2" -H 'Content-Type: application/json' -H "Cookie: $ADMIN" -d "$3"; }
id() { sed -n 's/.*"id":\([0-9]*\).*/\1/p'; }

EMPTY_LIST=$(dump_page landlords)
EMPTY_TENANTS=$(dump_page tenants)
EMPTY_CONTRACTS=$(dump_page contracts)
EMPTY_NEW=$(dump_page contracts/new)
LANDLORD=$(call POST landlords '{"name_full":"Общество с ограниченной ответственностью «Ромашка»","name_short":"ООО «Ромашка» <b>","inn":"7707083893"}' | id)

IP=$(call POST tenants '{"kind":"ip","name":"Петров Пётр <b>","gender":"m","inn":"500100732259"}' | id)
ORG=$(call POST tenants '{"kind":"org","name_full":"ООО «Берёза»","name_short":"ООО «Берёза» <b>","inn":"7707083893"}' | id)
PERSON=$(call POST tenants '{"kind":"person","name":"Сидорова Анна <b>","gender":"f","birth_date":"1990-05-17","inn":"526317984689"}' | id)
call POST backups '' >/dev/null
call PUT "tenants/$IP/equipment" '[{"name":"Станок <b>","model":"Silca","power":"0,55","qty":1}]' >/dev/null
CONTRACT=$(call POST contracts "{\"landlord_id\":$LANDLORD,\"rows\":[{\"tenant_id\":$IP,\"start\":\"2026-10-19\",\"rent\":\"20 000\"},{\"tenant_id\":$ORG,\"start\":\"2026-11-01\",\"rent\":\"9000\"}]}" | id | head -1)

PAGES=(
    ""
    "landlords"
    "landlords/new"
    "landlords/$LANDLORD"
    "tenants"
    "tenants/new"
    "tenants/$IP"
    "tenants/$ORG"
    "tenants/$PERSON"
    "contracts"
    "contracts/new"
    "contracts/$CONTRACT"
    "users"
    "password"
    "backups"
)

failed=0
if grep -q 'Создать администратора' <<<"$SETUP_DOM"; then
    echo "ok    пустая база: форма первого администратора"
else
    failed=1
    echo "FAIL  пустая база: нет формы первого администратора"
fi
if grep -q '>Войти<' <<<"$(dump_as "" contracts)"; then
    echo "ok    без входа: форма входа"
else
    failed=1
    echo "FAIL  без входа: нет формы входа"
fi
check() {
    local page=$1 dom=$2
    main=$(tr -d '\n' <<<"$dom" | grep -o '<main.*</main>' || true)
    message=$(grep -oE '<div id="message"[^>]*>[^<]*' <<<"$dom" || true)
    problems=()
    grep -q '<h2' <<<"$main" || problems+=("страница не нарисована (нет <h2>)")
    grep -q 'hidden' <<<"$message" || problems+=("показана ошибка: ${message#*>}")
    grep -qE '>[^<]*(null|undefined)[^<]*<|value="(null|undefined)"' <<<"$main" && problems+=("в тексте есть null или undefined")
    grep -q '<b>' <<<"$main" && problems+=("текст из базы вставлен без экранирования")
    if [ ${#problems[@]} -eq 0 ]; then
        echo "ok    #/$page"
    else
        failed=1
        printf 'FAIL  #/%s: %s\n' "$page" "$(IFS=';'; echo "${problems[*]}")"
    fi
}
if grep -q 'Арендодателей пока нет' <<<"$EMPTY_LIST"; then
    echo "ok    пустая база: список арендодателей пуст"
else
    failed=1
    echo "FAIL  пустая база: нет надписи «Арендодателей пока нет»"
fi
if grep -q 'Арендаторов пока нет' <<<"$EMPTY_TENANTS"; then
    echo "ok    пустая база: список арендаторов пуст"
else
    failed=1
    echo "FAIL  пустая база: нет надписи «Арендаторов пока нет»"
fi
if grep -q 'Договоров пока нет' <<<"$EMPTY_CONTRACTS" && grep -q 'Сначала заведите' <<<"$EMPTY_NEW"; then
    echo "ok    пустая база: договоров нет, создание просит завести стороны"
else
    failed=1
    echo "FAIL  пустая база: нет надписи «Договоров пока нет» или «Сначала заведите»"
fi
for page in "${PAGES[@]}"; do
    check "$page" "$(dump_page "$page")"
done
exit $failed
