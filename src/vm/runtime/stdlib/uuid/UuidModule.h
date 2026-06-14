#pragma once

#include "../../SharedRuntime.h"

namespace VM::Runtime
{

class UuidModule
{
public:
	static constexpr std::string_view ModuleName() { return "std.uuid_native"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
