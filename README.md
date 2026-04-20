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
- `to_int(value)`
- `to_float(value)`
- `to_bool(value)`
- `clamp(value, min, max)`

### `std.io`

- `print(...values)`
- `println(...values)`
- `printf(format, ...values)`
- `read()`
- `readln()`

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
- `to_int(value)`
- `to_float(value)`
- `to_bool(value)`

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

## Что ещё не доведено

Отложено на отдельный этап:

- `effect`
- `handle`
- `actor`
- `transaction`
- полноценная frontend-семантика `unsafe` поверх более богатой ownership/effect model

## Editor Support

В репозитории есть базовая подсветка Aura для редакторов:

- VS Code: [editors/vscode](/Users/exevald/study/aura-compiler/editors/vscode)
- JetBrains TextMate bundle: [editors/jetbrains-textmate](/Users/exevald/study/aura-compiler/editors/jetbrains-textmate)

Подробности по установке и структуре лежат в [docs/editor-support.md](/Users/exevald/study/aura-compiler/docs/editor-support.md).
