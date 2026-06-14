# Microservice E2E

Полный тест запускает MySQL и Aura-сервис через Docker Compose, применяет
миграцию и проверяет HTTP API с реальными HS256 JWT.

`ai_gateway` поднимается отдельным compose-сервисом и отдаёт фронтенд, статические assets и AI highlight/summary endpoints. Текущий e2e проверяет и backend CRUD, и gateway assets/API в mock-режиме.

Для сборки используются:

- `microservicetemplate/Dockerfile` для backend image
- `microservicetemplate/ai_gateway/Dockerfile` для Stasis gateway
- `microservicetemplate/bin/stl`, синхронизированный из `~/study/stasis/bin/stl`
- `microservicetemplate/stasis-src/grammar`, который копируется в runtime image как `/src/stasis/grammar`
- стандартные Linux MySQL client библиотеки, CURL headers, `clang-tools-18` и `git`, которые ставятся внутри build image

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
- `GET /` backend redirect на gateway
- `GET /`, `GET /styles.css`, `GET /app.js` у gateway
- `401` без bearer token
- создание, чтение списка, обновление и удаление задачи
- `OPTIONS` preflight и CORS-заголовки у backend
- `POST /api/highlight` и `POST /api/summary` у gateway в mock-режиме
- инвалидирование actor-backed list cache после записи
- изоляция задач по JWT claim `sub`
- canonical статус `resolved`

Успешный прогон завершается строкой `MICROSERVICE_E2E_OK` и удаляет
контейнеры вместе с тестовым volume.
