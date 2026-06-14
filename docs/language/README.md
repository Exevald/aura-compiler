# Язык Aura

Этот документ описывает синтаксис и основные конструкции Aura на уровне пользователя языка.

## Файл, модуль и импорты

Каждый файл начинается с объявления модуля. Импорт поддерживает qualified name и локальный alias.

```aura
module samples.interfaces_exports.main;

import samples.interfaces_exports.api as api;
import std.io as io;
```

Экспортируются функции, типы и другие декларации, помеченные `export`.

```aura
module samples.interfaces_exports.api;

export type ReaderFactory<T> = T -> T;
export fn make_ready(value: int) : int {
    return value + 1;
};
```

## Объявления и область видимости

Aura поддерживает:

- `var`
- `const`
- `shared var`
- `thread_local var`

```aura
var counter: int = 1;
const limit: int = 10;
shared var sharedCounter: int = 0;
thread_local var cache: int = 41;
```

Блок `{ ... }` создаёт новую область видимости. Объявления могут быть на уровне модуля и внутри блоков.

## Типы

Встроенные типы:

- `int`
- `float`
- `bool`
- `string`
- `void`
- `never`

Составные типы:

- массивы: `[T]`
- maps: `map[K]V`
- указатели: `ptr<T>`
- ссылки: `ref<T>`
- tasks: `task<T>`
- channels: `channel<T>`
- function types: `T -> U`
- function types с несколькими аргументами: `(A, B) -> C`

```aura
type IntList = [int];
type Reducer<T> = (T, T) -> T;

var numbers: [int] = [1, 2, 3];
var labels = map[string]int{"answer": 42};
var address: ptr<int>;
```

## Литералы и значения

Поддерживаются:

- целые литералы
- литералы с плавающей точкой
- строки
- `true`, `false`
- `null`
- литералы массивов

```aura
var a = 42;
var b = 2.5;
var c = "hello";
var flags = [true, false];
var none = null;
```

## Функции

Обычная функция объявляется так:

```aura
fn add(left: int, right: int) : int {
    return left + right;
}
```

Также поддерживаются function expressions:

```aura
var pick_larger = fn(left: int, right: int) -> {
    if (left > right) {
        return left;
    }
    return right;
};
```

## Callable signatures

Aura использует несколько форм callable-сигнатур.

### Function declaration

```aura
fn name<T>(param1: A, param2: B = default) : R
with { ctx: Ctx }
raises { EffectA | EffectB }
requires (condition)
ensures (result_condition)
{
    ...
}
```

Ключевые части сигнатуры:

- имя функции
- optional generic-параметры
- список параметров
- optional тип результата
- optional `with`-контекст
- optional `raises`
- optional контракты `requires` и `ensures`

Если тип результата не указан, функция использует правила текущей грамматики для `type_guide_opt`; в публичной документации std-пакетов сигнатуры всегда пишутся с явным результатом.

### Method declaration

Метод внутри `struct` имеет ту же форму, но объявляется в теле типа:

```aura
struct Counter {
    value: int;

    fn increment(step: int) : int {
        self.value = self.value + step;
        return self.value;
    }
}
```

### Interface method signature

Методы интерфейса перечисляются без тела:

```aura
interface Reader<T> {
    fn read() : T;
    fn close() : void;
}
```

### Arrow function

Короткая callable-форма:

```aura
fn(left: int, right: int) -> left + right
fn(request: [string]) -> {
    return request[0];
}
```

### Function types

Тип функции записывается через `->`:

```aura
type Mapper<T> = T -> T;
type Reducer<T> = (T, T) -> T;
type RouteFn = ([string]) -> string;
```

### Actor callables

В `actor` используются две callable-формы:

```aura
actor Wallet {
    state balance: int = 0;

    msg deposit(amount: int) : void {
        balance = balance + amount;
    }

    query get_balance() : int {
        return balance;
    }
}
```

- `msg` описывает message handler
- `query` описывает read-oriented запрос

### Variadic functions

Последний параметр может быть variadic:

```aura
fn sum(seed: int, ...values: int) : int {
    var total = seed;
    iter (value of values) {
        total = total + value;
    }
    return total;
}

std.io.print(...values: any) : void
std.log.Info(...values: any) : void
```

## Generic-параметры и ограничения

Типы и функции могут иметь generic-параметры:

```aura
struct Box<T> {
    value: T;
}

fn choose<T>(left: T, right: T, reduce: (T, T) -> T) : T {
    return reduce(left, right);
}
```

Поддерживаются ограничения через `T: SomeType`:

```aura
fn double_value<T: int>(value: T) : int {
    return value + value;
}
```

## Type aliases

`type` создаёт alias для существующего типа.

```aura
type IntList = [int];
type ReaderFactory<T> = T -> T;
```

## Struct и методы

`struct` определяет составной пользовательский тип. Внутри struct можно объявлять методы.

```aura
struct Counter {
    value: int;

    fn increment() : int {
        self.value = self.value + 1;
        return self.value;
    }
}
```

Методы используют `self` для доступа к полям.

## Enum

`enum` поддерживает варианты без payload и с payload.

```aura
enum Option<T> { None | Some(T) }

var selected = Some(42);
print(selected.tag);
print(selected[0]);
```

Практически это означает:

- `value.tag` даёт имя или индекс варианта, доступный runtime
- `value[index]` даёт payload-поле варианта

## Interface

`interface` описывает набор методов. Значение совместимо с интерфейсом, если предоставляет нужные методы.

```aura
interface Reader<T> {
    fn read() : T;
}

struct Box {
    value: int;

    fn read() : int {
        return self.value;
    }
}

var reader: Reader<int> = Box(41);
```

## Выражения и операторы

Поддерживаются:

- присваивание `=`
- логические операции `and`, `or`
- сравнение `==`, `!=`, `<`, `<=`, `>`, `>=`
- арифметика `+`, `-`, `*`, `/`, `mod`, `div`
- унарные операции `-`, `+`, `not`
- address-of `&`
- dereference `*`

```aura
var total = (a + b * 2) / 3;
var ok = total > 0 and not false;
```

## Управляющие конструкции

### `if`

```aura
if (value > 0) {
    print("positive");
} else {
    print("non-positive");
}
```

### `while`

```aura
var i = 0;
while (i < 3) {
    i = i + 1;
}
```

### `iter`

`iter` используется для последовательного обхода коллекции с цепочкой адаптеров.

```aura
iter (item of values with [
    drop(1),
    take(3),
    reverse,
    filter(fn(v: int) -> v > 2),
    transform(fn(v: int) -> v + 10)
]) {
    print(item);
}
```

### `print`

Есть statement-форма `print expression`, а также stdlib-функции `std.io.print`, `std.io.println`, `std.io.printf`.

### `return`

Возвращает значение из функции. В функциях `void` может использоваться `return;`.

## Массивы и индексация

Массивы задаются как `[T]` и индексируются через `[]`.

```aura
var values = [10, 20, 30];
var first = values[0];
```

## Указатели, ссылки и `unsafe`

Операции с указателями и небезопасным доступом выполняются внутри `unsafe`.

```aura
fn set_to_ten(target: ref<int>) : void {
    *target = 10;
}

unsafe {
    var value: int = 4;
    var address: ptr<int> = &value;
    *address = *address + 3;
    set_to_ten(value);
}
```

`ptr<T>` используется для указателей, `ref<T>` - для передачи изменяемой ссылки на значение.

## Контракты и контекст

Aura поддерживает три формы контрактов:

- `requires`
- `ensures`
- `invariant`

Также функция может объявить требуемый контекст через `with`.

```aura
struct PositiveBox {
    value: int;
} invariant (self.value > 0)

fn divide(value: int, by: int) : int
with { logger: string }
requires (by != 0)
ensures (result <= value)
{
    return value / by;
}
```

`invariant` относится к типу, `requires` и `ensures` - к функции.

## Effects и handlers

Эффекты используются для явного описания внешних требований функции.

```aura
effect Input {
    fn read_number() : int;
}

fn compute() raises { Input } {
    print(read_number() + 35);
}

handle compute() with {
    effect read_number() -> {
        resume(7);
    }
}
```

Ключевые элементы:

- `effect` объявляет capability
- `raises` объявляет, что функция может поднять эффект
- `handle ... with { ... }` задаёт обработчик
- `resume(...)` продолжает выполнение с возвращаемым значением

## Actors

Aura поддерживает actor-ориентированные объявления:

```aura
actor Wallet {
    state balance: int = 0;

    msg deposit(amount: int) : void {
        balance = balance + amount;
    }

    query getBalance() : int {
        return balance;
    }
}
```

## Transactions

`transaction(...)` используется для областей, где несколько ресурсов должны обновляться согласованно.

```aura
shared var counter: int = 0;

transaction(shared counter) {
    counter = counter + 1;
}
```

## Tasks, `go` и `await`

`go` запускает callable с sendable-аргументами, а `await` получает результат.

```aura
fn add(left: int, right: int) : int {
    return left + right;
}

var pending = go add(19, 23);
var answer = await pending;
```

`std.task`, `std.channel`, `std.context`, wait groups и error groups дают
более явную библиотечную поверхность для concurrency и cancellation.

## Comptime

Aura поддерживает compile-time вычисления через `comptime fn` и `comptime` blocks.

```aura
comptime fn build_base(seed: int) : int {
    return seed * 2;
}

var folded: int = comptime {
    var base = build_base(3);
    return base + 1;
};
```

## Особенности runtime-модели

Часть языка тесно связана с VM runtime и builtin-модулями:

- networking и HTTP реализованы через runtime handles
- публичный MySQL API идёт через `std.db.mysql`, а runtime использует внутренние bridge-модули
- compile-time проверки памяти, `send`/`sync` и deadlock analysis работают под капотом, без необходимости импортировать internal runtime packages

Это означает, что часть builtin API работает либо с `any`, либо с opaque runtime handle-типами. В std-документации для читаемости могут использоваться имена вроде `std.net.Connection`, `std.mysql.Result` или `std.sync.Thread`.

## Связанные материалы

- [Документация стандартной библиотеки](../std/README.md)
- `grammar.md`
- `samples/`
