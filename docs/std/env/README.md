# Package std.env

`std.env` работает с переменными окружения процесса.

## Функции

### `get(key: string) : string`

Возвращает значение переменной окружения.

- Если переменная отсутствует, поведение определяется runtime как string-oriented lookup.
- Для сценариев с fallback обычно удобнее `get_or`.

```aura
var home = env.get("HOME");
```

### `get_or(key: string, fallback: string) : string`

Возвращает значение переменной окружения или `fallback`, если переменная не задана.

- Это основной helper для конфигурации приложений.

```aura
var host = env.get_or("HOST", "127.0.0.1");
```

### `has(key: string) : bool`

Проверяет, установлена ли переменная окружения.

```aura
var exists = env.has("PORT");
```

## Пример

```aura
import std.env as env;

var host = env.get_or("HOST", "127.0.0.1");
var hasPort = env.has("PORT");
```
