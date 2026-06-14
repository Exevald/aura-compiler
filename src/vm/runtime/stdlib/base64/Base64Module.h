#pragma once

namespace VM::Runtime
{
class SharedRuntime;

class Base64Module
{
public:
	static constexpr const char* ModuleName() { return "std.base64_native"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
