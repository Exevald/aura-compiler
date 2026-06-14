#pragma once

namespace VM::Runtime
{
class SharedRuntime;

class ServiceModule
{
public:
	static constexpr const char* ModuleName() { return "std.service_native"; }
	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
