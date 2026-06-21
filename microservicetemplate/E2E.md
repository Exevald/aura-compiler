# Microservice E2E

Полный тест запускает MySQL и Aura-сервис через Docker Compose, применяет
миграцию и проверяет HTTP API с реальными HS256 JWT.

`ai_gateway` поднимается отдельным compose-сервисом и остаётся AI-adapter'ом для highlight/summary. Пользовательский UI теперь даёт `taskManagerApp`, который строится отдельным compose-сервисом и ходит в backend по bearer JWT. Для метрик и логов добавлены `prometheus`, `loki`, `alloy` и `grafana`.

Для сборки используются:

- `microservicetemplate/Dockerfile` для backend image
- `microservicetemplate/ai_gateway/Dockerfile` для Stasis gateway
- `microservicetemplate/mylanguage/Dockerfile` для `taskManagerApp`
- `microservicetemplate/bin/stl`, синхронизированный из `~/study/stasis/bin/stl`
- `microservicetemplate/stasis-src/grammar`, который копируется в runtime image как `/src/stasis/grammar`
- стандартные Linux MySQL client библиотеки, CURL headers, `clang-tools-18` и `git`, которые ставятся внутри build image
- `microservicetemplate/observability/prometheus/prometheus.yml`
- `microservicetemplate/observability/loki/loki.yml`
- `microservicetemplate/observability/alloy/config.alloy`
- `microservicetemplate/observability/grafana/provisioning/*`
- `microservicetemplate/observability/grafana/dashboards/microservicetemplate.json`
- `microservicetemplate/observability/grafana/dashboards/container-logs.json`

## Запуск

Из каталога `microservicetemplate`:

```bash
make test-e2e
```

Или из корня репозитория:

```bash
./microservicetemplate/scripts/e2e.sh
```

Нужны Docker с Compose plugin, `curl`, `openssl`, `grep` и `sed`.

## Что проверяется

- сборка Linux-образа с актуальным `aura-compiler`
- успешная миграция MySQL
- `/healthz` и `/readyz` у backend и gateway
- `/metrics` у backend
- container logs попадают в Loki
- `GET /` backend redirect на gateway
- `401` без bearer token
- создание, чтение списка, обновление и удаление задачи
- `OPTIONS` preflight и CORS-заголовки у backend
- `POST /api/highlight` и `POST /api/summary` у gateway в mock-режиме при явном `AI_GATEWAY_MODE=mock`
- `taskManagerApp` smoke через тот же image, который делает authenticated CRUD и AI-запросы
- инвалидирование actor-backed list cache после записи
- изоляция задач по JWT claim `sub`
- canonical статус `resolved`
- `taskManagerApp` стартует в контейнере и может слать запросы в backend и ai-gateway
- Prometheus scrape target `app:8082/metrics` поднимается
- Loki принимает логи контейнеров через Alloy
- Grafana health endpoint отвечает и dashboard provisioning доступен

Успешный прогон завершается строкой `MICROSERVICE_E2E_OK` и удаляет
контейнеры вместе с тестовым volume.
