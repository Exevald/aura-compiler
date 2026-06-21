# Package std.map

`std.map` предоставляет runtime map-обёртку для user-facing ассоциативных массивов.

## Функции

### `new<K, V>() : map[K]V`

Создает пустую map.

```aura
var values: map[string]int = maps.new();
```

### `len<K, V>(values: map[K]V) : int`

Возвращает число элементов в map.

```aura
var size = maps.len(values);
```

### `has<K, V>(values: map[K]V, key: K) : bool`

Проверяет наличие ключа.

- Поддерживаются string, int и bool ключи.

```aura
var exists = maps.has(values, "svc");
```

### `delete<K, V>(values: map[K]V, key: K) : bool`

Удаляет ключ из map и возвращает `true`, если запись существовала.

```aura
var deleted = maps.delete(values, "svc");
```

### `keys<K, V>(values: map[K]V) : [K]`

Возвращает массив ключей.

- Порядок соответствует runtime-итерации map и не должен считаться стабильным контрактом.

```aura
var names = maps.keys(values);
```

### `values<K, V>(values: map[K]V) : [V]`

Возвращает массив значений.

```aura
var payloads = maps.values(values);
```

## Пример

```aura
import std.map as maps;

var items: map[string]int = maps.new();
items["svc"] = 7;
var hasSvc = maps.has(items, "svc");
var size = maps.len(items);
var valuesOnly = maps.values(items);
```
