#pragma once

#include "../../SharedRuntime.h"

namespace VM::Runtime
{

class MySqlModule
{
public:
	static constexpr std::string_view ModuleName() { return "std.mysql_native"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
