#pragma once

#include "../../core/values/Value.h"

namespace VM::Execution
{
class ExecutionContext;
}

namespace VM::Runtime
{

class DiagnosticsModule
{
public:
	static constexpr const char* ModuleName() { return "std.memory"; }

	static void Install(Execution::ExecutionContext& context);
};

} // namespace VM::Runtime
