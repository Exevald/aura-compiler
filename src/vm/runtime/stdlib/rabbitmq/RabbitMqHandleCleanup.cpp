#include "RabbitMqModuleInternal.h"

namespace VM::Core
{

RabbitMqConnectionHandle::~RabbitMqConnectionHandle()
{
	std::lock_guard lock(mutex);
	if (connection == nullptr)
	{
		open = false;
		return;
	}

	auto* state = ToRabbitMqConnection(*this);
	(void)amqp_channel_close(state, channel, AMQP_REPLY_SUCCESS);
	(void)amqp_connection_close(state, AMQP_REPLY_SUCCESS);
	(void)amqp_destroy_connection(state);
	connection = nullptr;
	open = false;
}

} // namespace VM::Core
