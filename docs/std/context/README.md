# Package std.context

`std.context` предоставляет cancellation context для task- и service-oriented кода.

## Функции

### `background() : std.context.Context`

Создает корневой context без родителя.

```aura
var root = ctx.background();
```

### `with_cancel(parent: std.context.Context) : std.context.Context`

Создает child context, который можно отменить отдельно.

```aura
var child = ctx.with_cancel(root);
```

### `cancel(value: std.context.Context) : bool`

Отменяет context.

- Обычно применяется к root или к context, полученному через `with_cancel`.

```aura
ctx.cancel(child);
```

### `is_cancelled(value: std.context.Context) : bool`

Проверяет, был ли context отменен.

```aura
var done = ctx.is_cancelled(child);
```

## Пример

```aura
import std.context as ctx;

var root = ctx.background();
var child = ctx.with_cancel(root);
ctx.cancel(child);
```
