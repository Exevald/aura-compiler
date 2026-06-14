# Package std.mq.rabbitmq

`std.mq.rabbitmq` — high-level queue-first RabbitMQ facade для Aura-сервисов.

Модуль рассчитан на service-style сценарий: открыть connection, объявить очередь, опубликовать сообщение, затем обработать его через `consume`/`consume_with_options` и явно завершить `ack` или `nack`.

`open(...)` всегда возвращает connection handle. Если соединение не удалось открыть, handle будет закрытым, а текст ошибки можно получить через `error(conn)`.

## Функции

### `open(url: string) : std.mq.rabbitmq.Connection`

Открывает RabbitMQ connection handle.
Если broker недоступен или URL некорректен, connection остаётся закрытым, а причина доступна через `error(conn)`.

### `close(conn: std.mq.rabbitmq.Connection) : bool`

Закрывает connection handle.

### `declare_queue(conn, name: string) : bool`

Объявляет очередь.

### `publish(conn, queue: string, body: string) : bool`

Публикует строковое сообщение в очередь.

### `publish_json(conn, queue: string, body_json: string) : bool`

Публикует JSON payload после валидации и компактизации.

### `consume(conn, queue: string, handler) : void`

Запускает consumer loop c `prefetch = 1`.
`handler` получает `std.mq.rabbitmq.Message` и сам решает, вызывать `ack` или `nack`.

### `consume_with_options(conn, queue: string, prefetch: int, handler) : void`

Запускает consumer loop с explicit prefetch.
Практический способ завершить loop в одном процессе для worker/demo-сценария: после `ack` вызвать `close(conn)` внутри handler.

### `ack(message: std.mq.rabbitmq.Message) : bool`

Подтверждает обработку сообщения.

### `nack(message: std.mq.rabbitmq.Message, requeue: bool) : bool`

Отклоняет сообщение.

### `body(message: std.mq.rabbitmq.Message) : string`

Возвращает payload сообщения.

### `delivery_tag(message: std.mq.rabbitmq.Message) : int`

Возвращает delivery tag.

### `routing_key(message: std.mq.rabbitmq.Message) : string`

Возвращает routing key.

### `error(conn: std.mq.rabbitmq.Connection) : string`

Возвращает последнее текстовое описание ошибки для connection.

## Пример

```aura
import std.io as io;
import std.json as json;
import std.mq.rabbitmq as mq;
import std.uuid as uuid;

var conn = mq.open("amqp://guest:guest@127.0.0.1:5672/");
var queue = "jobs";

fn process_message(message) : void {
    var payload = mq.body(message);
    io.println("job_id", json.get_string(payload, "job_id"));
    io.println("kind", json.get_string(payload, "kind"));
    io.println("ack", mq.ack(message));
    io.println("close", mq.close(conn));
};

print mq.declare_queue(conn, queue);
print mq.publish_json(conn, queue, json.object([
    json.field("job_id", json.encode_string(uuid.new_v4())),
    json.field("kind", json.encode_string("thumbnail"))
]));
mq.consume(conn, queue, process_message);
```

Для этого примера нужен локальный broker на `127.0.0.1:5672`.
