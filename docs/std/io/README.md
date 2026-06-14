# Package std.io

`std.io` предоставляет базовые функции консольного ввода-вывода.

## Функции

### `print(...values: any) : void`

Выводит переданные значения в стандартный вывод без обязательного перевода строки.

```aura
io.print("value =", 7);
```

### `println(...values: any) : void`

Выводит значения и завершает вывод переводом строки.

```aura
io.println("ready =", true);
```

### `printf(format: string, ...values: any) : void`

Форматированный вывод по строке формата.

```aura
io.printf("id=%d name=%s", 7, "aura");
```

### `read() : string`

Читает следующее token-oriented значение из стандартного ввода.

```aura
var token = io.read();
```

### `readln() : string`

Читает остаток текущей строки или следующую строку целиком.

```aura
var line = io.readln();
```

## Пример

```aura
import std.core as core;
import std.io as io;

io.print("value:");
var raw = io.read();
var n = core.to_int(raw);
io.println("parsed =", n);
```
