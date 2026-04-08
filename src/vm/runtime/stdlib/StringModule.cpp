#include "StringModule.h"
#include "../../core/values/ValueHelper.h"
#include "../ExecutionContext.h"
#include "../NativeModuleSupport.h"

namespace VM::Runtime
{

using Core::Value;
using Core::ValueHelper;
using Execution::ExecutionContext;

void StringModule::Install(ExecutionContext& context)
{
	context.DefineGlobal(ModuleName(), MakeModule(ModuleName()));

	context.DefineGlobal(
		std::string(ModuleName()) + ".len",
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

	context.DefineGlobal(
		std::string(ModuleName()) + ".concat",
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

	context.DefineGlobal(
		std::string(ModuleName()) + ".contains",
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

	context.DefineGlobal(
		std::string(ModuleName()) + ".to_string",
		MakeNative(
			"to_string",
			1,
			[](ExecutionContext&, const std::vector<Value>& args) -> Value {
				return std::make_shared<const std::string>(ValueHelper::ToString(args[0]));
			}));
}

} // namespace VM::Runtime