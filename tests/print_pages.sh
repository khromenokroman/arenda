#!/bin/bash
# Проверка печатной формы через headless Chrome и PDF: договор занимает ровно MAX_PAGES страниц A4 (два листа
# с двух сторон) и с минимальными данными, и с длинными реквизитами и 10 строками оборудования. Тогда в пачке
# каждый договор начинается с нового листа и при двусторонней печати.
#
# Использование: tests/print_pages.sh <путь к arenda> <каталог web> [каталог для PDF]
# Нужны chromium или google-chrome, node (tests/e2e/drive.js) и pdfinfo (poppler-utils).
set -euo pipefail

BIN=$1
WEB=$2
OUT=${3:-}
PORT=${PORT:-18096}
MAX_PAGES=${MAX_PAGES:-4}
export CHROME=${CHROME:-$(command -v chromium || command -v google-chrome || true)}
DIR=$(cd "$(dirname "$0")" && pwd)
[ -n "$CHROME" ] || { echo "print_pages: не найден chromium или google-chrome" >&2; exit 2; }
command -v pdfinfo >/dev/null || { echo "print_pages: не найден pdfinfo (poppler-utils)" >&2; exit 2; }

TMP=$(mktemp -d)
"$BIN" --db "$TMP/print.db" --web "$WEB" --port "$PORT" >"$TMP/server.log" 2>&1 &
PID=$!
trap 'kill $PID 2>/dev/null || true; wait $PID 2>/dev/null || true; rm -rf "$TMP"' EXIT
for _ in $(seq 50); do
    curl -sf "http://127.0.0.1:$PORT/api/version" >/dev/null && break
    sleep 0.1
done
API=http://127.0.0.1:$PORT/api
ADMIN=$(curl -sf -D - -o /dev/null -X POST "$API/auth/setup" -H 'Content-Type: application/json' \
    -d '{"login":"admin","password":"secret1"}' | sed -n 's/^Set-Cookie: \([^;]*\).*/\1/Ip')
call() { curl -sf -X "$1" "$API/$2" -H 'Content-Type: application/json' -H "Cookie: $ADMIN" -d "$3"; }
id() { sed -n 's/.*"id":\([0-9]*\).*/\1/p'; }

LONG_ADDRESS="123456, Московская область, городской округ Красногорск, рабочий посёлок Нахабино, улица Красноармейская, дом 123, корпус 4, строение 5, помещение 67"
LANDLORD=$(call POST landlords "{\"name_full\":\"Общество с ограниченной ответственностью «Торгово-производственная компания Северо-Западный региональный продовольственный кооператив»\",
\"name_short\":\"ООО «ТПК СЗРПК»\",\"ogrn\":\"1027700132195\",\"inn\":\"7707083893\",\"kpp\":\"773601001\",\"address\":\"$LONG_ADDRESS\",
\"phone\":\"+7 (495) 000-00-00, +7 (900) 000-00-00\",\"signer_post\":\"Генеральный директор\",\"signer_post_gen\":\"генерального директора\",
\"signer_name\":\"Константинопольский Александр Владиславович\",\"signer_name_gen\":\"Константинопольского Александра Владиславовича\",
\"signer_basis_gen\":\"Устава\",\"account\":\"40702810538000123456\",\"bank\":\"ПАО «Сбербанк России», Северо-Западный банк\",\"bik\":\"044525225\",
\"object_name_gen\":\"торгово-развлекательного центра «Северное сияние»\",\"object_address\":\"$LONG_ADDRESS\",\"city\":\"г. Красногорск\",
\"region_gen\":\"Московской области\",\"municipality_gen\":\"городского округа Красногорск Московской области\"}" | id)
ORG=$(call POST tenants "{\"kind\":\"org\",\"name_full\":\"Общество с ограниченной ответственностью «Мастерская по ремонту обуви, изготовлению ключей и металлоремонту Золотые руки»\",
\"name_short\":\"ООО «Золотые руки»\",\"ogrn\":\"1027700132195\",\"inn\":\"7707083893\",\"kpp\":\"773601001\",\"address\":\"$LONG_ADDRESS\",
\"phone\":\"+7 (900) 111-22-33\",\"signer_post\":\"Генеральный директор\",\"signer_post_gen\":\"генерального директора\",
\"signer_name\":\"Христорождественская Анастасия-Мария Владиславовна\",\"signer_name_gen\":\"Христорождественской Анастасии-Марии Владиславовны\",
\"signer_basis_gen\":\"доверенности № 123 от 01.01.2026\",\"account\":\"40702810538000123456\",\"bank\":\"ПАО «Сбербанк России»\",\"bik\":\"044525225\",
\"area\":\"12,55\",\"trade_area\":\"10,25\",\"floor\":\"2\",
\"purpose\":\"оказания услуг по металлоремонту (изготовление ключей, ремонт мелких металлических изделий, заточка инструмента, ремонт обуви и сумок)\",
\"activity_gen\":\"металлоремонта и ремонта обуви\",\"premises_condition\":\"удовлетворительное, отделка стен и пола без повреждений, электропроводка исправна, пригодно для использования по назначению\"}" | id)
PERSON=$(call POST tenants "{\"kind\":\"person\",\"name\":\"Христорождественская Анастасия-Мария Владиславовна\",\"gender\":\"f\",\"birth_date\":\"1990-05-17\",
\"inn\":\"526317984689\",\"address\":\"$LONG_ADDRESS\",\"phone\":\"+7 (900) 111-22-33\",\"passport\":\"4510 123456\",
\"passport_issued\":\"Отделением УФМС России по Московской области по городскому округу Красногорск в рабочем посёлке Нахабино\",
\"passport_date\":\"2010-06-01\",\"passport_code\":\"500-123\",\"account\":\"40817810700000000001\",\"bank\":\"ПАО «Сбербанк России»\",\"bik\":\"044525225\",
\"area\":\"12,55\",\"trade_area\":\"10,25\",\"floor\":\"2\",
\"purpose\":\"оказания услуг по металлоремонту (изготовление ключей, ремонт мелких металлических изделий, заточка инструмента, ремонт обуви и сумок)\",
\"activity_gen\":\"металлоремонта и ремонта обуви\",\"premises_condition\":\"удовлетворительное, отделка стен и пола без повреждений, электропроводка исправна, пригодно для использования по назначению\"}" | id)
MIN_LANDLORD=$(call POST landlords '{"name_full":"ООО «А»","name_short":"А"}' | id)
MIN_TENANT=$(call POST tenants '{"kind":"ip","name":"Ли Ан","gender":"m","inn":"500100732259"}' | id)
MIN=$(call POST contracts "{\"landlord_id\":$MIN_LANDLORD,\"rows\":[{\"tenant_id\":$MIN_TENANT,\"start\":\"2026-10-19\",\"rent\":\"5000\"}]}" | id | head -1)
ROWS=""
for i in $(seq 10); do
    ROWS="$ROWS${ROWS:+,}{\"name\":\"Станок для изготовления ключей с электроприводом $i\",\"model\":\"Silca Bravo Professional $i\",\"power\":\"0,55\",\"qty\":2}"
done
call PUT "tenants/$ORG/equipment" "[$ROWS]" >/dev/null
call PUT "tenants/$PERSON/equipment" "[$ROWS]" >/dev/null
call POST contracts "{\"landlord_id\":$LANDLORD,\"rows\":[{\"tenant_id\":$ORG,\"start\":\"2026-10-19\",\"rent\":\"9 090,91\"},{\"tenant_id\":$PERSON,\"start\":\"2026-10-19\",\"rent\":\"9000\"}]}" >"$TMP/created.json"
IDS=$(grep -o '"id":[0-9]*' "$TMP/created.json" | cut -d: -f2 | paste -sd,)

# печать через CDP: страница требует входа, cookie ставит drive.js
pages() {
    COOKIE=$ADMIN node "$DIR/e2e/drive.js" "$1" --pdf "$2" >/dev/null 2>&1
    pdfinfo "$2" | sed -n 's/^Pages: *//p'
}

failed=0
check() {
    local name=$1 url=$2 expected=$3
    local n
    n=$(pages "$url" "$TMP/$name.pdf")
    [ -n "$OUT" ] && cp "$TMP/$name.pdf" "$OUT/"
    if [ "$n" -eq "$expected" ]; then
        echo "ok    $name: $n стр."
    else
        failed=1
        echo "FAIL  $name: $n стр., нужно ровно $expected"
    fi
}
FIRST=${IDS%%,*}
LAST=${IDS##*,}
COUNT=$(tr ',' '\n' <<<"$IDS" | wc -l)
check minimal "http://127.0.0.1:$PORT/print/contracts/$MIN" "$MAX_PAGES"
check org "http://127.0.0.1:$PORT/print/contracts/$FIRST" "$MAX_PAGES"
check person "http://127.0.0.1:$PORT/print/contracts/$LAST" "$MAX_PAGES"
check batch "http://127.0.0.1:$PORT/print/contracts?ids=$IDS" $((COUNT * MAX_PAGES))
exit $failed
