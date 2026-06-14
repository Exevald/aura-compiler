#pragma once

#include "../../SharedRuntime.h"

namespace VM::Runtime
{

class HttpRawModule
{
public:
	static constexpr std::string_view ModuleName() { return "std.http.raw_native"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
