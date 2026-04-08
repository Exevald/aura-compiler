#include "CoreModule.h"
#include "../../core/values/ValueHelper.h"
#include "../ExecutionContext.h"
#include "../NativeModuleSupport.h"

#include <algorithm>
#include <cmath>

namespace VM::Runtime
{

using Core::Value;
using Core::ValueHelper;
using Execution::ExecutionContext;

void CoreModule::Install(ExecutionContext& context)
{
	context.DefineGlobal(ModuleName(), MakeModule(ModuleName()));

	context.DefineGlobal(
		std::string(ModuleName()) + ".len",
		MakeNative(
			"len",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				if (const auto array = RequireArray(
						ctx,
						args[0],
						"std.core.len expects an array or string"))
				{
					return static_cast<int64_t>(array->elements.size());
				}

				ctx.ClearError();
				if (const auto stringValue = RequireString(
						ctx,
						args[0],
						"std.core.len expects an array or string"))
				{
					return static_cast<int64_t>(stringValue->size());
				}

				return std::monostate{};
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".max",
		MakeNative(
			"max",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				try
				{
					return CompareValues(
							   args[0],
							   args[1],
							   "std.core")
							>= 0
						? args[0]
						: args[1];
				}
				catch (const std::exception& e)
				{
					ctx.RaiseError(e.what());
					return std::monostate{};
				}
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".min",
		MakeNative(
			"min",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				try
				{
					return CompareValues(
							   args[0],
							   args[1],
							   "std.core")
							<= 0
						? args[0]
						: args[1];
				}
				catch (const std::exception& e)
				{
					ctx.RaiseError(e.what());
					return std::monostate{};
				}
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".abs",
		MakeNative(
			"abs",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				if (std::holds_alternative<int64_t>(args[0]))
				{
					return std::abs(std::get<int64_t>(args[0]));
				}
				if (std::holds_alternative<double>(args[0]))
				{
					return std::abs(std::get<double>(args[0]));
				}

				ctx.RaiseError("std.core.abs expects an int or float");
				return std::monostate{};
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".sort",
		MakeNative(
			"sort",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto array = RequireArray(
					ctx,
					args[0],
					"std.core.sort expects an array");
				if (!array)
				{
					return std::monostate{};
				}

				try
				{
					std::ranges::stable_sort(array->elements,
						[](const Value& lhs, const Value& rhs) {
							return CompareValues(lhs, rhs, "std.core") < 0;
						});
				}
				catch (const std::exception& e)
				{
					ctx.RaiseError(e.what());
					return std::monostate{};
				}

				return array;
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".push",
		MakeNative(
			"push",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto array = RequireArray(
					ctx,
					args[0],
					"std.core.push expects an array");
				if (!array)
				{
					return std::monostate{};
				}
				array->elements.push_back(args[1]);
				return array;
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".pop",
		MakeNative(
			"pop",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto array = RequireArray(
					ctx,
					args[0],
					"std.core.pop expects an array");
				if (!array)
				{
					return std::monostate{};
				}
				if (array->elements.empty())
				{
					ctx.RaiseError("std.core.pop expects a non-empty array");
					return std::monostate{};
				}
				Value value = array->elements.back();
				array->elements.pop_back();
				return value;
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".concat",
		MakeNative(
			"concat",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto left = RequireString(
					ctx,
					args[0],
					"std.core.concat expects a string lhs");
				if (!left)
				{
					return std::monostate{};
				}
				const auto right = RequireString(
					ctx,
					args[1],
					"std.core.concat expects a string rhs");
				if (!right)
				{
					return std::monostate{};
				}
				return std::make_shared<const std::string>(*left + *right);
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".contains",
		MakeNative(
			"contains",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto haystack = RequireString(
					ctx,
					args[0],
					"std.core.contains expects a string haystack");
				if (!haystack)
				{
					return std::monostate{};
				}
				const auto needle = RequireString(
					ctx,
					args[1],
					"std.core.contains expects a string needle");
				if (!needle)
				{
					return std::monostate{};
				}
				return haystack->find(*needle) != std::string::npos;
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".to_string",
		MakeNative(
			"to_string",
			1,
			[](ExecutionContext&, const std::vector<Value>& args) -> Value {
				return std::make_shared<const std::string>(ValueHelper::ToString(args[0]));
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".clamp",
		MakeNative(
			"clamp",
			3,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				try
				{
					if (CompareValues(args[1], args[2], "std.core") > 0)
					{
						ctx.RaiseError("std.core.clamp expects min <= max");
						return std::monostate{};
					}
					if (CompareValues(args[0], args[1], "std.core") < 0)
					{
						return args[1];
					}
					if (CompareValues(args[0], args[2], "std.core") > 0)
					{
						return args[2];
					}
					return args[0];
				}
				catch (const std::exception& e)
				{
					ctx.RaiseError(e.what());
					return std::monostate{};
				}
			}));
}

} // namespace VM::Runtime
