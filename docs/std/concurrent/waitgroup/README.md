# Package std.concurrent.waitgroup

`std.concurrent.waitgroup` предоставляет простой счетчик task-ов, которые нужно дождаться.

## Типы

### `WaitGroup`

Хранит список task handle-ов, добавленных через `add` или `spawn`.

## Функции

### `new() : WaitGroup`

Создает пустой wait group.

```aura
var group = wg.new();
```

### `add(group: WaitGroup, task_value: task.Task<void>) : void`

Добавляет существующую task в wait group.

### `spawn(group: WaitGroup, callee, ...args: any) : task.Task<void>`

Запускает task и автоматически добавляет ее в wait group.

```aura
var task1 = wg.spawn(group, publish);
```

### `count(group: WaitGroup) : int`

Возвращает число зарегистрированных task-ов.

### `wait(group: WaitGroup) : void`

Ждет завершения всех task-ов из wait group.

## Пример

```aura
import std.concurrent.waitgroup as wg;

var group = wg.new();
wg.spawn(group, publish);
wg.wait(group);
```
