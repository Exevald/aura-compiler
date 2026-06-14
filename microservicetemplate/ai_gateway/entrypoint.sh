#!/bin/sh
set -eu

escape_sed_replacement() {
  printf '%s' "$1" | sed -e 's/[&|]/\\&/g'
}

PROVIDER_URL=$(escape_sed_replacement "${AI_GATEWAY_PROVIDER_URL:-https://api.openai.com/v1/chat/completions}")
API_KEY=$(escape_sed_replacement "${AI_GATEWAY_API_KEY:-}")
MODE=$(escape_sed_replacement "${AI_GATEWAY_MODE:-mock}")
MODEL=$(escape_sed_replacement "${AI_GATEWAY_MODEL:-qwen-plus}")
HTTP_PORT=$(escape_sed_replacement "${AI_GATEWAY_HTTP_PORT:-8084}")

sed -i \
  -e "s|__AI_GATEWAY_PROVIDER_URL__|${PROVIDER_URL}|g" \
  -e "s|__AI_GATEWAY_API_KEY__|${API_KEY}|g" \
  -e "s|__AI_GATEWAY_MODE__|${MODE}|g" \
  -e "s|__AI_GATEWAY_MODEL__|${MODEL}|g" \
  -e "s|__AI_GATEWAY_HTTP_PORT__|${HTTP_PORT}|g" \
  /app/microservicetemplate/ai_gateway/controller/controller.stas

exec /app/compiler run /app/microservicetemplate/ai_gateway/controller/controller.stas
