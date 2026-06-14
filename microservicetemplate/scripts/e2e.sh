#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
COMPOSE_FILE="$ROOT_DIR/microservicetemplate/docker-compose.yml"
BASE_URL="${BASE_URL:-http://127.0.0.1:8082}"
JWT_SECRET="${AUTH_JWT_SECRET:-dev-secret}"
APP_CONTAINER="${APP_CONTAINER:-aura-microservicetemplate-app}"

if docker compose version >/dev/null 2>&1; then
    COMPOSE=(docker compose)
elif command -v docker-compose >/dev/null 2>&1; then
    COMPOSE=(docker-compose)
else
    printf 'Docker Compose plugin or docker-compose is required\n' >&2
    exit 1
fi

compose() {
    "${COMPOSE[@]}" -f "$COMPOSE_FILE" "$@"
}

base64url() {
    openssl base64 -A | tr '+/' '-_' | tr -d '='
}

jwt_for() {
    local subject="$1"
    local header payload signing_input signature
    header="$(printf '%s' '{"alg":"HS256","typ":"JWT"}' | base64url)"
    payload="$(printf '{"sub":"%s"}' "$subject" | base64url)"
    signing_input="$header.$payload"
    signature="$(printf '%s' "$signing_input" | openssl dgst -sha256 -hmac "$JWT_SECRET" -binary | base64url)"
    printf '%s.%s' "$signing_input" "$signature"
}

request() {
    local method="$1"
    local path="$2"
    local token="${3:-}"
    local body="${4:-}"
    local request_text response status
    request_text="$(printf '%s %s HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n' "$method" "$path")"
    if [[ -n "$token" ]]; then
        request_text+=$'Authorization: Bearer '"$token"$'\r\n'
    fi
    if [[ -n "$body" ]]; then
        request_text+=$'Content-Type: application/json\r\nContent-Length: '"${#body}"$'\r\n'
    fi
    request_text+=$'\r\n'
    if [[ -n "$body" ]]; then
        request_text+="$body"
    fi

    for _ in $(seq 1 60); do
        response="$(
            printf '%s' "$request_text" | docker exec -i "$APP_CONTAINER" bash -lc '
                request_file=/tmp/aura-e2e-request
                cat > "$request_file"
                exec 3<>/dev/tcp/127.0.0.1/8082
                cat "$request_file" >&3
                response="$(cat <&3)"
                status="$(printf "%s" "$response" | sed -n "1s#HTTP/1\\.[01] \\([0-9][0-9][0-9]\\).*#\\1#p")"
                printf "%s\n%s" "$response" "$status"
            ' 2>/dev/null || true
        )"
        status="${response##*$'\n'}"
        if [[ -n "$status" && "$status" =~ ^[0-9]{3}$ ]]; then
            printf '%s' "$response"
            return 0
        fi
        sleep 1
    done

    printf '%s' "$response"
    return 1
}

gateway_request() {
    local method="$1"
    local path="$2"
    local body="${3:-}"
    local response status
    if [[ -n "$body" ]]; then
        response="$(curl -sS -i -X "$method" -H 'Content-Type: application/json' -d "$body" "http://127.0.0.1:8084$path" || true)"
    else
        response="$(curl -sS -i -X "$method" "http://127.0.0.1:8084$path" || true)"
    fi
    status="$(printf '%s' "$response" | sed -n '1s#HTTP/1\\.[01] \\([0-9][0-9][0-9]\\).*#\\1#p')"
    printf '%s\n%s' "$response" "$status"
}

assert_status() {
    local response="$1"
    local expected="$2"
    local actual="${response##*$'\n'}"
    if [[ "$actual" != "$expected" ]]; then
        printf 'expected HTTP %s, got %s\n%s\n' "$expected" "$actual" "$response" >&2
        exit 1
    fi
}

assert_contains() {
    local response="$1"
    local needle="$2"
    if ! printf '%s' "$response" | grep -Fq "$needle"; then
        printf 'expected response to contain %s\n%s\n' "$needle" "$response" >&2
        exit 1
    fi
}

cleanup() {
    compose down -v --remove-orphans >/dev/null 2>&1 || true
}
trap cleanup EXIT

cleanup
compose build app
compose build ai-gateway
compose up -d mysql
compose run --rm migrate
compose up -d app
compose up -d ai-gateway

for _ in $(seq 1 60); do
    if docker exec "$APP_CONTAINER" bash -lc 'exec 3<>/dev/tcp/127.0.0.1/8082' >/dev/null 2>&1; then
        break
    fi
    sleep 1
done

sleep 1
sleep 20

for _ in $(seq 1 60); do
    if curl -fsS http://127.0.0.1:8084/healthz >/dev/null 2>&1; then
        break
    fi
    sleep 1
done

user_a_token="$(jwt_for user-a)"
user_b_token="$(jwt_for user-b)"

root_response="$(request GET /)"
assert_status "$root_response" 302
assert_contains "$root_response" "Location: http://127.0.0.1:8084/"

options_response="$(request OPTIONS /api/v1/tasks)"
assert_status "$options_response" 200
assert_contains "$options_response" "Access-Control-Allow-Origin: *"

unauthorized="$(request GET /api/v1/tasks)"
assert_status "$unauthorized" 401

created="$(request POST /api/v1/tasks "$user_a_token" '{"title":"first","description":"created","status":"todo"}')"
assert_status "$created" 201
created_body="${created%$'\n'*}"
task_id="$(printf '%s' "$created_body" | sed -n 's/.*"id":\([0-9][0-9]*\).*/\1/p')"
if [[ -z "$task_id" ]]; then
    printf 'created response has no task id: %s\n' "$created_body" >&2
    exit 1
fi

listed="$(request GET /api/v1/tasks "$user_a_token")"
assert_status "$listed" 200
printf '%s' "$listed" | grep -q '"title":"first"'

updated="$(request PUT "/api/v1/tasks/$task_id" "$user_a_token" '{"title":"updated","description":"changed","status":"resolved"}')"
assert_status "$updated" 200
printf '%s' "$updated" | grep -q '"status":"resolved"'

listed_after_update="$(request GET /api/v1/tasks "$user_a_token")"
assert_status "$listed_after_update" 200
printf '%s' "$listed_after_update" | grep -q '"title":"updated"'

other_user_get="$(request GET "/api/v1/tasks/$task_id" "$user_b_token")"
assert_status "$other_user_get" 404

deleted="$(request DELETE "/api/v1/tasks/$task_id" "$user_a_token")"
assert_status "$deleted" 200
printf '%s' "$deleted" | grep -q '"deleted":true'

listed_after_delete="$(request GET /api/v1/tasks "$user_a_token")"
assert_status "$listed_after_delete" 200
if printf '%s' "$listed_after_delete" | grep -q "\"id\":$task_id"; then
    printf 'deleted task is still present: %s\n' "$listed_after_delete" >&2
    exit 1
fi

gateway_root="$(gateway_request GET /)"
assert_status "$gateway_root" 200
assert_contains "$gateway_root" "<link rel=\"stylesheet\" href=\"/styles.css\">"
assert_contains "$gateway_root" "<script defer src=\"/app.js\"></script>"

styles_response="$(gateway_request GET /styles.css)"
assert_status "$styles_response" 200
assert_contains "$styles_response" "Content-Type: text/css"

script_response="$(gateway_request GET /app.js)"
assert_status "$script_response" 200
assert_contains "$script_response" "Content-Type: application/javascript"

gateway_health="$(gateway_request GET /healthz)"
assert_status "$gateway_health" 200
assert_contains "$gateway_health" '"status": "ok"'

gateway_ready="$(gateway_request GET /readyz)"
assert_status "$gateway_ready" 200
assert_contains "$gateway_ready" '"status": "ready"'

gateway_options="$(gateway_request OPTIONS /api/highlight)"
assert_status "$gateway_options" 200

ai_highlight="$(gateway_request POST /api/highlight '{"prompt":"You are helping prioritize tasks.\nTASK|1|todo|first|created\nTASK|2|resolved|done|closed","temperature":0.2}')"
assert_status "$ai_highlight" 200
assert_contains "$ai_highlight" '"response": "1|'

ai_summary="$(gateway_request POST /api/summary '{"prompt":"You are summarizing a task board.\nTASK|1|todo|first|created\nTASK|2|resolved|done|closed","temperature":0.2}')"
assert_status "$ai_summary" 200
assert_contains "$ai_summary" '"response": "Board has'

printf 'MICROSERVICE_E2E_OK\n'
