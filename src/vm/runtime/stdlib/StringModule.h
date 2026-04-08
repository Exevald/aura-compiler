#pragma once

namespace VM::Execution
{
class ExecutionContext;
}

namespace VM::Runtime
{

class StringModule
{
public:
	static constexpr const char* ModuleName() { return "std.text"; }
	static void Install(Execution::ExecutionContext& context);
};

} // namespace VM::Runtime
