#include "LogModule.h"
#include "../../core/values/ValueHelper.h"
#include "../ExecutionContext.h"
#include "../NativeModuleSupport.h"

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

Value EmitLog(
	ExecutionContext& ctx,
	const std::string& level,
	const std::vector<Value>& args,
	const bool fatal)
{
	std::cout << "[" << level << "] " << JoinArgs(args) << "\n";
	if (fatal)
	{
		ctx.RaiseError("Fatal log invoked");
	}
	return std::monostate{};
}

} // namespace

void LogModule::Install(ExecutionContext& context)
{
	context.DefineGlobal(ModuleName(), MakeModule(ModuleName()));

	context.DefineGlobal(
		std::string(ModuleName()) + ".Error",
		MakeNative(
			"Error",
			0,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return EmitLog(ctx, "ERROR", args, false);
			},
			true));

	context.DefineGlobal(
		std::string(ModuleName()) + ".Warn",
		MakeNative(
			"Warn",
			0,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return EmitLog(ctx, "WARN", args, false);
			},
			true));

	context.DefineGlobal(
		std::string(ModuleName()) + ".Info",
		MakeNative(
			"Info",
			0,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return EmitLog(ctx, "INFO", args, false);
			},
			true));

	context.DefineGlobal(
		std::string(ModuleName()) + ".Fatal",
		MakeNative(
			"Fatal",
			0,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				return EmitLog(ctx, "FATAL", args, true);
			},
			true));
}

} // namespace VM::Runtime
