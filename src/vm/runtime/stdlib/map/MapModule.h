#pragma once

namespace VM::Runtime
{
class SharedRuntime;

class MapModule
{
public:
	static constexpr const char* ModuleName() { return "std.map_native"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
