#include "DiagnosticsModule.h"
#include "../../core/values/ValueHelper.h"
#include "../ExecutionContext.h"
#include "../NativeModuleSupport.h"

#include <cstdint>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace VM::Runtime
{

using Core::ArrayPtr;
using Core::ClosurePtr;
using Core::EnumPtr;
using Core::FunctionPtr;
using Core::InstancePtr;
using Core::IteratorPtr;
using Core::ModulePtr;
using Core::NativeFunctionPtr;
using Core::PointerPtr;
using Core::StringPtr;
using Core::Value;
using Core::ValueHelper;
using Execution::ExecutionContext;

namespace
{

bool IsSendValue(const Value& value, std::unordered_set<const void*>& visited);
bool IsSyncValue(const Value& value, std::unordered_set<const void*>& visited);
int64_t EstimateDeepSize(const Value& value, std::unordered_set<const void*>& visited);

bool IsSendValue(const Value& value, std::unordered_set<const void*>& visited)
{
	return std::visit(
		[&]<typename T0>(const T0& current) -> bool {
			using T = std::decay_t<T0>;

			if constexpr (std::is_same_v<T, std::monostate> || std::is_same_v<T, bool>
				|| std::is_same_v<T, int64_t> || std::is_same_v<T, double> || std::is_same_v<T, StringPtr>
				|| std::is_same_v<T, FunctionPtr> || std::is_same_v<T, NativeFunctionPtr>
				|| std::is_same_v<T, ModulePtr>)
			{
				return true;
			}
			else if constexpr (std::is_same_v<T, PointerPtr> || std::is_same_v<T, IteratorPtr>)
			{
				return false;
			}
			else if constexpr (std::is_same_v<T, ArrayPtr>)
			{
				if (!current || !visited.insert(current.get()).second)
				{
					return true;
				}

				for (const auto& element : current->elements)
				{
					if (!IsSendValue(element, visited))
					{
						return false;
					}
				}
				return true;
			}
			else if constexpr (std::is_same_v<T, InstancePtr>)
			{
				if (!current || !visited.insert(current.get()).second)
				{
					return true;
				}

				for (const auto& field : current->fields)
				{
					if (!IsSendValue(field, visited))
					{
						return false;
					}
				}
				return true;
			}
			else if constexpr (std::is_same_v<T, EnumPtr>)
			{
				if (!current || !visited.insert(current.get()).second)
				{
					return true;
				}

				for (const auto& arg : current->args)
				{
					if (!IsSendValue(arg, visited))
					{
						return false;
					}
				}
				return true;
			}
			else if constexpr (std::is_same_v<T, ClosurePtr>)
			{
				if (!current || !visited.insert(current.get()).second)
				{
					return true;
				}

				for (const auto& capture : current->captures)
				{
					if (!IsSendValue(capture, visited))
					{
						return false;
					}
				}
				return true;
			}
			else
			{
				return false;
			}
		},
		value);
}

bool IsSyncValue(const Value& value, std::unordered_set<const void*>& visited)
{
	return std::visit(
		[&]<typename T0>(const T0& current) -> bool {
			using T = std::decay_t<T0>;

			if constexpr (std::is_same_v<T, std::monostate> || std::is_same_v<T, bool>
				|| std::is_same_v<T, int64_t> || std::is_same_v<T, double> || std::is_same_v<T, StringPtr>
				|| std::is_same_v<T, FunctionPtr> || std::is_same_v<T, NativeFunctionPtr>
				|| std::is_same_v<T, ModulePtr>)
			{
				return true;
			}
			else if constexpr (std::is_same_v<T, PointerPtr> || std::is_same_v<T, IteratorPtr>)
			{
				return false;
			}
			else if constexpr (std::is_same_v<T, ArrayPtr>)
			{
				if (!current || !visited.insert(current.get()).second)
				{
					return true;
				}

				for (const auto& element : current->elements)
				{
					if (!IsSyncValue(element, visited))
					{
						return false;
					}
				}
				return true;
			}
			else if constexpr (std::is_same_v<T, InstancePtr>)
			{
				if (!current || !visited.insert(current.get()).second)
				{
					return true;
				}

				for (const auto& field : current->fields)
				{
					if (!IsSyncValue(field, visited))
					{
						return false;
					}
				}
				return true;
			}
			else if constexpr (std::is_same_v<T, EnumPtr>)
			{
				if (!current || !visited.insert(current.get()).second)
				{
					return true;
				}

				for (const auto& arg : current->args)
				{
					if (!IsSyncValue(arg, visited))
					{
						return false;
					}
				}
				return true;
			}
			else if constexpr (std::is_same_v<T, ClosurePtr>)
			{
				if (!current || !visited.insert(current.get()).second)
				{
					return true;
				}

				for (const auto& capture : current->captures)
				{
					if (!IsSyncValue(capture, visited))
					{
						return false;
					}
				}
				return true;
			}
			else
			{
				return false;
			}
		},
		value);
}

int64_t EstimateDeepSize(const Value& value, std::unordered_set<const void*>& visited)
{
	return std::visit(
		[&]<typename T0>(const T0& current) -> int64_t {
			using T = std::decay_t<T0>;

			if constexpr (std::is_same_v<T, std::monostate>)
			{
				return 0;
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				return sizeof(bool);
			}
			else if constexpr (std::is_same_v<T, int64_t>)
			{
				return sizeof(int64_t);
			}
			else if constexpr (std::is_same_v<T, double>)
			{
				return sizeof(double);
			}
			else if constexpr (std::is_same_v<T, StringPtr>)
			{
				return current ? static_cast<int64_t>(current->size()) : 0;
			}
			else if constexpr (std::is_same_v<T, FunctionPtr> || std::is_same_v<T, NativeFunctionPtr>)
			{
				return sizeof(void*);
			}
			else if constexpr (std::is_same_v<T, ModulePtr>)
			{
				return current ? static_cast<int64_t>(current->name.size()) : 0;
			}
			else if constexpr (std::is_same_v<T, PointerPtr>)
			{
				return sizeof(void*);
			}
			else if constexpr (std::is_same_v<T, IteratorPtr>)
			{
				return sizeof(void*);
			}
			else if constexpr (std::is_same_v<T, ArrayPtr>)
			{
				if (!current || !visited.insert(current.get()).second)
				{
					return 0;
				}

				int64_t total = static_cast<int64_t>(sizeof(*current));
				for (const auto& element : current->elements)
				{
					total += EstimateDeepSize(element, visited);
				}
				return total;
			}
			else if constexpr (std::is_same_v<T, InstancePtr>)
			{
				if (!current || !visited.insert(current.get()).second)
				{
					return 0;
				}

				int64_t total = static_cast<int64_t>(sizeof(*current));
				for (const auto& field : current->fields)
				{
					total += EstimateDeepSize(field, visited);
				}
				return total;
			}
			else if constexpr (std::is_same_v<T, EnumPtr>)
			{
				if (!current || !visited.insert(current.get()).second)
				{
					return 0;
				}

				int64_t total = static_cast<int64_t>(sizeof(*current));
				for (const auto& arg : current->args)
				{
					total += EstimateDeepSize(arg, visited);
				}
				return total;
			}
			else if constexpr (std::is_same_v<T, ClosurePtr>)
			{
				if (!current || !visited.insert(current.get()).second)
				{
					return 0;
				}

				int64_t total = static_cast<int64_t>(sizeof(*current));
				for (const auto& capture : current->captures)
				{
					total += EstimateDeepSize(capture, visited);
				}
				return total;
			}
			else
			{
				return 0;
			}
		},
		value);
}

} // namespace

void DiagnosticsModule::Install(ExecutionContext& context)
{
	const auto installPrefix = [&context](const std::string& moduleName) {
		auto module = std::make_shared<VM::Core::Module>();
		module->name = moduleName;
		context.DefineGlobal(module->name, module);

		context.DefineGlobal(
			moduleName + ".active_allocations",
			MakeNative(
				"active_allocations",
				0,
				[](const ExecutionContext& ctx, const std::vector<Value>&) -> Value {
					return static_cast<int64_t>(ctx.GetAllocationStats().activeAllocations);
				}));

		context.DefineGlobal(
			moduleName + ".active_bytes",
			MakeNative(
				"active_bytes",
				0,
				[](const ExecutionContext& ctx, const std::vector<Value>&) -> Value {
					return static_cast<int64_t>(ctx.GetAllocationStats().activeBytes);
				}));

		context.DefineGlobal(
			moduleName + ".total_allocations",
			MakeNative(
				"total_allocations",
				0,
				[](const ExecutionContext& ctx, const std::vector<Value>&) -> Value {
					return static_cast<int64_t>(ctx.GetAllocationStats().totalAllocations);
				}));

		context.DefineGlobal(
			moduleName + ".total_bytes",
			MakeNative(
				"total_bytes",
				0,
				[](const ExecutionContext& ctx, const std::vector<Value>&) -> Value {
					return static_cast<int64_t>(ctx.GetAllocationStats().totalBytes);
				}));

		context.DefineGlobal(
			moduleName + ".deep_size",
			MakeNative(
				"deep_size",
				1,
				[](ExecutionContext&, const std::vector<Value>& args) -> Value {
					std::unordered_set<const void*> visited;
					return EstimateDeepSize(args[0], visited);
				}));

		context.DefineGlobal(
			moduleName + ".is_send",
			MakeNative(
				"is_send",
				1,
				[](ExecutionContext&, const std::vector<Value>& args) -> Value {
					std::unordered_set<const void*> visited;
					return IsSendValue(args[0], visited);
				}));

		context.DefineGlobal(
			moduleName + ".is_sync",
			MakeNative(
				"is_sync",
				1,
				[](ExecutionContext&, const std::vector<Value>& args) -> Value {
					std::unordered_set<const void*> visited;
					return IsSyncValue(args[0], visited);
				}));

		context.DefineGlobal(
			moduleName + ".assert_send",
			MakeNative(
				"assert_send",
				1,
				[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
					if (std::unordered_set<const void*> visited;
						!IsSendValue(args[0], visited))
					{
						ctx.RaiseError("Value is not Send-safe");
						return std::monostate{};
					}
					return true;
				}));

		context.DefineGlobal(
			moduleName + ".assert_sync",
			MakeNative(
				"assert_sync",
				1,
				[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
					if (std::unordered_set<const void*> visited;
						!IsSyncValue(args[0], visited))
					{
						ctx.RaiseError("Value is not Sync-safe");
						return std::monostate{};
					}
					return true;
				}));

		context.DefineGlobal(
			moduleName + ".assert_no_leaks",
			MakeNative(
				"assert_no_leaks",
				0,
				[](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
					if (ctx.GetAllocationStats().activeAllocations != 0)
					{
						ctx.RaiseError(
							"Memory leak detected: "
							+ std::to_string(ctx.GetAllocationStats().activeAllocations)
							+ " allocation(s) still active");
						return std::monostate{};
					}
					return true;
				}));

		context.DefineGlobal(
			moduleName + ".alloc",
			MakeNative(
				"alloc",
				1,
				[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
					const auto byteCount = ValueHelper::As<int64_t>(args[0]);
					if (byteCount <= 0)
					{
						ctx.RaiseError("alloc expects a positive byte count");
						return std::monostate{};
					}

					auto cell = std::make_shared<Value>(std::monostate{});
					auto alive = std::make_shared<bool>(true);
					const void* raw = ctx.Allocate(byteCount);

					auto ptr = std::make_shared<VM::Core::Pointer>();
					ptr->targetName = "&heap:" + std::to_string(reinterpret_cast<uintptr_t>(raw));
					ptr->get = [cell, alive]() -> Value {
						if (!*alive)
						{
							throw std::runtime_error("Use after free");
						}
						return *cell;
					};
					ptr->set = [cell, alive](Value value) {
						if (!*alive)
						{
							throw std::runtime_error("Use after free");
						}
						*cell = std::move(value);
					};
					ptr->deallocate = [&ctx, alive, raw]() {
						if (!*alive)
						{
							return false;
						}

						*alive = false;
						return ctx.Release(raw);
					};
					return ptr;
				}));

		context.DefineGlobal(
			moduleName + ".free",
			MakeNative(
				"free",
				1,
				[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
					if (!std::holds_alternative<PointerPtr>(args[0]))
					{
						ctx.RaiseError("free expects a pointer");
						return std::monostate{};
					}

					const auto& pointer = std::get<PointerPtr>(args[0]);
					if (!pointer || !pointer->deallocate)
					{
						ctx.RaiseError("Pointer is not owned by std.memory.alloc");
						return std::monostate{};
					}

					if (!pointer->deallocate())
					{
						ctx.RaiseError("Double free detected");
						return std::monostate{};
					}

					return std::monostate{};
				}));
	};

	installPrefix(ModuleName());
}

} // namespace VM::Runtime
