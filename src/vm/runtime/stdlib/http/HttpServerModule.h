#pragma once

#include "../../SharedRuntime.h"

namespace VM::Runtime
{

class HttpServerModule
{
public:
	static constexpr std::string_view ModuleName() { return "std.http.server_native"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
