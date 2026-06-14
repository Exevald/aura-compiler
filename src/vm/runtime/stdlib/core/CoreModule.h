#pragma once

namespace VM::Runtime
{
class SharedRuntime;

class CoreModule
{
public:
	static constexpr const char* ModuleName() { return "std.core_native"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
