#pragma once

namespace VM::Runtime
{
class SharedRuntime;

class TimeModule
{
public:
	static constexpr const char* ModuleName() { return "std.time_native"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
