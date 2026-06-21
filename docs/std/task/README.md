# Package std.task

`std.task` предоставляет lightweight task API поверх runtime task scheduler.

## Функции

### `spawn(callee, ...args: any) : Task<any>`

Запускает callable в отдельной task.

- Аргументы передаются как есть.
- Возвращает task handle.

```aura
fn calc() : int { return 42; }
var work = task.spawn(calc);
```

### `join<T>(task_value: Task<T>) : T`

Ждет завершения task и возвращает ее результат.

- Если task завершилась с ошибкой, runtime поднимет ошибку.

```aura
var value = task.join(work);
```

### `cancel<T>(task_value: Task<T>) : bool`

Просит runtime отменить task.

```aura
var cancelled = task.cancel(work);
```

### `is_done<T>(task_value: Task<T>) : bool`

Проверяет, завершена ли task.

```aura
var done = task.is_done(work);
```

### `join_result<T>(task_value: Task<T>) : std.result.Result<T, string>`

Ждет task и возвращает explicit result вместо исключения.

- `Ok(value)` если task завершилась успешно.
- `Err(message)` если task завершилась с ошибкой.

```aura
var outcome = task.join_result(work);
print outcome.tag;
print outcome[0];
```

## Пример

```aura
import std.task as task;

fn calc() : int { return 42; }

var work = task.spawn(calc);
var answer = task.join(work);
```
