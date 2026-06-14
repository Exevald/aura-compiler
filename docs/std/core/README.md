# Package std.core

`std.core` содержит базовые runtime-утилиты общего назначения: длину, сравнение, массивные операции, строковые helpers и приведения типов.

## Функции

### `len(value: string) : int`

### `len<T>(value: [T]) : int`

Возвращает длину строки или число элементов массива.

```aura
var width = core.len("aura");
var count = core.len([1, 2, 3]);
```

### `max(left: int, right: int) : int`

### `max(left: float, right: float) : float`

Возвращает большее из двух чисел одного runtime-вида.

```aura
var limit = core.max(10, 12);
```

### `min(left: int, right: int) : int`

### `min(left: float, right: float) : float`

Возвращает меньшее из двух чисел одного runtime-вида.

```aura
var floor = core.min(10, 12);
```

### `abs(value: int) : int`

### `abs(value: float) : float`

Возвращает абсолютное значение числа.

```aura
var distance = core.abs(-42);
```

### `sort<T>(values: [T]) : [T]`

Сортирует массив на месте и возвращает тот же массив.

- В текущей реализации используется stable sort.
- Элементы должны быть взаимно сравнимы.

```aura
var xs = [3, 1, 2];
core.sort(xs);
```

### `push<T>(values: [T], value: T) : [T]`

Добавляет элемент в конец массива и возвращает массив.

```aura
var xs = [1, 2];
core.push(xs, 3);
```

### `pop<T>(values: [T]) : T`

Удаляет и возвращает последний элемент массива.

- Ожидает непустой массив.

```aura
var xs = [1, 2, 3];
var last = core.pop(xs);
```

### `concat(left: string, right: string) : string`

Конкатенирует две строки.

```aura
var full = core.concat("au", "ra");
```

### `contains(text: string, part: string) : bool`

Проверяет, содержит ли строка `text` подстроку `part`.

```aura
var ok = core.contains("aura", "ur");
```

### `to_string(value: any) : string`

Преобразует runtime-значение в строку.

```aura
var text = core.to_string(42);
```

### `to_int(value: any) : int`

Преобразует совместимое runtime-значение в `int`.

```aura
var count = core.to_int("42");
```

### `to_float(value: any) : float`

Преобразует совместимое runtime-значение в `float`.

```aura
var ratio = core.to_float("3.14");
```

### `to_bool(value: any) : bool`

Преобразует совместимое runtime-значение в `bool`.

```aura
var enabled = core.to_bool("true");
```

### `clamp(value: int, low: int, high: int) : int`

### `clamp(value: float, low: float, high: float) : float`

Ограничивает число диапазоном `[low, high]`.

```aura
var clamped = core.clamp(15, 0, 10);
```

## Пример

```aura
import std.core as core;

var xs = core.sort([3, 1, 2]);
var top = core.pop(xs);
var n = core.to_int("42");
var s = core.concat("au", "ra");
var clamped = core.clamp(15, 0, 10);
```
