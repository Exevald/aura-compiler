#include "MySqlModuleInternal.h"

#include "../../NativeModuleSupport.h"

namespace VM::Runtime
{

void MySqlModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(
		std::string(ModuleName()),
		MakeModule(std::string(ModuleName())));

	MySqlInternal::InstallConnectionBuiltins(runtime);
	MySqlInternal::InstallStatementBuiltins(runtime);
	MySqlInternal::InstallResultBuiltins(runtime);
}

} // namespace VM::Runtime
