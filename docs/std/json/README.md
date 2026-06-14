# Package std.json

`std.json` - string-oriented JSON пакет. JSON values в API представлены строками, а функции строят или читают JSON без отдельного typed AST.

## Функции

### `is_valid(json_text: string) : bool`

Проверяет, является ли строка синтаксически корректным JSON.

### `compact(json_text: string) : string`

Возвращает компактную версию JSON-строки без лишних пробелов.

### `quote(text: string) : string`

Экранирует строку как JSON string literal content.

### `encode_string(text: string) : string`

Кодирует строку как JSON string value.

### `encode_number(value: int) : string`

### `encode_number(value: float) : string`

Кодирует число как JSON number literal.

### `encode_bool(value: bool) : string`

Кодирует булево значение как `true` или `false`.

### `null_literal() : string`

Возвращает JSON literal `null`.

### `field(key: string, value_json: string) : string`

Собирает JSON field snippet вида `"key":value`.

### `array(values: [string]) : string`

Собирает JSON array из массива готовых JSON value strings.

### `object(fields: [string]) : string`

Собирает JSON object из массива field snippets.

### `get_string(object_json: string, key: string) : string`

Извлекает top-level поле объекта и интерпретирует его как JSON string.

### `get_int(object_json: string, key: string) : int`

Извлекает top-level поле и интерпретирует его как целое число.

### `get_bool(object_json: string, key: string) : bool`

Извлекает top-level поле и интерпретирует его как `true` или `false`.

## Пример

```aura
import std.json as json;

var body = json.object([
    json.field("service", json.encode_string("aura")),
    json.field("ready", json.encode_bool(true)),
    json.field("count", json.encode_number(3))
]);

var ready = json.get_bool(body, "ready");
```
