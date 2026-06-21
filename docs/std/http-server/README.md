# Package std.http.server

`std.http.server` - high-level HTTP facade module поверх runtime HTTP server API. В документации `HttpRequest` означает массив `[method, path, body]`, а `route_fn` имеет тип `(HttpRequest) -> string`.

## Функции

### `serve_once(address: string, port: int, route_fn: (HttpRequest) -> string) : void`

Запускает HTTP server, обслуживающий ровно один request.

```aura
fn route(request: [string]) : string {
    return server.text(200, "ok");
}

server.serve_once("127.0.0.1", 8080, route);
```

### `serve_n(address: string, port: int, max_requests: int, route_fn: (HttpRequest) -> string) : void`

Запускает server и обслуживает ограниченное число запросов.

```aura
fn route(request: [string]) : string {
    return server.text(200, "ok");
}

server.serve_n("127.0.0.1", 8080, 10, route);
```

### `listen_and_serve(address: string, port: int, route_fn: (HttpRequest) -> string) : void`

Запускает обычный server loop без заранее заданного лимита запросов.

```aura
fn route(request: [string]) : string {
    return server.text(200, "ok");
}

server.listen_and_serve("127.0.0.1", 8080, route);
```

### `listen_and_serve_with(address: string, port: int, route_fn: (HttpRequest) -> string, shutdown_ctx: std.context.Context, read_timeout_ms: int, write_timeout_ms: int) : void`

Запускает обычный server loop c graceful shutdown через `shutdown_ctx` и явными socket timeout значениями.

```aura
import std.context as ctx;

var shutdown = ctx.background();
server.listen_and_serve_with("0.0.0.0", 8080, route, shutdown, 1000, 1000);
```

### `method(request: HttpRequest) : string`

Возвращает HTTP-метод request.

```aura
import std.http.raw as raw;

var request = raw.parse_request("GET /health HTTP/1.1\r\nContent-Length: 0\r\n\r\n");
var method = server.method(request);
```

### `path(request: HttpRequest) : string`

Возвращает path часть request.

### `body(request: HttpRequest) : string`

Возвращает request body как строку.

### `header(request: HttpRequest, name: string) : string`

Возвращает значение header по имени.

### `path_segments(request: HttpRequest) : [string]`

Разбивает path на сегменты через `/`.

- Результат включает пустые элементы для leading/trailing slash.

### `segment(request: HttpRequest, index: int) : string`

Возвращает конкретный path segment по индексу.

### `path_has_prefix(request: HttpRequest, prefix: string) : bool`

Проверяет, начинается ли path с указанного префикса.

### `path_suffix_after_prefix(request: HttpRequest, prefix: string) : string`

Возвращает остаток path после заданного префикса.

- Если prefix не совпадает с началом path, возвращает пустую строку.
- Если suffix после prefix содержит еще один `/`, возвращает пустую строку.
- Это helper для простого path routing.

### `route_equals(request: HttpRequest, method: string, path: string) : bool`

Проверяет одновременное совпадение HTTP-метода и пути.

```aura
import std.http.raw as raw;

var request = raw.parse_request("GET /health HTTP/1.1\r\nContent-Length: 0\r\n\r\n");
var ok = server.route_equals(request, "GET", "/health");
```

### `ok_json(body_json: string) : string`

Возвращает JSON-response со статусом `200 OK`.

### `created_json(body_json: string) : string`

Возвращает JSON-response со статусом `201 Created`.

### `json(status: int, body_json: string) : string`

Возвращает JSON-response с произвольным статусом.

### `response_with_headers(status: int, content_type: string, body: string, headers: [string]) : string`

Собирает response с дополнительными заголовками.

### `text(status: int, body: string) : string`

Возвращает `text/plain` response.

### `not_found(message: string) : string`

Возвращает готовый `404` response.

### `bad_request(message: string) : string`

Возвращает готовый `400` response.

### `redirect(location: string) : string`

Возвращает готовый `302` response.

## Пример

```aura
import std.http.server as server;
import std.http.middleware as middleware;
import std.json as json;

fn route(request: [string]) : string {
    if (server.route_equals(request, "GET", "/health")) {
        return server.ok_json(json.object([json.field("ok", json.encode_bool(true))]));
    }
    return server.not_found("missing");
}

var app = middleware.chain(route, [
    middleware.request_logger,
    middleware.with_response_header("X-Service", "example")
]);
```
