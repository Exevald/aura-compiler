# Package std.uuid

`std.uuid` предоставляет минимальный user-facing API для генерации UUID v4.

## Функции

### `new_v4() : string`

Возвращает UUID v4 в canonical lowercase string form:

`xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx`

## Пример

```aura
import std.io as io;
import std.uuid as uuid;

var request_id = uuid.new_v4();
io.println("request_id", request_id);
```
