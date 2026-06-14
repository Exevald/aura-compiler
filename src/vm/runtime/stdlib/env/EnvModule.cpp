#include "EnvModule.h"
#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

#include <cstdlib>

namespace VM::Runtime
{

using Core::Value;
using Execution::ExecutionContext;

void EnvModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(ModuleName(), MakeModule(ModuleName()));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".get",
		MakeNative(
			"get",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto key = RequireString(ctx, args[0], "std.env.get expects a string key");
				if (!key)
				{
					return std::monostate{};
				}
				if (const char* raw = std::getenv(key->c_str()))
				{
					return std::make_shared<const std::string>(raw);
				}
				return std::make_shared<const std::string>("");
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".has",
		MakeNative(
			"has",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto key = RequireString(ctx, args[0], "std.env_native.has expects a string key");
				if (!key)
				{
					return std::monostate{};
				}
				return std::getenv(key->c_str()) != nullptr;
			}));
}

} // namespace VM::Runtime
