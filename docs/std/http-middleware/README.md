# Package std.http.middleware

`std.http.middleware` предоставляет middleware-композицию поверх `(request) -> string` HTTP handlers.

## Функции

### `chain(route_fn, middlewares) : (HttpRequest) -> string`

Собирает handler pipeline из route_fn и списка middleware.

- Middleware применяются справа налево.
- Каждый middleware получает `next_fn` и `request`.

```aura
var app = middleware.chain(route, [middleware.request_logger]);
```

### `request_logger(next_fn, request: [string]) : string`

Логирует метод и path request, затем вызывает следующий handler.

```aura
var app = middleware.chain(route, [middleware.request_logger]);
```

### `with_response_header(name: string, value: string)`

Создает middleware, которое добавляет response header.

```aura
var app = middleware.chain(route, [
    middleware.with_response_header("X-Service", "aura")
]);
```

## Пример

```aura
import std.http.middleware as middleware;
import std.http.server as server;
import std.json as json;

fn route(request: [string]) : string {
    return server.ok_json(json.object([]));
}

var app = middleware.chain(route, [
    middleware.request_logger,
    middleware.with_response_header("X-Test", "1")
]);
```
