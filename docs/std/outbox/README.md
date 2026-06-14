# Package std.outbox

`std.outbox` реализует service-style transactional outbox поверх `std.db.mysql` и `std.mq.rabbitmq`.

Модель v1:

- бизнес-изменения и запись события делаются в одной MySQL транзакции
- relay worker позже публикует накопленные записи в RabbitMQ
- после успешной публикации запись помечается как `sent`
- при неуспехе relay увеличивает `attempts` и сохраняет `last_error`

## Функции

### `ensure_schema(conn: std.mysql.Connection) : bool`

Создаёт таблицу `aura_outbox`, если её ещё нет.

### `enqueue_text(conn: std.mysql.Connection, topic: string, routing_key: string, payload: string) : int`

Добавляет запись в outbox со статусом `pending` и возвращает `id`.

### `enqueue_json(conn: std.mysql.Connection, topic: string, routing_key: string, payload_json: string) : int`

То же самое, но с JSON валидацией и compaction.

### `with_tx(conn: std.mysql.Connection, callback) : any`

Outbox-oriented обёртка над `std.db.mysql.with_tx`.

### `run_rabbitmq_relay(conn: std.mysql.Connection, rabbit: std.mq.rabbitmq.Connection, batch_size: int) : int`

Берёт до `batch_size` записей со статусами `pending` и `failed`, публикует их в RabbitMQ и возвращает число успешно доставленных событий.

### `run_rabbitmq_relay_forever(conn: std.mysql.Connection, rabbit: std.mq.rabbitmq.Connection, batch_size: int, idle_delay_ms: int) : void`

Бесконечный relay loop с паузой между пустыми батчами.

### `mark_failed(conn: std.mysql.Connection, id: int, error: string) : bool`

Явно помечает запись как `failed`, увеличивает `attempts` и обновляет `last_error`.

## Пример

```aura
import std.db.mysql as db;
import std.json as json;
import std.outbox as outbox;

fn create_order(conn) : int {
    db.exec_stmt(conn, "insert into orders(id, status) values(?, ?)", [1, "created"]);
    return outbox.enqueue_json(conn, "jobs", "jobs", json.object([
        json.field("kind", json.encode_string("order.created"))
    ]));
};

var conn = db.open("host=127.0.0.1;port=3306;user=aura;password=aura_pass;database=aura_test");
outbox.ensure_schema(conn);
outbox.with_tx(conn, create_order);
db.close(conn);
```
