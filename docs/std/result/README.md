# Package std.result

`std.result` определяет sum type для явного представления успешного или ошибочного результата.

## Типы

### `Result<T, E>`

Вариантный тип с двумя формами:

- `Ok(T)` для успешного результата
- `Err(E)` для ошибки

В runtime значение можно читать через:

- `.tag` для имени/индекса варианта
- `[0]` для payload у `Ok` или `Err`

## Конструкторы

### `Ok(value)`

Создает успешный `Result<T, E>`.

### `Err(error)`

Создает ошибочный `Result<T, E>`.

## Пример

```aura
import std.result as result;

var ok: result.Result<int, string> = result.Ok(7);
var err: result.Result<int, string> = result.Err("boom");
print ok.tag;
print ok[0];
```
