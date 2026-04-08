# aura-compiler

Компилятор языка Aura

## std-lib

### `std.core`

- `len(value)`
- `max(a, b)`
- `min(a, b)`
- `abs(value)`
- `sort(array)`
- `push(array, value)`
- `pop(array)`
- `concat(lhs, rhs)`
- `contains(haystack, needle)`
- `to_string(value)`
- `clamp(value, min, max)`

### `std.io`

- `print(...values)`
- `println(...values)`
- `printf(format, ...values)`

`printf` сейчас поддерживает `%v`, `%s`, `%d`, `%f`, `%t` и `%%`.

### `std.math`

- `max(a, b)`
- `min(a, b)`
- `abs(value)`
- `clamp(value, min, max)`

### `std.array`

- `len(array)`
- `sort(array)`
- `push(array, value)`
- `pop(array)`

### `std.text`

- `len(string)`
- `concat(lhs, rhs)`
- `contains(haystack, needle)`
- `to_string(value)`

### `std.log`

- `Error(...values)`
- `Warn(...values)`
- `Info(...values)`
- `Fatal(...values)`

### `std.memory`:

- `active_allocations()`
- `active_bytes()`
- `total_allocations()`
- `total_bytes()`
- `deep_size(value)`
- `is_send(value)`
- `is_sync(value)`
- `assert_send(value)`
- `assert_sync(value)`
- `assert_no_leaks()`
- `alloc(bytes)`
- `free(ptr)`

### `std.sync`:

- `current_thread()`
- `spawn()`
- `mutex()`
- `lock(thread, mutex)`
- `unlock(thread, mutex)`
- `would_deadlock(thread, mutex)`
- `assert_no_deadlock()`
- `join(waiting, target)`
- `finish(thread)`
- `is_locked(mutex)`
- `owner_id(mutex)`
- `thread_id(thread)`
- `thread_count()`
- `mutex_count()`
- `wait_edge_count()`

## Пример

```aura
module samples.main;

import std.math as math;
import std.array as arr;
import std.text as text;
import std.io as io;
import std.log as log;

var values = [8, 2];
arr.push(values, 5);
var sorted = arr.sort(values);

io.println(arr.len(sorted), arr.pop(sorted));
io.printf("clamped=%d", math.clamp(math.abs(-12), 0, 10));
log.Info(text.concat("au", "ra"));
```
