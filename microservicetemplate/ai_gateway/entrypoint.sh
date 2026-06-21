#!/bin/sh
set -eu

escape_sed_replacement() {
  printf '%s' "$1" | sed -e 's/[&|]/\\&/g'
}

normalize_model() {
  case "$1" in
    gpt-5.4|gpt-5.4-medium)
      printf '%s' "gemini-3.5-flash"
      ;;
    *)
      printf '%s' "$1"
      ;;
  esac
}

PROVIDER_URL=$(escape_sed_replacement "${AI_GATEWAY_PROVIDER_URL:-https://generativelanguage.googleapis.com/v1beta/openai/chat/completions}")
API_KEY_VALUE="${AI_GATEWAY_API_KEY:-${GEMINI_API_KEY:-}}"
API_KEY=$(escape_sed_replacement "${API_KEY_VALUE}")
MODE=$(escape_sed_replacement "${AI_GATEWAY_MODE:-real}")
MODEL_VALUE="${AI_GATEWAY_MODEL:-gemini-3.5-flash}"
MODEL_NORMALIZED="$(normalize_model "$MODEL_VALUE")"
if [ "$MODEL_VALUE" != "$MODEL_NORMALIZED" ]; then
  printf '%s\n' "AI_GATEWAY_MODEL=$MODEL_VALUE normalized to $MODEL_NORMALIZED" >&2
fi
MODEL=$(escape_sed_replacement "$MODEL_NORMALIZED")
HTTP_PORT=$(escape_sed_replacement "${AI_GATEWAY_HTTP_PORT:-8084}")

if [ "$MODE" != "mock" ] && [ -z "$API_KEY_VALUE" ]; then
  printf '%s\n' "AI_GATEWAY_API_KEY or GEMINI_API_KEY is required in real mode" >&2
  exit 1
fi

sed -i \
  -e "s|__AI_GATEWAY_PROVIDER_URL__|${PROVIDER_URL}|g" \
  -e "s|__AI_GATEWAY_API_KEY__|${API_KEY}|g" \
  -e "s|__AI_GATEWAY_MODE__|${MODE}|g" \
  -e "s|__AI_GATEWAY_MODEL__|${MODEL}|g" \
  -e "s|__AI_GATEWAY_HTTP_PORT__|${HTTP_PORT}|g" \
  /app/microservicetemplate/ai_gateway/controller/controller.stas

exec /app/compiler run /app/microservicetemplate/ai_gateway/controller/controller.stas
