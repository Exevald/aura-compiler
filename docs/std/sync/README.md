# Package std.sync

`std.sync` предоставляет минимальные runtime-примитивы потоков и mutex. Публичные handle-типы: `std.sync.Thread` и `std.sync.Mutex`.

## Функции

### `current_thread() : ThreadHandle`

Возвращает handle текущего logical thread.

```aura
var current = sync.current_thread();
```

### `spawn() : ThreadHandle`

Создаёт новый logical thread handle.

```aura
var worker = sync.spawn();
```

### `mutex() : MutexHandle`

Создаёт новый mutex handle.

```aura
var m = sync.mutex();
```

### `lock(thread: ThreadHandle, mutex: MutexHandle) : bool`

Пытается захватить mutex от имени указанного thread handle.

```aura
var thread = sync.current_thread();
var mutex = sync.mutex();
sync.lock(thread, mutex);
```

### `unlock(thread: ThreadHandle, mutex: MutexHandle) : bool`

Освобождает mutex от имени указанного thread handle.

```aura
var thread = sync.current_thread();
var mutex = sync.mutex();
sync.lock(thread, mutex);
sync.unlock(thread, mutex);
```

### `join(waiting_thread: ThreadHandle, target_thread: ThreadHandle) : bool`

Регистрирует ожидание одного thread handle на завершение другого.

```aura
var waiting = sync.current_thread();
var target = sync.spawn();
sync.join(waiting, target);
```

### `finish(thread: ThreadHandle) : bool`

Помечает thread handle как завершённый.

```aura
var target = sync.spawn();
sync.finish(target);
```

## Пример

```aura
import std.sync as sync;

var mutex = sync.mutex();
var thread = sync.current_thread();
sync.lock(thread, mutex);
sync.unlock(thread, mutex);
```
