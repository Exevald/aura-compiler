# Package std.log

`std.log` предоставляет простое логирование по уровням. Все функции принимают вариадический набор runtime-значений.

## Функции

### `Error(...values: any) : void`

Логирует сообщение уровня error.

```aura
log.Error("db unavailable");
```

### `Warn(...values: any) : void`

Логирует сообщение уровня warning.

```aura
log.Warn("cache miss");
```

### `Info(...values: any) : void`

Логирует информационное сообщение.

```aura
log.Info("server started");
```

### `Fatal(...values: any) : void`

Логирует фатальное сообщение.

```aura
log.Fatal("cannot recover");
```

## Пример

```aura
import std.log as log;

log.Info("service started");
log.Warn("cache miss");
log.Error("cannot open config");
```
