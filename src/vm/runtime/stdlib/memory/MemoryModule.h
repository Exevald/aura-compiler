#pragma once

namespace VM::Runtime
{

class SharedRuntime;

class MemoryModule
{
public:
	static constexpr const char* ModuleName() { return "std.memory_native"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
