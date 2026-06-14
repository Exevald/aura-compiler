# Package std.config

`std.config` - user-facing пакет для чтения конфигурации сервиса из environment и runtime config surface.

## Функции

### `get(key: string) : string`

Возвращает строковое значение по ключу.

### `require(key: string) : string`

Возвращает значение по ключу и завершает программу через `std.log.Fatal`, если ключ отсутствует.

### `get_int(key: string, fallback: int) : int`

Читает целочисленное значение, иначе возвращает `fallback`.

### `get_bool(key: string, fallback: bool) : bool`

Читает булево значение, иначе возвращает `fallback`.

### `get_duration_ms(key: string, fallback: int) : int`

Читает timeout/duration в миллисекундах, иначе возвращает `fallback`.

## Пример

```aura
import std.config as config;
import std.io as io;

io.println("port", config.get_int("SERVICE_PORT", 8080));
io.println("debug", config.get_bool("DEBUG", false));
io.println("timeout", config.get_duration_ms("TIMEOUT_MS", 250));
```
