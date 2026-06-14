#pragma once

namespace VM::Runtime
{
class SharedRuntime;

class ContextModule
{
public:
	static constexpr const char* ModuleName() { return "std.context_native"; }
	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
