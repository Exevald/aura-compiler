#pragma once

namespace VM::Runtime
{
class SharedRuntime;
class MathModule
{
public:
	static constexpr const char* ModuleName() { return "std.math"; }
	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
