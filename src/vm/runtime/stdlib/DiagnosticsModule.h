#pragma once

#include "../../core/values/Value.h"

namespace VM::Runtime
{
class SharedRuntime;
class DiagnosticsModule
{
public:
	static constexpr const char* ModuleName() { return "std.memory"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
