# Package std.option

`std.option` определяет базовый sum type для значений, которые могут быть либо присутствующими, либо отсутствующими.

## Типы

### `Option<T>`

Вариантный тип с двумя формами:

- `Some(T)` для присутствующего значения
- `None` для отсутствия значения

В runtime значение можно читать через:

- `.tag` для имени/индекса варианта
- `[0]` для payload у `Some`

## Конструкторы

### `Some(value)`

Создает `Option<T>` со значением.

### `None()`

Создает пустой `Option<T>`.

## Пример

```aura
import std.option as option;

var value: option.Option<int> = option.Some(42);
var empty: option.Option<int> = option.None();
print value.tag;
print value[0];
```
