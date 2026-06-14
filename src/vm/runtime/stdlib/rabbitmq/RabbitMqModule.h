#pragma once

#include "../../SharedRuntime.h"

namespace VM::Runtime
{

class RabbitMqModule
{
public:
	static constexpr std::string_view ModuleName() { return "std.mq.rabbitmq_native"; }

	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
