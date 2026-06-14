#pragma once

#include "../../SharedRuntime.h"

namespace VM::Runtime
{

class NetModule
{
public:
	static constexpr std::string_view ModuleName() { return "std.net_native"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
