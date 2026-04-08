#pragma once

namespace VM::Execution
{
class ExecutionContext;
}

namespace VM::Runtime
{

class ArrayModule
{
public:
	static constexpr const char* ModuleName() { return "std.array"; }

	static void Install(Execution::ExecutionContext& context);
};

} // namespace VM::Runtime
