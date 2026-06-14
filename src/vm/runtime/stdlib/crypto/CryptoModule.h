#pragma once

namespace VM::Runtime
{
class SharedRuntime;

class CryptoModule
{
public:
	static constexpr const char* ModuleName() { return "std.crypto_native"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
