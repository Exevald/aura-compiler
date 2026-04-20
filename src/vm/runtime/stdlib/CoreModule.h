#pragma once

namespace VM::Runtime
{
class SharedRuntime;

class CoreModule
{
public:
	static constexpr const char* ModuleName() { return "std.core"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
