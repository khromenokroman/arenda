#!/bin/bash
# Сценарии с кликами в headless Chrome (tests/e2e/*.js через tests/e2e/drive.js, нужен node).
# Каждый сценарий получает свежую пустую базу; DaData подменяется tests/e2e/fake_dadata.js (ключ good).
#
# Использование: tests/web_e2e.sh <путь к arenda> <каталог web>
# E2E_ONLY=landlords.js - только этот сценарий.
set -euo pipefail

BIN=$1
WEB=$2
PORT=${PORT:-18098}
DADATA_PORT=${DADATA_PORT:-18097}
DIR=$(cd "$(dirname "$0")" && pwd)
export CHROME=${CHROME:-$(command -v chromium || command -v google-chrome || true)}
[ -n "$CHROME" ] || { echo "web_e2e: не найден chromium или google-chrome" >&2; exit 2; }

# сценарий и адрес, с которого он начинается
SCENARIOS=(
    "landlords.js|#/landlords"
    "tenants.js|#/tenants/new"
    "lookup.js|#/landlords/new"
)

node "$DIR/e2e/fake_dadata.js" "$DADATA_PORT" &
DADATA_PID=$!
trap 'kill $DADATA_PID 2>/dev/null || true' EXIT

failed=0
for entry in "${SCENARIOS[@]}"; do
    script=${entry%%|*}
    hash=${entry#*|}
    [ -n "${E2E_ONLY:-}" ] && [ "$script" != "$E2E_ONLY" ] && continue
    TMP=$(mktemp -d)
    ARENDA_DADATA_KEY=good "$BIN" --db "$TMP/e2e.db" --web "$WEB" --port "$PORT" \
        --dadata-url "http://127.0.0.1:$DADATA_PORT/rs" >"$TMP/server.log" 2>&1 &
    PID=$!
    for _ in $(seq 50); do
        curl -sf "http://127.0.0.1:$PORT/api/version" >/dev/null && break
        sleep 0.1
    done
    if node "$DIR/e2e/drive.js" "http://127.0.0.1:$PORT/$hash" "$DIR/e2e/$script"; then
        echo "ok    $script"
    else
        echo "FAIL  $script"
        failed=1
    fi
    kill $PID; wait $PID 2>/dev/null || true
    rm -rf "$TMP"
done
exit $failed
