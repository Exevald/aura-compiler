# Package std.db.mysql

`std.db.mysql` - основной публичный MySQL API в Aura. Пакет скрывает низкоуровневый runtime bridge и даёт пользовательские helpers для connection/pool lifecycle, транзакций и parameterized query execution.

## Функции

### `open(dsn: string) : MySqlConnection`

Открывает одиночное соединение.

### `open_pool(dsn: string, size: int) : MySqlPool`

Создаёт пул соединений фиксированного размера.

### `close(handle: any) : bool`

Закрывает connection или pool.

### `ping(handle: any) : bool`

Проверяет доступность connection или pool.

### `begin(handle: MySqlConnection) : bool`

### `commit(handle: MySqlConnection) : bool`

### `rollback(handle: MySqlConnection) : bool`

Низкоуровневые транзакционные операции. В прикладном коде чаще полезнее `with_tx`.

### `with_tx<T>(db_handle: MySqlConnection, callback: (MySqlConnection) -> T) : T`

Запускает callback внутри транзакции.

- Вызывает `begin`.
- Передаёт callback тот же `db_handle`.
- Если callback завершается успешно, вызывает `commit`.
- Если callback падает с runtime error, делает `rollback`.
- Если `commit` не проходит, тоже делает `rollback` и surface-ит ошибку.

```aura
import std.db.mysql as db;

var conn = db.open("mysql://root:secret@127.0.0.1:3306/app");
db.with_tx(conn, fn(handle) -> {
    return db.exec_stmt(handle, "delete from short_links where slug = ?", ["home"]);
});
```

### `query_one(db_handle: MySqlConnection, sql: string, params: [any]) : MySqlRow`

### `query_one(db_handle: MySqlPool, sql: string, params: [any]) : MySqlRow`

Выполняет подготовленный запрос с параметрами и возвращает одну строку.

```aura
import std.db.mysql as db;

var conn = db.open("mysql://root:secret@127.0.0.1:3306/app");
var row = db.query_one(conn, "select slug from short_links where slug = ?", ["home"]);
```

### `query_all(db_handle: MySqlConnection, sql: string, params: [any]) : [MySqlRow]`

### `query_all(db_handle: MySqlPool, sql: string, params: [any]) : [MySqlRow]`

Выполняет запрос с параметрами и материализует все строки в массив.

```aura
import std.db.mysql as db;

var conn = db.open("mysql://root:secret@127.0.0.1:3306/app");
var rows = db.query_all(conn, "select slug from short_links", []);
```

### `exec_stmt(db_handle: MySqlConnection, sql: string, params: [any]) : MySqlResult`

### `exec_stmt(db_handle: MySqlPool, sql: string, params: [any]) : MySqlResult`

Выполняет statement с параметрами и возвращает result handle.

```aura
import std.db.mysql as db;

var conn = db.open("mysql://root:secret@127.0.0.1:3306/app");
var result = db.exec_stmt(conn, "delete from short_links where slug = ?", ["home"]);
```

### `affected_rows(result: MySqlResult) : int`

### `last_insert_id(result: MySqlResult) : int`

### `get(row: MySqlRow, name: string) : any`

### `get_at(row: MySqlRow, index: int) : any`

### `is_null(value: any) : bool`

### `error(handle: any) : string`

Функции чтения результата и текстовых ошибок.

## Пример

```aura
import std.db.mysql as db;

var conn = db.open("mysql://root:secret@127.0.0.1:3306/app");
var row = db.query_one(conn, "select count(*) as total from short_links", []);
var rows = db.query_all(conn, "select slug from short_links where slug = ?", ["home"]);
var result = db.exec_stmt(conn, "delete from short_links where slug = ?", ["home"]);
print db.get(row, "total");
print db.affected_rows(result);
db.close(conn);
```
