# Package std.net

`std.net` - низкоуровневый сетевой пакет для blocking IPv4 TCP операций. Публичные handle-типы: `std.net.Listener` и `std.net.Connection`.

## Функции

### `listen(address: string, port: int) : NetListener`

Открывает listener socket на заданном IPv4-адресе и порту.

```aura
var listener = net.listen("127.0.0.1", 8080);
```

### `accept(listener: NetListener) : NetConnection`

Принимает входящее TCP-соединение.

```aura
var listener = net.listen("127.0.0.1", 8080);
var connection = net.accept(listener);
```

### `read(connection: NetConnection, max_bytes: int) : string`

Читает до `max_bytes` байт из соединения.

```aura
var listener = net.listen("127.0.0.1", 8080);
var connection = net.accept(listener);
var chunk = net.read(connection, 4096);
```

### `write(connection: NetConnection, payload: string) : int`

Записывает строковый payload в TCP-соединение.

```aura
var listener = net.listen("127.0.0.1", 8080);
var connection = net.accept(listener);
var sent = net.write(connection, "pong");
```

### `close(handle: NetHandle) : bool`

Закрывает listener или connection handle.

```aura
var listener = net.listen("127.0.0.1", 8080);
var connection = net.accept(listener);
net.close(connection);
net.close(listener);
```

### `local_port(handle: NetHandle) : int`

Возвращает локальный порт для listener или connection handle.

```aura
var listener = net.listen("127.0.0.1", 8080);
var port = net.local_port(listener);
```

### `set_nodelay(connection: NetConnection, enabled: bool) : bool`

Включает или выключает `TCP_NODELAY`.

```aura
var listener = net.listen("127.0.0.1", 8080);
var connection = net.accept(listener);
net.set_nodelay(connection, true);
```

### `set_read_timeout(connection: NetConnection, millis: int) : bool`

Устанавливает timeout чтения в миллисекундах.

```aura
var listener = net.listen("127.0.0.1", 8080);
var connection = net.accept(listener);
net.set_read_timeout(connection, 1000);
```

### `set_write_timeout(connection: NetConnection, millis: int) : bool`

Устанавливает timeout записи в миллисекундах.

```aura
var listener = net.listen("127.0.0.1", 8080);
var connection = net.accept(listener);
net.set_write_timeout(connection, 1000);
```

## Пример

```aura
import std.net as net;

var listener = net.listen("127.0.0.1", 8080);
var connection = net.accept(listener);
var request = net.read(connection, 4096);
net.write(connection, "ok");
net.close(connection);
net.close(listener);
```
