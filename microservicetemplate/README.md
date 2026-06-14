# microservicetemplate

Шаблон CRUD-микросервиса на Aura с DDD-слоями, MySQL-backed `TaskRepository`, JWT bearer auth и actor-backed read-cache для списка задач.

Рядом добавлен отдельный `ai_gateway` на Stasis. Он отдаёт светлый pipeline-фронтенд из `index.html`, `styles.css` и `app.js`, а также AI-endpoints для highlight/summary; сам `microservicetemplate` остаётся чистым API задач.

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
docker compose -f microservicetemplate/docker-compose.yml build app ai-gateway
docker compose -f microservicetemplate/docker-compose.yml up mysql -d
docker compose -f microservicetemplate/docker-compose.yml run --rm migrate
docker compose -f microservicetemplate/docker-compose.yml up app
docker compose -f microservicetemplate/docker-compose.yml up ai-gateway
```

Compose использует build context от корня `aura-compiler`, потому что контейнерам нужны:

- исходники `aura-compiler/microservicetemplate`
- стандартная библиотека Stasis из `microservicetemplate/bin/stl`, синхронизированная из `~/study/stasis/bin/stl`
- Linux-зависимости MySQL client и CURL headers в build image (`default-libmysqlclient-dev`, `libcurl4-openssl-dev`)
- `clang-tools-18` в build image, чтобы CMake мог вызвать `clang-scan-deps`
- `git` в build image, потому что Stasis CMake подтягивает Catch2 через `FetchContent`

`ai_gateway` собирает Stasis compiler внутри Docker image из vendored `microservicetemplate/stasis-src`, а затем запускает UI из `microservicetemplate/web`. Браузерный UI открывается на `http://127.0.0.1:8084/`.
В runtime-образ также копируется `microservicetemplate/stasis-src/grammar`, потому что собранный compiler ожидает grammar по пути `/src/stasis/grammar`.

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
- `GET /api/v1/tasks`
- `GET /api/v1/tasks/{id}`
- `POST /api/v1/tasks`
- `PUT /api/v1/tasks/{id}`
- `DELETE /api/v1/tasks/{id}`
- `POST /api/dev/token`

## Build Notes

- `microservicetemplate/bin/stl` должен быть синхронизирован из `~/study/stasis/bin/stl` перед сборкой `ai_gateway`.
- Linux-сборка Stasis больше не требует ручной правки пути к MySQL: CMake ищет стандартные каталоги `/usr/include`, `/usr/lib/x86_64-linux-gnu` и родственные.
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
- UI: `http://127.0.0.1:8084/`
- assets: `GET /styles.css`, `GET /app.js`
- highlight: `POST http://127.0.0.1:8084/api/highlight`
- summary: `POST http://127.0.0.1:8084/api/summary`

Фронтенд ходит в `microservicetemplate` по `http://127.0.0.1:8082` и использует `POST /api/dev/token` для demo JWT. Для `GET/POST/PUT/DELETE` у `microservicetemplate` включены CORS-заголовки.

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
