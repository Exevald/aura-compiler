#pragma once

namespace VM::Runtime
{
class SharedRuntime;
class StringModule
{
public:
	static constexpr const char* ModuleName() { return "std.text"; }
	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
