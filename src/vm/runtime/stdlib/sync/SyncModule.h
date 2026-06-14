#pragma once

namespace VM::Runtime
{
class SharedRuntime;
class SyncModule
{
public:
	static constexpr const char* ModuleName() { return "std.sync_native"; }
	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
