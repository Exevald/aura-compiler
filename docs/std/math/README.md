# Package std.math

`std.math` содержит компактный набор числовых helper-функций. По смыслу это узкое подмножество `std.core`.

## Функции

### `max(left: int, right: int) : int`

### `max(left: float, right: float) : float`

Возвращает большее из двух чисел.

```aura
var top = math.max(10, 12);
```

### `min(left: int, right: int) : int`

### `min(left: float, right: float) : float`

Возвращает меньшее из двух чисел.

```aura
var low = math.min(10, 12);
```

### `abs(value: int) : int`

### `abs(value: float) : float`

Возвращает абсолютное значение числа.

```aura
var distance = math.abs(-42);
```

### `clamp(value: int, low: int, high: int) : int`

### `clamp(value: float, low: float, high: float) : float`

Ограничивает число диапазоном `[low, high]`.

```aura
var clamped = math.clamp(15, 0, 10);
```

## Пример

```aura
import std.math as math;

var top = math.max(10, 12);
var absValue = math.abs(-3);
var clamped = math.clamp(15, 0, 10);
```
