#include "ServiceModule.h"

#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

namespace VM::Runtime
{

using Core::StringMapHandle;
using Core::StringMapPtr;
using Core::Value;
using Execution::ExecutionContext;

namespace
{

Core::StringPtr MakeSharedString(const std::string& value)
{
	return std::make_shared<const std::string>(value);
}

StringMapPtr RequireStore(
	ExecutionContext& ctx,
	const Value& value,
	const std::string& error)
{
	if (!std::holds_alternative<StringMapPtr>(value) || !std::get<StringMapPtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<StringMapPtr>(value);
}

} // namespace

void ServiceModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(
		std::string(ModuleName()),
		MakeModule(std::string(ModuleName())));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".store_new",
		MakeNative0(
			"store_new",
			[](ExecutionContext&) -> Value {
				return std::make_shared<StringMapHandle>();
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".store_get",
		MakeNative2(
			"store_get",
			[](ExecutionContext& ctx, const Value& storeValue, const Value& keyValue) -> Value {
				const auto store = RequireStore(
					ctx,
					storeValue,
					"std.service.store_get expects a store");
				const auto key = RequireString(
					ctx,
					keyValue,
					"std.service.store_get expects a string key");
				if (!store || !key)
				{
					return std::monostate{};
				}

				std::lock_guard lock(store->mutex);
				if (const auto it = store->values.find(*key); it != store->values.end())
				{
					return it->second ? Value(it->second) : Value(MakeSharedString(""));
				}
				return MakeSharedString("");
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".store_set",
		MakeNative3(
			"store_set",
			[](ExecutionContext& ctx,
				const Value& storeValue,
				const Value& keyValue,
				const Value& bodyValue) -> Value {
				const auto store = RequireStore(
					ctx,
					storeValue,
					"std.service.store_set expects a store");
				const auto key = RequireString(
					ctx,
					keyValue,
					"std.service.store_set expects a string key");
				const auto body = RequireString(
					ctx,
					bodyValue,
					"std.service.store_set expects a string value");
				if (!store || !key || !body)
				{
					return std::monostate{};
				}

				std::lock_guard lock(store->mutex);
				const auto [it, inserted]
					= store->values.insert_or_assign(*key, body);
				(void)it;
				return inserted ? int64_t{ 201 } : int64_t{ 200 };
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".store_delete",
		MakeNative2(
			"store_delete",
			[](ExecutionContext& ctx, const Value& storeValue, const Value& keyValue) -> Value {
				const auto store = RequireStore(
					ctx,
					storeValue,
					"std.service.store_delete expects a store");
				const auto key = RequireString(
					ctx,
					keyValue,
					"std.service.store_delete expects a string key");
				if (!store || !key)
				{
					return std::monostate{};
				}

				std::lock_guard lock(store->mutex);
				return store->values.erase(*key) > 0;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".store_len",
		MakeNative1(
			"store_len",
			[](ExecutionContext& ctx, const Value& storeValue) -> Value {
				const auto store = RequireStore(
					ctx,
					storeValue,
					"std.service.store_len expects a store");
				if (!store)
				{
					return std::monostate{};
				}

				std::lock_guard lock(store->mutex);
				return static_cast<int64_t>(store->values.size());
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".shutdown_context",
		MakeNative0(
			"shutdown_context",
			[](ExecutionContext& ctx) -> Value {
				return ctx.GetRuntime().ShutdownContext();
			}));
}

} // namespace VM::Runtime
