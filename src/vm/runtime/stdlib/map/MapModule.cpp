#include "MapModule.h"

#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

#include <optional>

namespace VM::Runtime
{

using Core::Array;
using Core::MapHandle;
using Core::MapPtr;
using Core::Value;
using Execution::ExecutionContext;

namespace
{

std::optional<std::string> EncodeMapKey(const Value& value)
{
	if (std::holds_alternative<Core::StringPtr>(value))
	{
		const auto stringValue = std::get<Core::StringPtr>(value);
		return "s:" + (stringValue ? *stringValue : "");
	}
	if (std::holds_alternative<int64_t>(value))
	{
		return "i:" + std::to_string(std::get<int64_t>(value));
	}
	if (std::holds_alternative<bool>(value))
	{
		return std::get<bool>(value) ? "b:true" : "b:false";
	}
	return std::nullopt;
}

MapPtr RequireMap(
	ExecutionContext& ctx,
	const Value& value,
	const std::string& error)
{
	if (!std::holds_alternative<MapPtr>(value) || !std::get<MapPtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<MapPtr>(value);
}

} // namespace

void MapModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(std::string(ModuleName()), MakeModule(std::string(ModuleName())));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".new",
		MakeNative0(
			"new",
			[](ExecutionContext&) -> Value {
				return std::make_shared<MapHandle>();
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".len",
		MakeNative1(
			"len",
			[](ExecutionContext& ctx, const Value& mapValue) -> Value {
				const auto map = RequireMap(ctx, mapValue, "std.map.len expects a map");
				if (!map)
				{
					return std::monostate{};
				}

				const std::lock_guard lock(map->mutex);
				return static_cast<int64_t>(map->entries.size());
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".has",
		MakeNative2(
			"has",
			[](ExecutionContext& ctx, const Value& mapValue, const Value& keyValue) -> Value {
				const auto map = RequireMap(ctx, mapValue, "std.map.has expects a map");
				const auto encodedKey = EncodeMapKey(keyValue);
				if (!map || !encodedKey)
				{
					if (!encodedKey)
					{
						ctx.RaiseError("std.map.has expects string, int, or bool keys");
					}
					return std::monostate{};
				}

				const std::lock_guard lock(map->mutex);
				return map->entries.contains(*encodedKey);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".delete",
		MakeNative2(
			"delete",
			[](ExecutionContext& ctx, const Value& mapValue, const Value& keyValue) -> Value {
				const auto map = RequireMap(ctx, mapValue, "std.map.delete expects a map");
				const auto encodedKey = EncodeMapKey(keyValue);
				if (!map || !encodedKey)
				{
					if (!encodedKey)
					{
						ctx.RaiseError("std.map.delete expects string, int, or bool keys");
					}
					return std::monostate{};
				}

				const std::lock_guard lock(map->mutex);
				return map->entries.erase(*encodedKey) > 0;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".keys",
		MakeNative1(
			"keys",
			[](ExecutionContext& ctx, const Value& mapValue) -> Value {
				const auto map = RequireMap(ctx, mapValue, "std.map.keys expects a map");
				if (!map)
				{
					return std::monostate{};
				}

				auto array = std::make_shared<Array>();
				const std::lock_guard lock(map->mutex);
				array->elements.reserve(map->entries.size());
				for (const auto& [_, entry] : map->entries)
				{
					array->elements.push_back(entry.key);
				}
				return array;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".values",
		MakeNative1(
			"values",
			[](ExecutionContext& ctx, const Value& mapValue) -> Value {
				const auto map = RequireMap(ctx, mapValue, "std.map.values expects a map");
				if (!map)
				{
					return std::monostate{};
				}

				auto array = std::make_shared<Array>();
				const std::lock_guard lock(map->mutex);
				array->elements.reserve(map->entries.size());
				for (const auto& [_, entry] : map->entries)
				{
					array->elements.push_back(entry.value);
				}
				return array;
			}));
}

} // namespace VM::Runtime
