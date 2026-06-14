#include "StringModule.h"
#include "../../../core/values/ValueHelper.h"
#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

namespace VM::Runtime
{

using Core::Value;
using Core::ValueHelper;
using Execution::ExecutionContext;

namespace
{

void InstallStringModulePrefix(SharedRuntime& runtime, const std::string& moduleName)
{
	runtime.DefineGlobal(moduleName, MakeModule(moduleName));

	runtime.DefineGlobal(
		moduleName + ".len",
		MakeNative(
			"len",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto value = RequireString(
					ctx,
					args[0],
					"std.text.len expects a string");
				if (!value)
				{
					return std::monostate{};
				}
				return static_cast<int64_t>(value->size());
			}));

	runtime.DefineGlobal(
		moduleName + ".concat",
		MakeNative(
			"concat",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto left = RequireString(
					ctx,
					args[0],
					"std.text.concat expects a string lhs");
				if (!left)
				{
					return std::monostate{};
				}
				const auto right = RequireString(
					ctx,
					args[1],
					"std.text.concat expects a string rhs");
				if (!right)
				{
					return std::monostate{};
				}
				return std::make_shared<const std::string>(*left + *right);
			}));

	runtime.DefineGlobal(
		moduleName + ".contains",
		MakeNative(
			"contains",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto haystack = RequireString(
					ctx,
					args[0],
					"std.text.contains expects a string haystack");
				if (!haystack)
				{
					return std::monostate{};
				}
				const auto needle = RequireString(
					ctx,
					args[1],
					"std.text.contains expects a string needle");
				if (!needle)
				{
					return std::monostate{};
				}
				return haystack->find(*needle) != std::string::npos;
			}));

	runtime.DefineGlobal(
		moduleName + ".starts_with",
		MakeNative(
			"starts_with",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto haystack = RequireString(
					ctx,
					args[0],
					"std.text.starts_with expects a string haystack");
				if (!haystack)
				{
					return std::monostate{};
				}
				const auto prefix = RequireString(
					ctx,
					args[1],
					"std.text.starts_with expects a string prefix");
				if (!prefix)
				{
					return std::monostate{};
				}
				return haystack->rfind(*prefix, 0) == 0;
			}));

	runtime.DefineGlobal(
		moduleName + ".index_of",
		MakeNative(
			"index_of",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto haystack = RequireString(
					ctx,
					args[0],
					"std.text.index_of expects a string haystack");
				if (!haystack)
				{
					return std::monostate{};
				}
				const auto needle = RequireString(
					ctx,
					args[1],
					"std.text.index_of expects a string needle");
				if (!needle)
				{
					return std::monostate{};
				}
				if (const size_t pos = haystack->find(*needle); pos != std::string::npos)
				{
					return static_cast<int64_t>(pos);
				}
				return int64_t{ -1 };
			}));

	runtime.DefineGlobal(
		moduleName + ".slice",
		MakeNative(
			"slice",
			3,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto value = RequireString(
					ctx,
					args[0],
					"std.text.slice expects a string");
				if (!value)
				{
					return std::monostate{};
				}
				const int64_t start = ValueHelper::As<int64_t>(args[1]);
				const int64_t length = ValueHelper::As<int64_t>(args[2]);
				if (start < 0 || length < 0 || static_cast<size_t>(start) > value->size())
				{
					ctx.RaiseError("std.text.slice expects valid start and length");
					return std::monostate{};
				}
				return std::make_shared<const std::string>(
					value->substr(static_cast<size_t>(start), static_cast<size_t>(length)));
			}));

	runtime.DefineGlobal(
		moduleName + ".split",
		MakeNative(
			"split",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto value = RequireString(
					ctx,
					args[0],
					"std.text.split expects a string value");
				if (!value)
				{
					return std::monostate{};
				}
				const auto delimiter = RequireString(
					ctx,
					args[1],
					"std.text.split expects a string delimiter");
				if (!delimiter)
				{
					return std::monostate{};
				}

				auto result = std::make_shared<Core::Array>();
				if (delimiter->empty())
				{
					for (const char ch : *value)
					{
						result->elements.push_back(std::make_shared<const std::string>(std::string(1, ch)));
					}
					return result;
				}

				size_t cursor = 0;
				while (true)
				{
					const size_t pos = value->find(*delimiter, cursor);
					if (pos == std::string::npos)
					{
						result->elements.push_back(std::make_shared<const std::string>(value->substr(cursor)));
						return result;
					}
					result->elements.push_back(
						std::make_shared<const std::string>(value->substr(cursor, pos - cursor)));
					cursor = pos + delimiter->size();
				}
			}));

	runtime.DefineGlobal(
		moduleName + ".to_string",
		MakeNative(
			"to_string",
			1,
			[](ExecutionContext&, const std::vector<Value>& args) -> Value {
				return std::make_shared<const std::string>(ValueHelper::ToString(args[0]));
			}));

	runtime.DefineGlobal(
		moduleName + ".to_int",
		MakeNative(
			"to_int",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return ConvertToInt(ctx, args[0], "std.text");
			}));

	runtime.DefineGlobal(
		moduleName + ".to_float",
		MakeNative(
			"to_float",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return ConvertToFloat(ctx, args[0], "std.text");
			}));

	runtime.DefineGlobal(
		moduleName + ".to_bool",
		MakeNative(
			"to_bool",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return ConvertToBool(ctx, args[0], "std.text");
			}));
}

} // namespace

void StringModule::Install(SharedRuntime& runtime)
{
	InstallStringModulePrefix(runtime, ModuleName());
}

} // namespace VM::Runtime
