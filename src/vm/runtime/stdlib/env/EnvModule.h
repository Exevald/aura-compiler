#pragma once

namespace VM::Runtime
{
class SharedRuntime;

class EnvModule
{
public:
	static constexpr const char* ModuleName() { return "std.env_native"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
