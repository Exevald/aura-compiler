#pragma once

#include "../../../core/values/Value.h"

extern "C"
{
#if __has_include(<rabbitmq-c/amqp.h>)
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/framing.h>
#else
#include <amqp.h>
#include <amqp_framing.h>
#endif
}

namespace VM::Core
{

inline amqp_connection_state_t ToRabbitMqConnection(const RabbitMqConnectionHandle& connection)
{
	return static_cast<amqp_connection_state_t>(connection.connection);
}

} // namespace VM::Core
