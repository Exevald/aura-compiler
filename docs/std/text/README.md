# Package std.text

`std.text` предоставляет строковые операции и текстовые преобразования. Это основной пакет для работы со строками в Aura.

## Функции

### `len(text: string) : int`

Возвращает длину строки.

```aura
var size = text.len("aura");
```

### `concat(left: string, right: string) : string`

Соединяет две строки.

```aura
var name = text.concat("au", "ra");
```

### `contains(text: string, part: string) : bool`

Проверяет наличие подстроки в строке.

```aura
var ok = text.contains("aura", "ra");
```

### `starts_with(text: string, prefix: string) : bool`

Возвращает `true`, если строка начинается с указанного префикса.

```aura
var ok = text.starts_with("aura", "au");
```

### `index_of(text: string, part: string) : int`

Возвращает индекс первого вхождения подстроки.

```aura
var pos = text.index_of("aura", "ra");
```

### `slice(text: string, start: int, end: int) : string`

Возвращает срез строки по диапазону индексов.

```aura
var piece = text.slice("aura", 1, 3);
```

### `split(text: string, separator: string) : [string]`

Разбивает строку по разделителю.

```aura
var parts = text.split("a,b,c", ",");
```

### `to_string(value: any) : string`

Преобразует runtime-значение в строку.

```aura
var raw = text.to_string(42);
```

### `to_int(value: any) : int`

Преобразует совместимое runtime-значение в `int`.

```aura
var n = text.to_int("42");
```

### `to_float(value: any) : float`

Преобразует совместимое runtime-значение в `float`.

```aura
var f = text.to_float("2.5");
```

### `to_bool(value: any) : bool`

Преобразует совместимое runtime-значение в `bool`.

```aura
var flag = text.to_bool("false");
```

## Пример

```aura
import std.text as text;

var full = text.concat("au", "ra");
var ok = text.starts_with(full, "au");
var idx = text.index_of(full, "ra");
var parts = text.split("a,b,c", ",");
```
