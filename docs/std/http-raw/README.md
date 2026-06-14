# Package std.http.raw

`std.http.raw` предоставляет низкоуровневые HTTP helper-функции: парсинг request и сборку raw HTTP response.

## Функции

### `parse_request(raw: string) : HttpRequest`

Разбирает сырой HTTP request text и возвращает `HttpRequest`.

- `HttpRequest` в документации означает `[method, path, body]`.

```aura
var req = raw.parse_request("GET /health HTTP/1.1\r\nContent-Length: 0\r\n\r\n");
```

### `read_request(connection: NetConnection) : HttpRequest`

Читает полный HTTP request из TCP-соединения.

```aura
import std.net as net;

var listener = net.listen("127.0.0.1", 8080);
var connection = net.accept(listener);
var req = raw.read_request(connection);
```

### `try_read_request(connection: NetConnection) : HttpRequest`

Пытается прочитать и разобрать request. При неуспехе runtime возвращает пустой массив.

```aura
import std.net as net;

var listener = net.listen("127.0.0.1", 8080);
var connection = net.accept(listener);
var req = raw.try_read_request(connection);
```

### `response(status: int, content_type: string, body: string) : string`

Собирает полный raw HTTP response.

```aura
var res = raw.response(200, "text/plain", "ok");
```

### `response_with_headers(status: int, content_type: string, body: string, headers: [string]) : string`

То же, что `response`, но с дополнительными заголовками.

```aura
var res = raw.response_with_headers(200, "text/plain", "ok", ["X-Trace: 1"]);
```

### `text(status: int, body: string) : string`

Собирает `text/plain` response.

```aura
var res = raw.text(404, "missing");
```

### `json(status: int, body_json: string) : string`

Собирает JSON response.

```aura
var res = raw.json(200, "{\"ok\":true}");
```

### `ok_json(body_json: string) : string`

Сокращение для JSON-response со статусом `200 OK`.

```aura
var res = raw.ok_json("{\"ok\":true}");
```

### `redirect(location: string) : string`

Собирает redirect-response со статусом `302`.

```aura
var res = raw.redirect("https://example.com");
```

## Пример

```aura
import std.http.raw as raw;
import std.json as json;

var body = json.object([
    json.field("ok", json.encode_bool(true))
]);
var response = raw.ok_json(body);
```
