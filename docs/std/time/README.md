# Package std.time

`std.time` предоставляет базовые wall-clock и monotonic time helpers.

## Функции

### `now_millis() : int`

Возвращает текущее системное время в миллисекундах с начала Unix epoch.

- Подходит для timestamp-oriented логики и простых временных меток.

```aura
var ts = time.now_millis();
```

### `monotonic_nanos() : int`

Возвращает monotonic time в наносекундах.

- Используйте для измерения интервалов.
- В отличие от wall-clock времени не зависит от изменения системных часов.

```aura
var started = time.monotonic_nanos();
```

### `sleep(millis: int) : void`

Приостанавливает выполнение текущего потока на указанное число миллисекунд.

- Это blocking sleep.

```aura
time.sleep(100);
```

## Пример

```aura
import std.time as time;

var started = time.monotonic_nanos();
time.sleep(50);
var now = time.now_millis();
```
