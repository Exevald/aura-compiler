# Package std.backoff

`std.backoff` - минимальный user-facing пакет для retry/backoff policy в IO- и database-heavy сервисах.

## Типы

### `Policy`

Содержит `base_millis`, `factor`, `max_millis`.

## Функции

### `constant(delay_millis: int) : Policy`

Создаёт policy с постоянной задержкой.

### `exponential(base_millis: int, factor: int, max_millis: int) : Policy`

Создаёт exponential backoff policy.

### `delay(policy: Policy, attempt: int) : int`

Вычисляет задержку для номера попытки.

### `sleep(policy: Policy, attempt: int) : void`

Усыпляет текущий поток на вычисленную задержку.

### `retry(max_attempts: int, policy: Policy, action) : bool`

Запускает callback до `max_attempts` раз. Перед каждой попыткой policy вычисляет `wait_millis`, и callback получает его вторым аргументом. Если callback возвращает `true`, цикл завершается успехом.

## Пример

```aura
import std.backoff as backoff;
import std.io as io;

var policy = backoff.exponential(5, 2, 40);
io.println("attempt0", backoff.delay(policy, 0));
io.println("attempt3", backoff.delay(policy, 3));
var ok = backoff.retry(4, policy, fn(attempt: int, wait_millis: int) -> {
    io.println("attempt", attempt, "delay", wait_millis);
    return attempt >= 2;
});
io.println("success", ok);
```
