#include "MathModule.h"
#include "../NativeModuleSupport.h"
#include "../SharedRuntime.h"

#include <cmath>

namespace VM::Runtime
{

using Core::Value;
using Execution::ExecutionContext;

void MathModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(ModuleName(), MakeModule(ModuleName()));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".max",
		MakeNative(
			"max",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				try
				{
					return CompareValues(args[0], args[1], "std.math") >= 0
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
					return CompareValues(args[0], args[1], "std.math") <= 0
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

				ctx.RaiseError("std.math.abs expects an int or float");
				return std::monostate{};
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".clamp",
		MakeNative(
			"clamp",
			3,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				try
				{
					if (CompareValues(args[1], args[2], "std.math") > 0)
					{
						ctx.RaiseError("std.math.clamp expects min <= max");
						return std::monostate{};
					}
					if (CompareValues(args[0], args[1], "std.math") < 0)
					{
						return args[1];
					}
					if (CompareValues(args[0], args[2], "std.math") > 0)
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
