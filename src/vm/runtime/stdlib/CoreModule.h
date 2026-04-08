#pragma once

namespace VM::Execution
{
class ExecutionContext;
}

namespace VM::Runtime
{

class CoreModule
{
public:
	static constexpr const char* ModuleName() { return "std.core"; }

	static void Install(Execution::ExecutionContext& context);
};

} // namespace VM::Runtime
