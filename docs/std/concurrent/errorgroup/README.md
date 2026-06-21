# Package std.concurrent.errorgroup

`std.concurrent.errorgroup` объединяет wait-group поведение с cancellation context.

## Типы

### `ErrorGroup`

Содержит список task-ов и связанный cancelable context.

## Функции

### `with_context(parent: std.context.Context) : ErrorGroup`

Создает error group, привязанную к переданному parent context.

### `background() : ErrorGroup`

Создает error group от `std.context.background()`.

### `group_context(group: ErrorGroup) : std.context.Context`

Возвращает context, который будет отменен при первом ошибочном task.

### `add(group: ErrorGroup, task_value: task.Task<void>) : void`

Добавляет существующую task.

### `spawn(group: ErrorGroup, callee, ...args: any) : task.Task<void>`

Запускает task и добавляет ее в group.

### `count(group: ErrorGroup) : int`

Возвращает число task-ов в group.

### `wait(group: ErrorGroup) : std.option.Option<string>`

Ждет все task-ы и возвращает первую ошибку, если она была.

- `Some(message)` при первой ошибке.
- `None`, если все task-ы завершились успешно.
- При первой ошибке group context отменяется.

```aura
var err = eg.wait(group);
print err.tag;
print err[0];
```

## Пример

```aura
import std.concurrent.errorgroup as eg;
import std.context as ctx;

var group = eg.with_context(ctx.background());
eg.spawn(group, worker);
var err = eg.wait(group);
```
