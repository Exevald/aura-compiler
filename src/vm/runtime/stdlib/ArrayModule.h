#pragma once

namespace VM::Runtime
{
class SharedRuntime;
class ArrayModule
{
public:
	static constexpr const char* ModuleName() { return "std.array"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
