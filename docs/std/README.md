# Стандартная библиотека Aura

Стандартная библиотека Aura состоит из встроенных модулей. Публичная поверхность живёт в `stdlib/std/**` и ориентирована на пользовательские сервисные сценарии. Низкоуровневые runtime bridge-модули и `std.*_native` остаются внутренней реализацией и не документируются как пользовательские пакеты.

## Пакеты

Базовые:

- [`std.core`](./core/README.md)
- [`std.io`](./io/README.md)
- [`std.math`](./math/README.md)
- [`std.array`](./array/README.md)
- `std.map`
- [`std.text`](./text/README.md)
- [`std.log`](./log/README.md)
- `std.option`
- `std.result`
- `std.base64`
- `std.crypto`
- [`std.config`](./config/README.md)
- [`std.backoff`](./backoff/README.md)
- [`std.uuid`](./uuid/README.md)

Concurrency, runtime и системные:

- [`std.sync`](./sync/README.md)
- `std.task`
- `std.channel`
- `std.context`
- `std.concurrent.waitgroup`
- `std.concurrent.errorgroup`
- [`std.env`](./env/README.md)
- [`std.time`](./time/README.md)
- [`std.net`](./net/README.md)

Данные и интеграции:

- [`std.db.mysql`](./db-mysql/README.md)
- [`std.outbox`](./outbox/README.md)
- [`std.mq.rabbitmq`](./mq-rabbitmq/README.md)
- [`std.http.raw`](./http-raw/README.md)
- [`std.http.server`](./http-server/README.md)
- `std.http.middleware`
- [`std.json`](./json/README.md)
- [`std.service`](./service/README.md)

## Runtime Types

Публичная stdlib-поверхность использует два вида специальных типов:

- `any` для действительно runtime-polymorphic значений
- opaque runtime handle-типы вроде `std.sync.Thread`, `std.net.Connection`, `std.mysql.Result`

Соглашения:

- `std.sync.Thread`, `std.sync.Mutex`, `std.net.Listener`, `std.net.Connection` - реальные типы публичной поверхности.
- `std.mysql.Connection`, `std.mysql.Pool`, `std.mysql.Statement`, `std.mysql.Result`, `std.mysql.Row` - runtime handle-типы, которые используются публичным фасадом `std.db.mysql`.
- `std.service.Store` - реальный тип store handle из `std.service`.
- `HttpRequest` означает массив вида `[method, path, body]`.
- `any` в сигнатуре означает boxed runtime value, а не “неизвестный тип”.
