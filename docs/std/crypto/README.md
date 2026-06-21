# Package std.crypto

`std.crypto` предоставляет две user-facing операции: HMAC-SHA256 signing и constant-time comparison.

## Функции

### `hmac_sha256_base64url(key: string, data: string) : string`

Вычисляет HMAC-SHA256 и возвращает результат в base64url-форме.

```aura
var signature = crypto.hmac_sha256_base64url("secret", "header.payload");
```

### `constant_time_equal(left: string, right: string) : bool`

Сравнивает две строки без раннего выхода по первому несовпадению.

- Подходит для проверок секретов, подписи и токенов.

```aura
var ok = crypto.constant_time_equal(signature, expected);
```

## Пример

```aura
import std.crypto as crypto;

var signature = crypto.hmac_sha256_base64url("secret", "payload");
var same = crypto.constant_time_equal(signature, signature);
```
