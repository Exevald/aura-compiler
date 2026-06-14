# Package std.service

`std.service` содержит service-oriented helper-функции и небольшой in-memory store API. Публичный store handle имеет тип `std.service.Store`.
Store теперь безопасен для конкурентного доступа из нескольких HTTP request workers.

## Функции

### `store_new() : ServiceStore`

Создаёт новый пустой string-to-string store.

```aura
var store = service.store_new();
```

### `store_get(store: ServiceStore, key: string) : string`

Читает значение по ключу.

```aura
var store = service.store_new();
var value = service.store_get(store, "health");
```

### `store_set(store: ServiceStore, key: string, value: string) : int`

Записывает значение по ключу.

```aura
var store = service.store_new();
var code = service.store_set(store, "health", "ok");
```

### `store_delete(store: ServiceStore, key: string) : bool`

Удаляет запись по ключу.

```aura
var store = service.store_new();
service.store_set(store, "health", "ok");
var deleted = service.store_delete(store, "health");
```

### `store_len(store: ServiceStore) : int`

Возвращает текущее число записей в store.

```aura
var store = service.store_new();
service.store_set(store, "health", "ok");
var size = service.store_len(store);
```

### `shutdown_context() : std.context.Context`

Возвращает context, который автоматически отменяется при `SIGINT` или `SIGTERM`.

```aura
var shutdown = service.shutdown_context();
```

### `port_or(default_port: int) : int`

Возвращает значение переменной окружения `PORT` или переданный порт по умолчанию.

```aura
var port = service.port_or(8080);
```

### `host_or(default_host: string) : string`

Возвращает значение переменной окружения `HOST` или строку по умолчанию.

```aura
var host = service.host_or("127.0.0.1");
```

## Пример

```aura
import std.service as service;

var shutdown = service.shutdown_context();
var host = service.host_or("127.0.0.1");
var port = service.port_or(8080);
var store = service.store_new();
var code = service.store_set(store, "health", "ok");
var value = service.store_get(store, "health");
```
