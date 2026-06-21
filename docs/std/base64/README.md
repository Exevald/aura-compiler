# Package std.base64

`std.base64` предоставляет минимальный helper для URL-safe base64 decoding.

## Функции

### `try_url_decode(value: string) : [any]`

Пытается декодировать base64url строку и возвращает массив из трех элементов:

- `[0]` - `bool`, успешен ли decode
- `[1]` - decoded string
- `[2]` - текст ошибки, если decode не удался

```aura
var decoded = base64.try_url_decode("eyJzdWIiOiJ1c2VyLTEifQ");
print decoded[0];
print decoded[1];
```

## Пример

```aura
import std.base64 as base64;

var decoded = base64.try_url_decode("eyJva2V5Ijp0cnVlfQ");
if (decoded[0]) {
    print decoded[1];
}
```
