#pragma once

namespace VM::Execution
{
class ExecutionContext;
}

namespace VM::Runtime
{

class MathModule
{
public:
	static constexpr const char* ModuleName() { return "std.math"; }
	static void Install(Execution::ExecutionContext& context);
};

} // namespace VM::Runtime
