# Package std.array

`std.array` содержит операции над массивами. В отличие от `std.core`, этот пакет сфокусирован только на коллекциях.

## Функции

### `len<T>(values: [T]) : int`

Возвращает текущее количество элементов массива.

```aura
var count = arr.len([1, 2, 3]);
```

### `sort<T>(values: [T]) : [T]`

Сортирует массив на месте и возвращает его.

- В runtime используется stable sort.
- Элементы должны быть взаимно сравнимы.

```aura
var xs = [3, 2, 1];
arr.sort(xs);
```

### `push<T>(values: [T], value: T) : [T]`

Добавляет новый элемент в конец массива.

```aura
var xs = [1, 2];
arr.push(xs, 3);
```

### `pop<T>(values: [T]) : T`

Удаляет последний элемент и возвращает его.

- Ожидает непустой массив.

```aura
var xs = [1, 2, 3];
var tail = arr.pop(xs);
```

## Пример

```aura
import std.array as arr;

var xs = [3, 1, 2];
arr.push(xs, 4);
arr.sort(xs);
var last = arr.pop(xs);
```
