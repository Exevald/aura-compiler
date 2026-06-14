#include "BuiltinModuleInstaller.h"
#include "../../../builtin/BuiltinModuleRegistry.h"
#include "../SharedRuntime.h"
#include "core/CoreModule.h"
#include "crypto/CryptoModule.h"
#include "ctx/ContextModule.h"
#include "base64/Base64Module.h"
#include "channel/ChannelModule.h"
#include "env/EnvModule.h"
#include "http/HttpRawModule.h"
#include "http/HttpServerModule.h"
#include "io/IOModule.h"
#include "log/LogModule.h"
#include "map/MapModule.h"
#include "memory/MemoryModule.h"
#include "mysql/MySqlModule.h"
#include "net/NetModule.h"
#include "rabbitmq/RabbitMqModule.h"
#include "service/ServiceModule.h"
#include "string/StringModule.h"
#include "sync/SyncModule.h"
#include "task/TaskModule.h"
#include "time/TimeModule.h"
#include "uuid/UuidModule.h"
#include "json/JsonModule.h"

#include <stdexcept>

namespace VM::Runtime
{

void InstallBuiltinStdlib(SharedRuntime& runtime)
{
	for (const auto& module : Aura::Builtin::BuiltinModules())
	{
		if (!module.installNativeRuntime)
		{
			continue;
		}

		if (module.name == CoreModule::ModuleName())
		{
			CoreModule::Install(runtime);
		}
		else if (module.name == IOModule::ModuleName())
		{
			IOModule::Install(runtime);
		}
		else if (module.name == LogModule::ModuleName())
		{
			LogModule::Install(runtime);
		}
		else if (module.name == MapModule::ModuleName())
		{
			MapModule::Install(runtime);
		}
		else if (module.name == Base64Module::ModuleName())
		{
			Base64Module::Install(runtime);
		}
		else if (module.name == CryptoModule::ModuleName())
		{
			CryptoModule::Install(runtime);
		}
		else if (module.name == StringModule::ModuleName())
		{
			StringModule::Install(runtime);
		}
		else if (module.name == MemoryModule::ModuleName())
		{
			MemoryModule::Install(runtime);
		}
		else if (module.name == SyncModule::ModuleName())
		{
			SyncModule::Install(runtime);
		}
		else if (module.name == TaskModule::ModuleName())
		{
			TaskModule::Install(runtime);
		}
		else if (module.name == ChannelModule::ModuleName())
		{
			ChannelModule::Install(runtime);
		}
		else if (module.name == ContextModule::ModuleName())
		{
			ContextModule::Install(runtime);
		}
		else if (module.name == EnvModule::ModuleName())
		{
			EnvModule::Install(runtime);
		}
		else if (module.name == NetModule::ModuleName())
		{
			NetModule::Install(runtime);
		}
		else if (module.name == MySqlModule::ModuleName())
		{
			MySqlModule::Install(runtime);
		}
		else if (module.name == RabbitMqModule::ModuleName())
		{
			RabbitMqModule::Install(runtime);
		}
		else if (module.name == HttpRawModule::ModuleName())
		{
			HttpRawModule::Install(runtime);
		}
		else if (module.name == HttpServerModule::ModuleName())
		{
			HttpServerModule::Install(runtime);
		}
		else if (module.name == JsonModule::ModuleName())
		{
			JsonModule::Install(runtime);
		}
		else if (module.name == TimeModule::ModuleName())
		{
			TimeModule::Install(runtime);
		}
		else if (module.name == ServiceModule::ModuleName())
		{
			ServiceModule::Install(runtime);
		}
		else if (module.name == UuidModule::ModuleName())
		{
			UuidModule::Install(runtime);
		}
		else
		{
			throw std::runtime_error("Builtin runtime installer is missing for module: "
				+ std::string(module.name));
		}
	}
}

} // namespace VM::Runtime
