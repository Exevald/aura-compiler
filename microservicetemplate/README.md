# microservicetemplate

Шаблон CRUD-микросервиса на Aura с DDD-слоями, MySQL-backed `TaskRepository`, JWT bearer auth, actor-backed read-cache для списка задач и локальным observability stack на Prometheus + Loki + Grafana.

Пользовательский интерфейс теперь вынесен в нативный `taskManagerApp` из `MyLanguage`, который запускается отдельным Docker-контейнером. Он ходит в `microservicetemplate` по bearer JWT и вызывает `ai-gateway` для highlight/summary.

## Быстрая проверка

Проверить все entrypoints без запуска MySQL и выполнить полный showcase языка:

```bash
cd microservicetemplate
make test
```

`cmd/showcase.aura` изолирован от production HTTP-path и демонстрирует
типы, generics, interfaces, enums, contracts, effects, actors, transactions,
comptime, iterators, maps, closures, variadic functions, `go`/`await`,
tasks, channels, pointers/refs и `unsafe`.

## Локально без контейнеров

1. Подними MySQL.
2. Примени миграцию:

```bash
cd microservicetemplate
MYSQL_DSN='host=127.0.0.1;port=3306;user=aura;password=aura;database=microservicetemplate' \
AUTH_JWT_SECRET='dev-secret' \
make migrate
```

3. Запусти сервис:

```bash
cd microservicetemplate
MYSQL_DSN='host=127.0.0.1;port=3306;user=aura;password=aura;database=microservicetemplate' \
AUTH_JWT_SECRET='dev-secret' \
make run
```

Проверка:

```bash
curl -i http://127.0.0.1:8082/healthz
curl -i http://127.0.0.1:8082/readyz
curl -i -H 'Authorization: Bearer <jwt>' http://127.0.0.1:8082/api/v1/tasks
```

JWT ожидается в формате `HS256` и должен содержать как минимум claim `sub`. Все `/api/v1/tasks*` endpoints требуют bearer token. `/healthz` и `/readyz` остаются публичными.

Для локального фронтенда есть dev endpoint `POST /api/dev/token`, который возвращает demo JWT для браузерного UI.

## Docker Compose

Файлы:

- `Dockerfile`
- `docker-compose.yml`
- `.env.example`
- `E2E.md`
- `ai_gateway/`

Полный контейнерный e2e-прогон с JWT и CRUD-запросами описан в `E2E.md`.

```bash
cd microservicetemplate
make test-e2e
```

Сборка и запуск из корня репозитория:

```bash
docker compose -f microservicetemplate/docker-compose.yml build app ai-gateway task-manager-app
docker compose -f microservicetemplate/docker-compose.yml up mysql -d
docker compose -f microservicetemplate/docker-compose.yml run --rm migrate
docker compose -f microservicetemplate/docker-compose.yml up app
docker compose -f microservicetemplate/docker-compose.yml up prometheus loki alloy grafana ai-gateway task-manager-app
```

Compose использует build context от корня `aura-compiler`, потому что контейнерам нужны:

- исходники `aura-compiler/microservicetemplate`
- стандартная библиотека Stasis из `microservicetemplate/bin/stl`, синхронизированная из `~/study/stasis/bin/stl`
- Linux-зависимости MySQL client и CURL headers в build image (`default-libmysqlclient-dev`, `libcurl4-openssl-dev`)
- `clang-tools-18` в build image, чтобы CMake мог вызвать `clang-scan-deps`
- `git` в build image, потому что Stasis CMake подтягивает Catch2 через `FetchContent`
- исходники `aura-compiler/microservicetemplate/mylanguage`, где лежит vendored `MyLanguage` и `taskManagerApp`

`ai_gateway` собирает Stasis compiler внутри Docker image из vendored `microservicetemplate/stasis-src` и остаётся отдельным AI-сервисом на `http://127.0.0.1:8084/`.
По умолчанию он ходит в Gemini OpenAI-compatible endpoint (`POST /v1beta/openai/chat/completions`) с моделью `gemini-3.5-flash`; API key передаётся через `AI_GATEWAY_API_KEY` или `GEMINI_API_KEY` и должен приходить из окружения, не из репозитория.
Для обратной совместимости старые значения `gpt-5.4` и `gpt-5.4-medium` автоматически нормализуются в `gemini-3.5-flash`.
Если нужен локальный офлайн-фоллбек, можно явно поставить `AI_GATEWAY_MODE=mock`.
В runtime-образ также копируется `microservicetemplate/stasis-src/grammar`, потому что собранный compiler ожидает grammar по пути `/src/stasis/grammar`.

`prometheus` scrape'ит `http://app:8082/metrics`, `loki` собирает container logs через Alloy, а `grafana` открывается на `http://localhost:3000/` и получает provisioned datasources + dashboards.

`task-manager-app` собирается из `microservicetemplate/mylanguage/Dockerfile`, запускает `taskManagerApp/main.rocket` и по сети использует:

- `http://app:8082` для CRUD задач и `POST /api/dev/token`
- `http://ai-gateway:8084` для highlight/summary

`task-manager-app` по умолчанию поднимает `Xvfb` внутри контейнера и публикует noVNC на `6080`. Это убирает зависимость от XQuartz/X11 и внешнего VNC-клиента.

Чтобы посмотреть окно:

```bash
docker compose -f microservicetemplate/docker-compose.yml up -d task-manager-app
```

Открывай в браузере:

```bash
open http://localhost:6080/vnc.html
```

Если `localhost` не подходит в твоей среде, используй IP Lima VM, например `http://192.168.5.15:6080/vnc.html`.

## Docker Image

Сборка образа:

```bash
docker build -f microservicetemplate/Dockerfile -t microservicetemplate:latest .
```

Если нужен только `ai_gateway`, собирай его отдельным Dockerfile из корня репозитория:

```bash
docker build -f microservicetemplate/ai_gateway/Dockerfile -t aura-ai-gateway:local .
```

Запуск миграции:

```bash
docker run --rm \
  -e MYSQL_DSN='host=host.docker.internal;port=3306;user=aura;password=aura;database=microservicetemplate' \
  -e AUTH_JWT_SECRET='dev-secret' \
  microservicetemplate:latest \
  /app/aura-compiler run /app/microservicetemplate/cmd/migrate.aura
```

Запуск сервиса:

```bash
docker run --rm -p 8082:8082 \
  -e HOST=0.0.0.0 \
  -e PORT=8082 \
  -e MYSQL_POOL_SIZE=4 \
  -e READ_TIMEOUT_MS=1000 \
  -e WRITE_TIMEOUT_MS=1000 \
  -e MYSQL_DSN='host=host.docker.internal;port=3306;user=aura;password=aura;database=microservicetemplate' \
  -e AUTH_JWT_SECRET='dev-secret' \
  microservicetemplate:latest
```

## Kubernetes

Манифесты лежат в `microservicetemplate/k8s/`.

Порядок применения:

```bash
kubectl apply -f microservicetemplate/k8s/namespace.yaml
kubectl apply -f microservicetemplate/k8s/mysql.yaml
kubectl apply -f microservicetemplate/k8s/app-config.yaml
kubectl apply -f microservicetemplate/k8s/migrate-job.yaml
kubectl wait --for=condition=complete job/microservicetemplate-migrate -n aura-microservicetemplate --timeout=180s
kubectl apply -f microservicetemplate/k8s/app-deployment.yaml
kubectl apply -f microservicetemplate/k8s/app-service.yaml
```

Перед применением замени `your-registry/microservicetemplate:latest` в манифестах на свой image.

## HTTP API

- `GET /healthz`
- `GET /readyz`
- `GET /metrics`
- `GET /api/v1/tasks`
- `GET /api/v1/tasks/{id}`
- `POST /api/v1/tasks`
- `PUT /api/v1/tasks/{id}`
- `DELETE /api/v1/tasks/{id}`
- `POST /api/dev/token`

## Observability

Grafana открывается на `http://localhost:3000/` без логина и уже видит datasource `Prometheus`.
Отдельно есть datasource `Loki` и dashboard `microservicetemplate container logs`.

Панели в dashboard:

- requests/sec
- error rate
- latency p95
- top routes

Prometheus открыт на `http://localhost:9090/` и scrape'ит `app:8082/metrics`.
Loki открыт на `http://localhost:3100/`, Alloy читает Docker socket и отправляет логи контейнеров в Loki.

Быстрая проверка:

```bash
curl -s http://127.0.0.1:8082/metrics | grep microservicetemplate_http_requests_total
curl -s "http://127.0.0.1:9090/api/v1/query?query=microservicetemplate_http_requests_total" | grep route
```

## Build Notes

- `microservicetemplate/bin/stl` должен быть синхронизирован из `~/study/stasis/bin/stl` перед сборкой `ai_gateway`.
- Linux-сборка Stasis больше не требует ручной правки пути к MySQL: CMake ищет стандартные каталоги `/usr/include`, `/usr/lib/x86_64-linux-gnu` и родственные.
- `taskManagerApp` image использует `cmake 3.30+` из `pip`, `clang-tools-18` для `clang-scan-deps` и `nlohmann-json3-dev` для `nlohmann/json.hpp`.
- Grafana provisioning лежит в `microservicetemplate/observability/grafana/`, Prometheus config в `microservicetemplate/observability/prometheus/`.
- Loki config лежит в `microservicetemplate/observability/loki/`, Alloy config в `microservicetemplate/observability/alloy/`.
- Если Docker на Lima упирается в место, сначала чисти builder/image cache в VM:

```bash
limactl shell docker docker builder prune -af
limactl shell docker docker image prune -af
limactl shell docker docker container prune -f
limactl shell docker docker volume prune -f
```

## AI Gateway

Отдельный сервис на Stasis поднимается как `ai-gateway` в `docker-compose.yml`.

- health: `http://127.0.0.1:8084/healthz`
- ready: `http://127.0.0.1:8084/readyz`
- highlight: `POST http://127.0.0.1:8084/api/highlight`
- summary: `POST http://127.0.0.1:8084/api/summary`

Legacy/debug web assets всё ещё доступны в `ai_gateway`, но основной пользовательский интерфейс теперь живёт в `taskManagerApp`.

## Task Manager App

Нативный GUI-клиент живёт в `microservicetemplate/mylanguage/taskManagerApp`.

- строится отдельным Docker-образом `aura-task-manager-app:local`
- использует bearer token, который получает через `POST /api/dev/token`
- умеет создавать, редактировать, удалять и переводить задачи в `resolved`
- вызывает `ai-gateway` для highlight/summary и подсвечивает важные карточки
- при отсутствии X-сервера стартует через `Xvfb`, чтобы контейнер был usable в headless-среде

## Service Shape

- `internal/domain/model`: сущность `Task` и интерфейс `TaskRepository`
- `internal/domain/service`: доменные правила валидации и обновления задачи
- `internal/app/auth`: token contract
- `internal/app/context`: request context с authenticated user
- `internal/app/service`: orchestration, MySQL transactions, cache invalidation
- `internal/app/service/auth`: auth decorator над app service
- `internal/infrastructure/auth`: JWT HS256 parser
- `internal/infrastructure/mysql`: MySQL repository
- `internal/infrastructure/transport`: HTTP routes и mapping ошибок в HTTP
