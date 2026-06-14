#pragma once

namespace VM::Runtime
{
class SharedRuntime;

class TaskModule
{
public:
	static constexpr const char* ModuleName() { return "std.task_native"; }
	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
