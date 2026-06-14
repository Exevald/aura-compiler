#include "LogModule.h"
#include "../../../core/values/ValueHelper.h"
#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

#include <iostream>

namespace VM::Runtime
{

using Core::Value;
using Core::ValueHelper;
using Execution::ExecutionContext;

namespace
{

std::string JoinArgs(const std::vector<Value>& args)
{
	std::string result;
	for (size_t i = 0; i < args.size(); ++i)
	{
		if (!result.empty())
		{
			result += " ";
		}
		result += ValueHelper::ToString(args[i]);
	}
	return result;
}

std::vector<Value> ExtractValuesArray(ExecutionContext& ctx, const Value& value, const std::string& domain)
{
	const auto array = RequireArray(ctx, value, domain + " expects an array of values");
	if (!array)
	{
		return {};
	}
	return array->elements;
}

Value EmitLog(
	ExecutionContext& ctx,
	const std::string& level,
	const Value& values,
	const bool fatal)
{
	const auto args = ExtractValuesArray(ctx, values, "std.log_native." + level);
	if (ctx.HasError())
	{
		return std::monostate{};
	}
	std::cout << "[" << level << "] " << JoinArgs(args) << "\n";
	if (fatal)
	{
		ctx.RaiseError("Fatal log invoked");
	}
	return std::monostate{};
}

} // namespace

void LogModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(ModuleName(), MakeModule(ModuleName()));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".Error",
		MakeNative(
			"Error",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return EmitLog(ctx, "ERROR", args[0], false);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".Warn",
		MakeNative(
			"Warn",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return EmitLog(ctx, "WARN", args[0], false);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".Info",
		MakeNative(
			"Info",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return EmitLog(ctx, "INFO", args[0], false);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".Fatal",
		MakeNative(
			"Fatal",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return EmitLog(ctx, "FATAL", args[0], true);
			}));
}

} // namespace VM::Runtime
