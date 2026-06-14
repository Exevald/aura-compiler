#include "TimeModule.h"
#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

#include <chrono>
#include <thread>

namespace VM::Runtime
{

using Core::Value;
using Execution::ExecutionContext;

void TimeModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(ModuleName(), MakeModule(ModuleName()));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".now_millis",
		MakeNative(
			"now_millis",
			0,
			[](ExecutionContext&, const std::vector<Value>&) -> Value {
				const auto now = std::chrono::system_clock::now().time_since_epoch();
				return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".monotonic_nanos",
		MakeNative(
			"monotonic_nanos",
			0,
			[](ExecutionContext&, const std::vector<Value>&) -> Value {
				const auto now = std::chrono::steady_clock::now().time_since_epoch();
				return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".sleep",
		MakeNative(
			"sleep",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				if (!std::holds_alternative<int64_t>(args[0]))
				{
					ctx.RaiseError("std.time_native.sleep expects an int millisecond value");
					return std::monostate{};
				}
				const auto millis = std::get<int64_t>(args[0]);
				if (millis < 0)
				{
					ctx.RaiseError("std.time_native.sleep expects a non-negative duration");
					return std::monostate{};
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(millis));
				return std::monostate{};
			}));
}

} // namespace VM::Runtime
