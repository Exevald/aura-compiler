#pragma once

namespace VM::Execution
{
class ExecutionContext;
}

namespace VM::Runtime
{

class SyncModule
{
public:
	static constexpr const char* ModuleName() { return "std.sync"; }
	static void Install(Execution::ExecutionContext& context);
};

} // namespace VM::Runtime
