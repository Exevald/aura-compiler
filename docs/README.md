# Aura Documentation

Документация Aura организована по образцу документации Go: есть краткая точка входа, отдельный reference по языку и отдельные страницы по пакетам стандартной библиотеки.

## Что такое Aura

Aura — компилируемый язык со статической грамматикой, модульной системой, базовой generic-поддержкой, встроенными контрактами, эффектами, actor-моделью и stdlib, ориентированной на сервисы и высоконагруженные системы. Репозиторий одновременно содержит:

- грамматику языка в `grammar.md`
- компилятор и VM runtime
- встроенные модули стандартной библиотеки
- runnable-примеры в `samples/`

## Навигация

- [Язык Aura](./language/README.md)
- [Стандартная библиотека](./std/README.md)
- [Деплой Aura-сервисов](./deployment/README.md)
- [Техническая презентация Aura](./AURA_PRESENTATION.md)

## Структура языка

Основные области языка:

- модули, `import` и `export`
- значения, типы и объявления
- функции, методы, generic-параметры и type aliases
- `struct`, `enum`, `interface`
- выражения, массивы, литералы и операторы
- `if`, `while`, `iter`, `return`, `print`, блоки
- `unsafe`, `ptr<T>`, `ref<T>`
- `requires`, `ensures`, `invariant`
- `effect`, `raises`, `handle`, `resume`
- `actor`, `msg`, `query`
- `comptime`
- `transaction`, `shared var`, `thread_local var`

## Стандартная библиотека

Публичные builtin-модули:

- `std.core`
- `std.io`
- `std.math`
- `std.array`
- `std.text`
- `std.log`
- `std.config`
- `std.backoff`
- `std.uuid`
- `std.sync`
- `std.env`
- `std.net`
- `std.db.mysql`
- `std.outbox`
- `std.mq.rabbitmq`
- `std.http.raw`
- `std.http.server`
- `std.time`
- `std.json`
- `std.service`

Internal runtime-модули вроде `std.*_native`, а также низкоуровневые диагностические поверхности памяти и внутренний MySQL bridge, не являются частью пользовательского API и импортируются только внутренним stdlib/runtime слоем.

## Источники истины

Документация в этой папке описывает текущую реализацию и опирается на:

- грамматику из `grammar.md`
- публичные builtin-спеки из `src/builtin/BuiltinModuleRegistry.h`
- примеры из `samples/`

Если пример, код runtime и старый текст в корневом `README.md` расходятся, приоритет у текущей реализации в коде.
