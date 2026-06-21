# Package std.channel

`std.channel` предоставляет typed channel API для обмена значениями между task-ами.

## Функции

### `make<T>(capacity: int) : Channel<T>`

Создает канал с заданной вместимостью.

```aura
var c: ch.Channel<int> = ch.make(1);
```

### `send<T>(channel_value: Channel<T>, value: T) : bool`

Отправляет значение в канал.

- Возвращает `false`, если канал закрыт или runtime не смог доставить значение.

```aura
ch.send(c, 42);
```

### `recv<T>(channel_value: Channel<T>) : std.option.Option<T>`

Получает значение из канала.

- `Some(value)` если значение было получено.
- `None` если канал закрыт или value отсутствует.

```aura
var item = ch.recv(c);
print item.tag;
print item[0];
```

### `close<T>(channel_value: Channel<T>) : bool`

Закрывает канал.

```aura
ch.close(c);
```

## Пример

```aura
import std.channel as ch;

var c: ch.Channel<int> = ch.make(1);
ch.send(c, 42);
var item = ch.recv(c);
```
