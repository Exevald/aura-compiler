#pragma once

namespace VM::Runtime
{
class SharedRuntime;

class ChannelModule
{
public:
	static constexpr const char* ModuleName() { return "std.channel_native"; }
	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime