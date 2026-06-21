# Стандартная библиотека Aura

Стандартная библиотека Aura состоит из встроенных модулей. Публичная поверхность живёт в `stdlib/std/**` и ориентирована на пользовательские сервисные сценарии. Низкоуровневые runtime bridge-модули и `std.*_native` остаются внутренней реализацией и не документируются как пользовательские пакеты.

## Пакеты

Базовые:

- [`std.core`](./core/README.md)
- [`std.io`](./io/README.md)
- [`std.math`](./math/README.md)
- [`std.array`](./array/README.md)
- [`std.map`](./map/README.md)
- [`std.text`](./text/README.md)
- [`std.log`](./log/README.md)
- [`std.option`](./option/README.md)
- [`std.result`](./result/README.md)
- [`std.base64`](./base64/README.md)
- [`std.crypto`](./crypto/README.md)
- [`std.config`](./config/README.md)
- [`std.backoff`](./backoff/README.md)
- [`std.uuid`](./uuid/README.md)

Concurrency, runtime и системные:

- [`std.sync`](./sync/README.md)
- [`std.task`](./task/README.md)
- [`std.channel`](./channel/README.md)
- [`std.context`](./context/README.md)
- [`std.concurrent.waitgroup`](./concurrent/waitgroup/README.md)
- [`std.concurrent.errorgroup`](./concurrent/errorgroup/README.md)
- [`std.env`](./env/README.md)
- [`std.time`](./time/README.md)
- [`std.net`](./net/README.md)

Данные и интеграции:

- [`std.db.mysql`](./db-mysql/README.md)
- [`std.outbox`](./outbox/README.md)
- [`std.mq.rabbitmq`](./mq-rabbitmq/README.md)
- [`std.http.raw`](./http-raw/README.md)
- [`std.http.server`](./http-server/README.md)
- [`std.http.middleware`](./http-middleware/README.md)
- [`std.json`](./json/README.md)
- [`std.service`](./service/README.md)

## Runtime Types

Публичная stdlib-поверхность использует два вида специальных типов:

- `any` для действительно runtime-polymorphic значений
- opaque runtime handle-типы вроде `std.sync.Thread`, `std.net.Connection`, `std.mysql.Result`
- sum types `std.option.Option<T>` и `std.result.Result<T, E>` для явного branching без исключений

Соглашения:

- `std.sync.Thread`, `std.sync.Mutex`, `std.net.Listener`, `std.net.Connection` - реальные типы публичной поверхности.
- `std.mysql.Connection`, `std.mysql.Pool`, `std.mysql.Statement`, `std.mysql.Result`, `std.mysql.Row` - runtime handle-типы, которые используются публичным фасадом `std.db.mysql`.
- `std.service.Store` - реальный тип store handle из `std.service`.
- `std.option.Option<T>` представлен как `None` или `Some(value)`; в runtime доступны `.tag` и payload index.
- `std.result.Result<T, E>` представлен как `Err(error)` или `Ok(value)`; в runtime доступны `.tag` и payload index.
- `HttpRequest` означает массив вида `[method, path, body]`.
- `any` в сигнатуре означает boxed runtime value, а не “неизвестный тип”.
