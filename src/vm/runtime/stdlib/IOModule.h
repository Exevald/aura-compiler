#pragma once

namespace VM::Execution
{
class ExecutionContext;
}

namespace VM::Runtime
{

class IOModule
{
public:
	static constexpr const char* ModuleName() { return "std.io"; }

	static void Install(Execution::ExecutionContext& context);
};

} // namespace VM::Runtime
