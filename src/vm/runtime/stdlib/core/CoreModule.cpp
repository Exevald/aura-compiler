#include "CoreModule.h"
#include "../../../core/values/ValueHelper.h"
#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

#include <algorithm>
#include <cmath>

namespace VM::Runtime
{

using Core::Value;
using Core::ValueHelper;
using Execution::ExecutionContext;

void CoreModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(ModuleName(), MakeModule(ModuleName()));

	runtime.DefineGlobal(
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

	runtime.DefineGlobal(
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

	runtime.DefineGlobal(
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

	runtime.DefineGlobal(
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

	runtime.DefineGlobal(
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
					std::vector<Value> elements;
					size_t size = 0;
					if (!ctx.GetArraySize(array, size))
					{
						ctx.RaiseError("std.core.sort expects an array");
						return std::monostate{};
					}
					elements.reserve(size);
					for (size_t i = 0; i < size; ++i)
					{
						Value value;
						if (!ctx.GetArrayElement(array, i, value))
						{
							ctx.RaiseError("std.core.sort failed to read transactional array state");
							return std::monostate{};
						}
						elements.push_back(std::move(value));
					}
					std::ranges::stable_sort(elements,
						[](const Value& lhs, const Value& rhs) {
							return CompareValues(lhs, rhs, "std.core") < 0;
						});
					if (!ctx.ReplaceArray(array, std::move(elements)))
					{
						ctx.RaiseError("std.core.sort failed to update transactional array state");
						return std::monostate{};
					}
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
					"std.core.push expects an array");
				if (!array)
				{
					return std::monostate{};
				}
				if (!ctx.PushArrayElement(array, args[1]))
				{
					ctx.RaiseError("std.core.push failed to update transactional array state");
					return std::monostate{};
				}
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
					"std.core.pop expects an array");
				if (!array)
				{
					return std::monostate{};
				}
				size_t size = 0;
				if (!ctx.GetArraySize(array, size) || size == 0)
				{
					ctx.RaiseError("std.core.pop expects a non-empty array");
					return std::monostate{};
				}
				Value value;
				if (!ctx.PopArrayElement(array, value))
				{
					ctx.RaiseError("std.core.pop failed to update transactional array state");
					return std::monostate{};
				}
				return value;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".remove_at",
		MakeNative(
			"remove_at",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto array = RequireArray(
					ctx,
					args[0],
					"std.core.remove_at expects an array");
				if (!array)
				{
					return std::monostate{};
				}

				const int64_t index = ValueHelper::As<int64_t>(args[1]);
				size_t size = 0;
				if (!ctx.GetArraySize(array, size))
				{
					ctx.RaiseError("std.core.remove_at expects an array");
					return std::monostate{};
				}
				if (index < 0 || static_cast<size_t>(index) >= size)
				{
					ctx.RaiseError("std.core.remove_at index is out of range");
					return std::monostate{};
				}

				std::vector<Value> elements;
				elements.reserve(size - 1);
				for (size_t i = 0; i < size; ++i)
				{
					if (i == static_cast<size_t>(index))
					{
						continue;
					}

					Value value;
					if (!ctx.GetArrayElement(array, i, value))
					{
						ctx.RaiseError("std.core.remove_at failed to read transactional array state");
						return std::monostate{};
					}
					elements.push_back(std::move(value));
				}

				if (!ctx.ReplaceArray(array, std::move(elements)))
				{
					ctx.RaiseError("std.core.remove_at failed to update transactional array state");
					return std::monostate{};
				}
				return array;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".remove",
		MakeNative(
			"remove",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto array = RequireArray(
					ctx,
					args[0],
					"std.core.remove expects an array");
				if (!array)
				{
					return std::monostate{};
				}

				size_t size = 0;
				if (!ctx.GetArraySize(array, size))
				{
					ctx.RaiseError("std.core.remove expects an array");
					return std::monostate{};
				}

				std::vector<Value> elements;
				elements.reserve(size);
				bool removed = false;
				for (size_t i = 0; i < size; ++i)
				{
					Value value;
					if (!ctx.GetArrayElement(array, i, value))
					{
						ctx.RaiseError("std.core.remove failed to read transactional array state");
						return std::monostate{};
					}
					if (!removed && ValueHelper::Equal(value, args[1]))
					{
						removed = true;
						continue;
					}
					elements.push_back(std::move(value));
				}

				if (!ctx.ReplaceArray(array, std::move(elements)))
				{
					ctx.RaiseError("std.core.remove failed to update transactional array state");
					return std::monostate{};
				}
				return array;
			}));

	runtime.DefineGlobal(
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

	runtime.DefineGlobal(
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

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".to_string",
		MakeNative(
			"to_string",
			1,
			[](ExecutionContext&, const std::vector<Value>& args) -> Value {
				return std::make_shared<const std::string>(ValueHelper::ToString(args[0]));
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".to_int",
		MakeNative(
			"to_int",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return ConvertToInt(ctx, args[0], "std.core");
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".to_float",
		MakeNative(
			"to_float",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return ConvertToFloat(ctx, args[0], "std.core");
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".to_bool",
		MakeNative(
			"to_bool",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return ConvertToBool(ctx, args[0], "std.core");
			}));

	runtime.DefineGlobal(
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
