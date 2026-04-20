#pragma once

namespace VM::Runtime
{
class SharedRuntime;
class IOModule
{
public:
	static constexpr const char* ModuleName() { return "std.io"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
