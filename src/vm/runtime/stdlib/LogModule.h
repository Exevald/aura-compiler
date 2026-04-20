#pragma once

namespace VM::Runtime
{
class SharedRuntime;
class LogModule
{
public:
	static constexpr const char* ModuleName() { return "std.log"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
