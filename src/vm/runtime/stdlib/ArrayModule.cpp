#include "ArrayModule.h"
#include "../NativeModuleSupport.h"
#include "../SharedRuntime.h"

#include <algorithm>

namespace VM::Runtime
{

using Core::Value;
using Execution::ExecutionContext;

void ArrayModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(ModuleName(), MakeModule(ModuleName()));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".len",
		MakeNative(
			"len",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto array = RequireArray(
					ctx,
					args[0],
					"std.array.len expects an array");
				if (!array)
				{
					return std::monostate{};
				}
				return static_cast<int64_t>(array->elements.size());
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".sort",
		MakeNative(
			"sort",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto array = RequireArray(
					ctx,
					args[0],
					"std.array.sort expects an array");
				if (!array)
				{
					return std::monostate{};
				}

				try
				{
					std::ranges::stable_sort(array->elements,
						[](const Value& lhs, const Value& rhs) {
							return CompareValues(lhs, rhs, "std.array") < 0;
						});
				}
				catch (const std::exception& e)
				{
					ctx.RaiseError(e.what());
					return std::monostate{};
				}

				return array;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".push",
		MakeNative(
			"push",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto array = RequireArray(
					ctx,
					args[0],
					"std.array.push expects an array");
				if (!array)
				{
					return std::monostate{};
				}
				array->elements.push_back(args[1]);
				return array;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".pop",
		MakeNative(
			"pop",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto array = RequireArray(
					ctx,
					args[0],
					"std.array.pop expects an array");
				if (!array)
				{
					return std::monostate{};
				}
				if (array->elements.empty())
				{
					ctx.RaiseError("std.array.pop expects a non-empty array");
					return std::monostate{};
				}

				Value value = array->elements.back();
				array->elements.pop_back();
				return value;
			}));
}

} // namespace VM::Runtime
