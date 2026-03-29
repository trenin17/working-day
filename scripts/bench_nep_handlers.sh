#!/usr/bin/env bash
# Замер HTTP-ручек C++ (userver): генерация ключей, nep-sign, nep-verify.
#
# Переменные:
#   NEP_BENCH_BASE_URL     — API (по умолчанию http://127.0.0.1:8080)
#   NEP_BENCH_PGURI или PGHOST/...
#   NEP_BENCH_ITERATIONS, NEP_BENCH_KEYS_ITERATIONS (для generate — см. ниже),
#   NEP_BENCH_SIGNATURE_PASSWORD, NEP_BENCH_S3_BUCKET (опционально)
#   NEP_BENCH_VERBOSE=1    — доп. проверочные SELECT в БД после вставки
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BASE="${NEP_BENCH_BASE_URL:-http://127.0.0.1:8080}"
ITER="${NEP_BENCH_ITERATIONS:-7}"
PASS="${NEP_BENCH_SIGNATURE_PASSWORD:-123456}"
S3_BUCKET="${NEP_BENCH_S3_BUCKET:-working-day-documents}"

VPY="${ROOT}/python_service/.venv/bin/python"
if [[ ! -x "$VPY" ]]; then
  echo "Нужен venv: python_service/.venv (например: make test-nep-unit)" >&2
  exit 1
fi

if [[ -z "${NEP_BENCH_PGURI:-}" && -z "${PGHOST:-}" ]]; then
  echo "Задай NEP_BENCH_PGURI или PGHOST (+ PGDATABASE и т.д.)." >&2
  exit 1
fi

if [[ -n "${NEP_BENCH_PGURI:-}" ]]; then
  PSQL=(psql "$NEP_BENCH_PGURI")
else
  export PGPASSWORD="${PGPASSWORD:-}"
  PSQL=(psql -h "${PGHOST:-localhost}" -p "${PGPORT:-5432}" -U "${PGUSER:-postgres}" -d "${PGDATABASE:-postgres}")
fi

BENCH_EMP_ID="nep_bench_e"
BENCH_DOC_ID="nep_bench_d"
BENCH_TOKEN="nep_bench_t"

CLEANED=0
cleanup() {
  [[ "$CLEANED" -eq 1 ]] && return
  CLEANED=1
  if [[ "${BENCH_SETUP_OK:-0}" -ne 1 ]]; then
    return
  fi
  echo "Очистка БД (bench)..." >&2
  "${PSQL[@]}" -v ON_ERROR_STOP=1 <<EOF || true
DELETE FROM working_day_first.documents WHERE id = '${BENCH_DOC_ID}';
DELETE FROM wd_general.auth_tokens WHERE token = '${BENCH_TOKEN}';
DELETE FROM working_day_first.employees WHERE id = '${BENCH_EMP_ID}';
EOF
}

trap cleanup EXIT INT TERM

command -v curl >/dev/null
command -v jq >/dev/null

# До INSERT: сервис должен слушать BASE (иначе смысла в бенче нет).
echo "Проверка ${BASE} ..." >&2
if ! curl -sS -o /dev/null --connect-timeout 5 --max-time 10 "${BASE}/"; then
  echo "Ошибка: нет соединения с ${BASE}" >&2
  echo "Запусти userver или: export NEP_BENCH_BASE_URL='http://HOST:PORT'" >&2
  exit 1
fi

"${PSQL[@]}" -v ON_ERROR_STOP=1 -qtc "SELECT 1 FROM wd_general.companies WHERE id = 'first'" | grep -q 1 || {
  echo "В БД должна быть компания id=first (wd_general.companies)." >&2
  exit 1
}

echo "Вставка сотрудника, документа, токена..." >&2
"${PSQL[@]}" -v ON_ERROR_STOP=1 <<EOF
BEGIN;
INSERT INTO working_day_first.employees (id, name, surname)
VALUES ('${BENCH_EMP_ID}', 'Bench', 'NEP');
INSERT INTO wd_general.auth_tokens (token, user_id, company_id, scopes)
VALUES ('${BENCH_TOKEN}', '${BENCH_EMP_ID}', 'first', ARRAY['user']::text[]);
INSERT INTO working_day_first.documents (id, name, sign_required, description, chain_metadata_new)
VALUES (
  '${BENCH_DOC_ID}',
  'NEP bench',
  0,
  '',
  ARRAY[]::wd_general.chain_metadata_item_new[]
);
COMMIT;
EOF

BENCH_SETUP_OK=1


if [[ "${NEP_BENCH_VERBOSE:-0}" == "1" ]]; then
  echo "Verbose (psql):" >&2
  "${PSQL[@]}" -v ON_ERROR_STOP=1 -qtc "
SELECT 'auth -> ' || user_id FROM wd_general.auth_tokens WHERE token = '${BENCH_TOKEN}';
SELECT 'employee_keys count: ' || COUNT(*)::text FROM working_day_first.employee_keys WHERE employee_id = '${BENCH_EMP_ID}';
" | sed 's/^/  /' >&2
fi

AUTH_HDR="Authorization: Bearer ${BENCH_TOKEN}"

PDF_TMP="$(mktemp /tmp/nep_bench_XXXXXX.pdf)"
"$VPY" -c "
import fitz
from pathlib import Path
p = Path('${PDF_TMP}')
doc = fitz.open()
page = doc.new_page()
page.insert_text((72, 72), 'NEP bench')
doc.save(str(p))
doc.close()
"

if [[ -n "$S3_BUCKET" ]]; then
  echo "Загрузка PDF в S3 s3://${S3_BUCKET}/${BENCH_DOC_ID} ..." >&2
  "$VPY" "${ROOT}/scripts/bench_nep_upload_pdf.py" "$PDF_TMP" "$S3_BUCKET" "$BENCH_DOC_ID"
else
  echo "Без NEP_BENCH_S3_BUCKET — только keys/generate." >&2
fi
rm -f "$PDF_TMP"

# curl: таймауты + явная ошибка при обрыве соединения (не путать с HTTP 400).
curl_time() {
  local body meta code t ec=0
  body="$(mktemp)"
  set +e
  meta=$(curl -sS --connect-timeout 15 --max-time 180 -o "$body" -w "%{http_code}|%{time_total}" "$@")
  ec=$?
  set -e
  IFS='|' read -r code t <<< "${meta//$'\r'/}"
  if [[ "$ec" -ne 0 || -z "${code:-}" || "$code" == "000" ]]; then
    echo "Ошибка сети: curl exit=${ec}, http_code=${code:-?}. URL недоступен или таймаут." >&2
    [[ -s "$body" ]] && cat "$body" >&2
    rm -f "$body"
    exit 1
  fi
  if [[ "$code" != "200" ]]; then
    echo "HTTP ${code}:" >&2
    cat "$body" >&2
    echo "" >&2
    rm -f "$body"
    exit 1
  fi
  rm -f "$body"
  echo "$t"
}

# Убрать ключи НЭП у бенч-сотрудника (между итерациями keys/generate); не замеряется.
bench_strip_nep_keys() {
  "${PSQL[@]}" -v ON_ERROR_STOP=1 -q <<EOF
BEGIN;
DELETE FROM working_day_first.employee_keys WHERE employee_id = '${BENCH_EMP_ID}';
DELETE FROM working_day_first.employee_signature_passwords WHERE employee_id = '${BENCH_EMP_ID}';
COMMIT;
EOF
}

run_series() {
  local label="$1"
  shift
  local i
  local -a samples=()
  for ((i = 0; i < ITER; i++)); do
    samples+=("$(curl_time "$@")")
  done
  "$VPY" - "$label" "${samples[@]}" <<'PY'
import sys
label = sys.argv[1]
xs = [float(x) for x in sys.argv[2:]]
xs_ms = [x * 1000 for x in xs]
print(f"{label}: n={len(xs)}  min={min(xs_ms):.1f}ms  max={max(xs_ms):.1f}ms  mean={sum(xs_ms)/len(xs_ms):.1f}ms")
PY
}

echo ""
echo "Замер: ${BASE} (${ITER}× на ручку)"
echo ""

echo "=== POST /v1/employee/keys/generate (${ITER}×; между запросами — DELETE в БД, не в замер) ==="
samples_keys=()
for ((i = 0; i < ITER; i++)); do
  samples_keys+=("$(curl_time -X POST \
    "${BASE}/v1/employee/keys/generate?signature_password=${PASS}" \
    -H "${AUTH_HDR}")")
  if [[ "$i" -lt $((ITER - 1)) ]]; then
    bench_strip_nep_keys
  fi
done
"$VPY" - "keys_generate" "${samples_keys[@]}" <<'PY'
import sys
label = sys.argv[1]
xs = [float(x) for x in sys.argv[2:]]
xs_ms = [x * 1000 for x in xs]
print(f"{label}: n={len(xs)}  min={min(xs_ms):.1f}ms  max={max(xs_ms):.1f}ms  mean={sum(xs_ms)/len(xs_ms):.1f}ms")
PY

BENCH_SIG_ID=""
if [[ -n "$S3_BUCKET" ]]; then
  echo ""
  echo "=== POST /v1/documents/nep-sign ==="
  samples=()
  for ((i = 0; i < ITER; i++)); do
    body="$(mktemp)"
    ec=0
    set +e
    meta=$(curl -sS --connect-timeout 15 --max-time 180 -o "$body" -w "%{http_code}|%{time_total}" -X POST \
      "${BASE}/v1/documents/nep-sign?document_id=${BENCH_DOC_ID}&signature_password=${PASS}" \
      -H "${AUTH_HDR}" \
      -H "Content-Type: application/json" \
      -d '{}')
    ec=$?
    set -e
    IFS='|' read -r code t <<< "${meta//$'\r'/}"
    if [[ "$ec" -ne 0 || -z "${code:-}" || "$code" == "000" ]]; then
      echo "Ошибка сети (nep-sign): curl=$ec code=${code:-?}" >&2
      rm -f "$body"
      exit 1
    fi
    if [[ "$code" != "200" ]]; then
      echo "HTTP ${code} (nep-sign):" >&2
      cat "$body" >&2
      echo "" >&2
      rm -f "$body"
      exit 1
    fi
    samples+=("$t")
    if [[ -z "$BENCH_SIG_ID" ]]; then
      BENCH_SIG_ID="$(jq -r '.signature_id // empty' "$body")"
    fi
    rm -f "$body"
  done
  "$VPY" - "nep_sign" "${samples[@]}" <<'PY'
import sys
label = sys.argv[1]
xs = [float(x) for x in sys.argv[2:]]
xs_ms = [x * 1000 for x in xs]
print(f"{label}: n={len(xs)}  min={min(xs_ms):.1f}ms  max={max(xs_ms):.1f}ms  mean={sum(xs_ms)/len(xs_ms):.1f}ms")
PY

  if [[ -z "$BENCH_SIG_ID" || "$BENCH_SIG_ID" == "null" ]]; then
    echo "Нет signature_id в ответе nep-sign." >&2
    exit 1
  fi

  echo ""
  echo "=== GET /v1/documents/nep-verify ==="
  run_series "nep_verify" -X GET \
    "${BASE}/v1/documents/nep-verify?signature_id=${BENCH_SIG_ID}" \
    -H "${AUTH_HDR}"
fi

echo ""
trap - EXIT INT TERM
cleanup
echo "Готово."
