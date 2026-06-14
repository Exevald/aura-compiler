#include "RabbitMqModule.h"

#include "RabbitMqModuleInternal.h"
#include "../../NativeModuleSupport.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

extern "C"
{
#if __has_include(<rabbitmq-c/tcp_socket.h>)
#include <rabbitmq-c/tcp_socket.h>
#else
#include <amqp_tcp_socket.h>
#endif
}

namespace VM::Runtime
{

using Core::RabbitMqConnectionHandle;
using Core::RabbitMqConnectionPtr;
using Core::RabbitMqMessageHandle;
using Core::RabbitMqMessagePtr;
using Core::StringPtr;
using Core::Value;
using Execution::ExecutionContext;

namespace
{

constexpr int kDefaultChannel = 1;
constexpr int kDefaultHeartbeatSeconds = 30;
constexpr int kConsumePollTimeoutMicros = 200000;

RabbitMqConnectionPtr RequireConnection(
	ExecutionContext& ctx,
	const Value& value,
	const std::string& error)
{
	if (!std::holds_alternative<RabbitMqConnectionPtr>(value) || !std::get<RabbitMqConnectionPtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<RabbitMqConnectionPtr>(value);
}

RabbitMqMessagePtr RequireMessage(
	ExecutionContext& ctx,
	const Value& value,
	const std::string& error)
{
	if (!std::holds_alternative<RabbitMqMessagePtr>(value) || !std::get<RabbitMqMessagePtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<RabbitMqMessagePtr>(value);
}

std::string CopyAmqpBytes(const amqp_bytes_t bytes)
{
	if (bytes.bytes == nullptr || bytes.len == 0)
	{
		return {};
	}
	return {
		static_cast<const char*>(bytes.bytes),
		static_cast<size_t>(bytes.len)
	};
}

std::string ReplyToError(const amqp_rpc_reply_t& reply)
{
	switch (reply.reply_type)
	{
	case AMQP_RESPONSE_NORMAL:
		return {};
	case AMQP_RESPONSE_NONE:
		return "missing AMQP reply";
	case AMQP_RESPONSE_LIBRARY_EXCEPTION:
		return amqp_error_string2(reply.library_error);
	case AMQP_RESPONSE_SERVER_EXCEPTION:
		if (reply.reply.id == AMQP_CONNECTION_CLOSE_METHOD)
		{
			const auto* close = static_cast<const amqp_connection_close_t*>(reply.reply.decoded);
			if (close != nullptr)
			{
				return std::string(amqp_constant_name(close->reply_code)) + ": " + CopyAmqpBytes(close->reply_text);
			}
		}
		if (reply.reply.id == AMQP_CHANNEL_CLOSE_METHOD)
		{
			const auto* close = static_cast<const amqp_channel_close_t*>(reply.reply.decoded);
			if (close != nullptr)
			{
				return std::string(amqp_constant_name(close->reply_code)) + ": " + CopyAmqpBytes(close->reply_text);
			}
		}
		return "server exception";
	default:
		return "unknown AMQP reply";
	}
}

void SetConnectionError(const RabbitMqConnectionPtr& connection, const std::string& error)
{
	if (!connection)
	{
		return;
	}
	std::lock_guard lock(connection->mutex);
	connection->lastError = error;
}

std::string PrefixedError(const std::string_view operation, const std::string& detail)
{
	return "std.mq.rabbitmq." + std::string(operation) + " failed: " + detail;
}

bool EnsureConnectionOpen(const RabbitMqConnectionPtr& connection, const std::string_view operation)
{
	if (!connection)
	{
		return false;
	}
	if (connection->open && connection->connection != nullptr)
	{
		return true;
	}
	connection->lastError = "std.mq.rabbitmq." + std::string(operation) + " cannot use a closed connection";
	return false;
}

void DestroyConnectionUnlocked(RabbitMqConnectionHandle& connection)
{
	if (connection.connection == nullptr)
	{
		connection.open = false;
		return;
	}

	auto* state = Core::ToRabbitMqConnection(connection);
	(void)amqp_channel_close(state, connection.channel, AMQP_REPLY_SUCCESS);
	(void)amqp_connection_close(state, AMQP_REPLY_SUCCESS);
	(void)amqp_destroy_connection(state);
	connection.connection = nullptr;
	connection.open = false;
}

RabbitMqConnectionPtr OpenConnection(const std::string& url)
{
	auto connection = std::make_shared<RabbitMqConnectionHandle>();
	connection->url = url;
	connection->channel = kDefaultChannel;

	std::string mutableUrl = url;
	amqp_connection_info info{};
	amqp_default_connection_info(&info);
	if (amqp_parse_url(mutableUrl.data(), &info) != AMQP_STATUS_OK)
	{
		connection->lastError = PrefixedError("open", "invalid AMQP URL");
		return connection;
	}
	if (info.vhost == nullptr || std::string_view(info.vhost).empty())
	{
		info.vhost = const_cast<char*>("/");
	}

	amqp_connection_state_t state = amqp_new_connection();
	if (state == nullptr)
	{
		connection->lastError = PrefixedError("open", "could not allocate AMQP connection");
		return connection;
	}

	amqp_socket_t* socket = amqp_tcp_socket_new(state);
	if (socket == nullptr)
	{
		connection->lastError = PrefixedError("open", "could not create TCP socket");
		(void)amqp_destroy_connection(state);
		return connection;
	}

	const int socketStatus = amqp_socket_open(socket, info.host, info.port);
	if (socketStatus != AMQP_STATUS_OK)
	{
		connection->lastError = PrefixedError("open", amqp_error_string2(socketStatus));
		(void)amqp_destroy_connection(state);
		return connection;
	}

	const amqp_rpc_reply_t loginReply = amqp_login(
		state,
		info.vhost,
		0,
		AMQP_DEFAULT_FRAME_SIZE,
		kDefaultHeartbeatSeconds,
		AMQP_SASL_METHOD_PLAIN,
		info.user,
		info.password);
	if (loginReply.reply_type != AMQP_RESPONSE_NORMAL)
	{
		connection->lastError = PrefixedError("open", ReplyToError(loginReply));
		(void)amqp_destroy_connection(state);
		return connection;
	}

	(void)amqp_channel_open(state, connection->channel);
	const amqp_rpc_reply_t channelReply = amqp_get_rpc_reply(state);
	if (channelReply.reply_type != AMQP_RESPONSE_NORMAL)
	{
		connection->lastError = PrefixedError("open", ReplyToError(channelReply));
		(void)amqp_connection_close(state, AMQP_REPLY_SUCCESS);
		(void)amqp_destroy_connection(state);
		return connection;
	}

	connection->connection = state;
	connection->open = true;
	connection->lastError.clear();
	return connection;
}

bool DeclareQueueImpl(const RabbitMqConnectionPtr& connection, const std::string& queue)
{
	std::lock_guard lock(connection->mutex);
	if (!EnsureConnectionOpen(connection, "declare_queue"))
	{
		return false;
	}

	auto* state = Core::ToRabbitMqConnection(*connection);
	(void)amqp_queue_declare(
		state,
		connection->channel,
		amqp_cstring_bytes(queue.c_str()),
		0,
		1,
		0,
		0,
		amqp_empty_table);

	const amqp_rpc_reply_t reply = amqp_get_rpc_reply(state);
	if (reply.reply_type != AMQP_RESPONSE_NORMAL)
	{
		connection->lastError = PrefixedError("declare_queue", ReplyToError(reply));
		return false;
	}

	connection->lastError.clear();
	return true;
}

bool PublishImpl(const RabbitMqConnectionPtr& connection, const std::string& queue, const std::string& body)
{
	std::lock_guard lock(connection->mutex);
	if (!EnsureConnectionOpen(connection, "publish"))
	{
		return false;
	}

	auto* state = Core::ToRabbitMqConnection(*connection);
	amqp_basic_properties_t properties{};
	properties._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
	properties.content_type = amqp_cstring_bytes("text/plain");
	properties.delivery_mode = 2;

	amqp_bytes_t payload{};
	payload.len = body.size();
	payload.bytes = const_cast<char*>(body.data());

	const int status = amqp_basic_publish(
		state,
		connection->channel,
		amqp_empty_bytes,
		amqp_cstring_bytes(queue.c_str()),
		0,
		0,
		&properties,
		payload);

	if (status != AMQP_STATUS_OK)
	{
		connection->lastError = PrefixedError("publish", amqp_error_string2(status));
		return false;
	}

	connection->lastError.clear();
	return true;
}

std::optional<int64_t> ParsePrefetch(ExecutionContext& ctx, const Value& value)
{
	if (std::holds_alternative<int64_t>(value))
	{
		return std::max<int64_t>(1, std::get<int64_t>(value));
	}
	if (std::holds_alternative<double>(value))
	{
		return std::max<int64_t>(1, static_cast<int64_t>(std::get<double>(value)));
	}
	ctx.RaiseError("std.mq.rabbitmq.consume_with_options expects an integer prefetch value");
	return std::nullopt;
}

bool StartConsumeImpl(
	const RabbitMqConnectionPtr& connection,
	const std::string& queue,
	const int64_t prefetch)
{
	std::lock_guard lock(connection->mutex);
	if (!EnsureConnectionOpen(connection, "consume"))
	{
		return false;
	}

	auto* state = Core::ToRabbitMqConnection(*connection);
	(void)amqp_basic_qos(state, connection->channel, 0, static_cast<uint16_t>(prefetch), 0);
	amqp_rpc_reply_t reply = amqp_get_rpc_reply(state);
	if (reply.reply_type != AMQP_RESPONSE_NORMAL)
	{
		connection->lastError = PrefixedError("consume_with_options", ReplyToError(reply));
		return false;
	}

	(void)amqp_basic_consume(
		state,
		connection->channel,
		amqp_cstring_bytes(queue.c_str()),
		amqp_empty_bytes,
		0,
		0,
		0,
		amqp_empty_table);
	reply = amqp_get_rpc_reply(state);
	if (reply.reply_type != AMQP_RESPONSE_NORMAL)
	{
		connection->lastError = PrefixedError("consume_with_options", ReplyToError(reply));
		return false;
	}

	connection->lastError.clear();
	return true;
}

bool AckImpl(const RabbitMqMessagePtr& message, const bool negativeAck, const bool requeue)
{
	if (!message || message->acknowledged || !message->connection)
	{
		return false;
	}

	std::lock_guard lock(message->connection->mutex);
	if (!EnsureConnectionOpen(message->connection, negativeAck ? "nack" : "ack"))
	{
		return false;
	}

	auto* state = Core::ToRabbitMqConnection(*message->connection);
	const int status = negativeAck
		? amqp_basic_nack(state, message->channel, static_cast<uint64_t>(message->deliveryTag), 0, requeue ? 1 : 0)
		: amqp_basic_ack(state, message->channel, static_cast<uint64_t>(message->deliveryTag), 0);
	if (status != AMQP_STATUS_OK)
	{
		message->connection->lastError = PrefixedError(negativeAck ? "nack" : "ack", amqp_error_string2(status));
		return false;
	}

	message->acknowledged = true;
	message->connection->lastError.clear();
	return true;
}

Value ConsumeImpl(
	ExecutionContext& ctx,
	const RabbitMqConnectionPtr& connection,
	const std::string& queue,
	const int64_t prefetch,
	const Value& handler)
{
	if (!StartConsumeImpl(connection, queue, prefetch))
	{
		ctx.RaiseError(connection->lastError);
		return std::monostate{};
	}

	while (true)
	{
		amqp_connection_state_t state = nullptr;
		{
			std::lock_guard lock(connection->mutex);
			if (!connection->open || connection->connection == nullptr)
			{
				break;
			}
			state = Core::ToRabbitMqConnection(*connection);
		}

		amqp_envelope_t envelope{};
		timeval timeout {
			.tv_sec = 0,
			.tv_usec = kConsumePollTimeoutMicros
		};
		const amqp_rpc_reply_t reply = amqp_consume_message(state, &envelope, &timeout, 0);
		if (reply.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION && reply.library_error == AMQP_STATUS_TIMEOUT)
		{
			continue;
		}
		if (reply.reply_type != AMQP_RESPONSE_NORMAL)
		{
			SetConnectionError(connection, PrefixedError("consume_with_options", ReplyToError(reply)));
			ctx.RaiseError(connection->lastError);
			return std::monostate{};
		}

		auto message = std::make_shared<RabbitMqMessageHandle>();
		message->connection = connection;
		message->channel = static_cast<int>(envelope.channel);
		message->deliveryTag = static_cast<int64_t>(envelope.delivery_tag);
		message->queue = queue;
		message->routingKey = CopyAmqpBytes(envelope.routing_key);
		message->body = CopyAmqpBytes(envelope.message.body);

		Value callbackResult;
		const auto invokeError = ctx.InvokeCallable(handler, { message }, callbackResult);
		amqp_destroy_envelope(&envelope);
		if (invokeError)
		{
			SetConnectionError(connection, PrefixedError("consume_with_options", *invokeError));
			ctx.RaiseError(*invokeError);
			return std::monostate{};
		}
	}

	return std::monostate{};
}

} // namespace

void RabbitMqModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(
		std::string(ModuleName()),
		MakeModule(std::string(ModuleName())));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".open",
		MakeNative(
			"open",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto url = RequireString(ctx, args[0], "std.mq.rabbitmq.open expects a connection URL");
				if (!url)
				{
					return std::monostate{};
				}
				return OpenConnection(*url);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".close",
		MakeNative(
			"close",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto connection = RequireConnection(ctx, args[0], "std.mq.rabbitmq.close expects a connection handle");
				if (!connection)
				{
					return std::monostate{};
				}

				std::lock_guard lock(connection->mutex);
				const bool wasOpen = connection->open && connection->connection != nullptr;
				DestroyConnectionUnlocked(*connection);
				connection->lastError.clear();
				return wasOpen;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".declare_queue",
		MakeNative(
			"declare_queue",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto connection = RequireConnection(ctx, args[0], "std.mq.rabbitmq.declare_queue expects a connection handle");
				const auto queue = RequireString(ctx, args[1], "std.mq.rabbitmq.declare_queue expects a queue name");
				if (!connection || !queue)
				{
					return std::monostate{};
				}
				return DeclareQueueImpl(connection, *queue);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".publish",
		MakeNative(
			"publish",
			3,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto connection = RequireConnection(ctx, args[0], "std.mq.rabbitmq.publish expects a connection handle");
				const auto queue = RequireString(ctx, args[1], "std.mq.rabbitmq.publish expects a queue name");
				const auto body = RequireString(ctx, args[2], "std.mq.rabbitmq.publish expects a string payload");
				if (!connection || !queue || !body)
				{
					return std::monostate{};
				}
				return PublishImpl(connection, *queue, *body);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".consume",
		MakeNative(
			"consume",
			3,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto connection = RequireConnection(ctx, args[0], "std.mq.rabbitmq.consume expects a connection handle");
				const auto queue = RequireString(ctx, args[1], "std.mq.rabbitmq.consume expects a queue name");
				if (!connection || !queue)
				{
					return std::monostate{};
				}
				return ConsumeImpl(ctx, connection, *queue, 1, args[2]);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".consume_with_options",
		MakeNative(
			"consume_with_options",
			4,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto connection = RequireConnection(ctx, args[0], "std.mq.rabbitmq.consume_with_options expects a connection handle");
				const auto queue = RequireString(ctx, args[1], "std.mq.rabbitmq.consume_with_options expects a queue name");
				const auto prefetch = ParsePrefetch(ctx, args[2]);
				if (!connection || !queue || !prefetch)
				{
					return std::monostate{};
				}
				return ConsumeImpl(ctx, connection, *queue, *prefetch, args[3]);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".ack",
		MakeNative(
			"ack",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto message = RequireMessage(ctx, args[0], "std.mq.rabbitmq.ack expects a message handle");
				if (!message)
				{
					return std::monostate{};
				}
				return AckImpl(message, false, false);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".nack",
		MakeNative(
			"nack",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto message = RequireMessage(ctx, args[0], "std.mq.rabbitmq.nack expects a message handle");
				if (!message)
				{
					return std::monostate{};
				}
				if (!std::holds_alternative<bool>(args[1]))
				{
					ctx.RaiseError("std.mq.rabbitmq.nack expects a bool requeue flag");
					return std::monostate{};
				}
				return AckImpl(message, true, std::get<bool>(args[1]));
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".body",
		MakeNative(
			"body",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto message = RequireMessage(ctx, args[0], "std.mq.rabbitmq.body expects a message handle");
				if (!message)
				{
					return std::monostate{};
				}
				return std::make_shared<const std::string>(message->body);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".delivery_tag",
		MakeNative(
			"delivery_tag",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto message = RequireMessage(ctx, args[0], "std.mq.rabbitmq.delivery_tag expects a message handle");
				if (!message)
				{
					return std::monostate{};
				}
				return message->deliveryTag;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".routing_key",
		MakeNative(
			"routing_key",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto message = RequireMessage(ctx, args[0], "std.mq.rabbitmq.routing_key expects a message handle");
				if (!message)
				{
					return std::monostate{};
				}
				return std::make_shared<const std::string>(message->routingKey);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".error",
		MakeNative(
			"error",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto connection = RequireConnection(ctx, args[0], "std.mq.rabbitmq.error expects a connection handle");
				if (!connection)
				{
					return std::monostate{};
				}
				std::lock_guard lock(connection->mutex);
				return std::make_shared<const std::string>(connection->lastError);
			}));
}

} // namespace VM::Runtime
