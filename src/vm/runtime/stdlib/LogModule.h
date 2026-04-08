#pragma once

namespace VM::Execution
{
class ExecutionContext;
}

namespace VM::Runtime
{

class LogModule
{
public:
	static constexpr const char* ModuleName() { return "std.log"; }

	static void Install(Execution::ExecutionContext& context);
};

} // namespace VM::Runtime
