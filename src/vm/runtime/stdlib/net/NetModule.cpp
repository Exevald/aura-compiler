#include "NetModule.h"
#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace VM::Runtime
{

using Core::ConnectionHandle;
using Core::ConnectionPtr;
using Core::ListenerHandle;
using Core::ListenerPtr;
using Core::Value;
using Execution::ExecutionContext;

namespace
{

ListenerPtr RequireListener(ExecutionContext& ctx, const Value& value, const std::string& error)
{
	if (!std::holds_alternative<ListenerPtr>(value) || !std::get<ListenerPtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<ListenerPtr>(value);
}

ConnectionPtr RequireConnection(ExecutionContext& ctx, const Value& value, const std::string& error)
{
	if (!std::holds_alternative<ConnectionPtr>(value) || !std::get<ConnectionPtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<ConnectionPtr>(value);
}

bool CloseListener(const ListenerPtr& listener)
{
	if (!listener)
	{
		return false;
	}
	std::lock_guard lock(listener->mutex);
	if (listener->fd < 0)
	{
		return false;
	}
	close(listener->fd);
	listener->fd = -1;
	return true;
}

bool CloseConnection(const ConnectionPtr& connection)
{
	if (!connection)
	{
		return false;
	}
	std::lock_guard lock(connection->mutex);
	if (connection->fd < 0)
	{
		return false;
	}
	::close(connection->fd);
	connection->fd = -1;
	return true;
}

int SocketLocalPort(const int fd)
{
	sockaddr_in addr{};
	socklen_t size = sizeof(addr);
	if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &size) != 0)
	{
		return -1;
	}
	return ntohs(addr.sin_port);
}

bool SetTimeoutOption(const int fd, const int option, const int64_t millis)
{
	timeval timeout{};
	timeout.tv_sec = static_cast<time_t>(millis / 1000);
	timeout.tv_usec = static_cast<suseconds_t>((millis % 1000) * 1000);
	return ::setsockopt(fd, SOL_SOCKET, option, &timeout, sizeof(timeout)) == 0;
}

} // namespace

void NetModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(
		std::string(ModuleName()),
		MakeModule(std::string(ModuleName())));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".listen",
		MakeNative(
			"listen",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto address = RequireString(
					ctx,
					args[0],
					"std.net.listen expects a string address");
				if (!address)
				{
					return std::monostate{};
				}

				const int64_t portValue = Core::ValueHelper::As<int64_t>(args[1]);
				if (portValue < 0 || portValue > 65535)
				{
					ctx.RaiseError("std.net.listen expects a port in range 0..65535");
					return std::monostate{};
				}

				const int fd = socket(AF_INET, SOCK_STREAM, 0);
				if (fd < 0)
				{
					ctx.RaiseError("std.net.listen failed to create socket");
					return std::monostate{};
				}

				int reuse = 1;
				::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

				sockaddr_in addr{};
#ifdef __APPLE__
				addr.sin_len = sizeof(addr);
#endif
				addr.sin_family = AF_INET;
				addr.sin_port = htons(static_cast<uint16_t>(portValue));
				if (inet_pton(AF_INET, address->c_str(), &addr.sin_addr) != 1)
				{
					close(fd);
					ctx.RaiseError("std.net.listen expects an IPv4 address");
					return std::monostate{};
				}

				if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
				{
					const auto error = std::string(std::strerror(errno));
					close(fd);
					ctx.RaiseError("std.net.listen failed to bind socket: " + error);
					return std::monostate{};
				}

				if (::listen(fd, 128) != 0)
				{
					const auto error = std::string(std::strerror(errno));
					::close(fd);
					ctx.RaiseError("std.net.listen failed to listen on socket: " + error);
					return std::monostate{};
				}

				auto listener = std::make_shared<ListenerHandle>();
				listener->fd = fd;
				listener->port = static_cast<uint16_t>(SocketLocalPort(fd));
				return listener;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".accept",
		MakeNative(
			"accept",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto listener = RequireListener(
					ctx,
					args[0],
					"std.net.accept expects a listener handle");
				if (!listener)
				{
					return std::monostate{};
				}

				std::lock_guard lock(listener->mutex);
				if (listener->fd < 0)
				{
					ctx.RaiseError("std.net.accept cannot use a closed listener");
					return std::monostate{};
				}

				sockaddr_in peer{};
#ifdef __APPLE__
				peer.sin_len = sizeof(peer);
#endif
				socklen_t size = sizeof(peer);
				const int fd = ::accept(listener->fd, reinterpret_cast<sockaddr*>(&peer), &size);
				if (fd < 0)
				{
					ctx.RaiseError("std.net.accept failed: " + std::string(std::strerror(errno)));
					return std::monostate{};
				}

				auto connection = std::make_shared<ConnectionHandle>();
				connection->fd = fd;
				connection->port = ntohs(peer.sin_port);
				return connection;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".read",
		MakeNative(
			"read",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto connection = RequireConnection(
					ctx,
					args[0],
					"std.net.read expects a connection handle");
				if (!connection)
				{
					return std::monostate{};
				}

				const int64_t maxBytes = Core::ValueHelper::As<int64_t>(args[1]);
				if (maxBytes <= 0 || maxBytes > 1'048'576)
				{
					ctx.RaiseError("std.net.read expects max_bytes in range 1..1048576");
					return std::monostate{};
				}

				std::string buffer(static_cast<size_t>(maxBytes), '\0');
				ssize_t readCount = 0;
				{
					std::lock_guard<std::mutex> lock(connection->mutex);
					if (connection->fd < 0)
					{
						ctx.RaiseError("std.net.read cannot use a closed connection");
						return std::monostate{};
					}
					readCount = ::recv(connection->fd, buffer.data(), buffer.size(), 0);
				}

				if (readCount < 0)
				{
					ctx.RaiseError("std.net.read failed: " + std::string(std::strerror(errno)));
					return std::monostate{};
				}

				buffer.resize(static_cast<size_t>(readCount));
				return std::make_shared<const std::string>(std::move(buffer));
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".write",
		MakeNative(
			"write",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto connection = RequireConnection(ctx, args[0], "std.net.write expects a connection handle");
				if (!connection)
				{
					return std::monostate{};
				}

				const auto data = RequireString(ctx, args[1], "std.net.write expects a string payload");
				if (!data)
				{
					return std::monostate{};
				}

				size_t sent = 0;
				std::lock_guard<std::mutex> lock(connection->mutex);
				if (connection->fd < 0)
				{
					ctx.RaiseError("std.net.write cannot use a closed connection");
					return std::monostate{};
				}

				while (sent < data->size())
				{
					const ssize_t wrote = ::send(
						connection->fd,
						data->data() + sent,
						data->size() - sent,
						0);
					if (wrote <= 0)
					{
						ctx.RaiseError(
							"std.net.write failed: " + std::string(std::strerror(errno)));
						return std::monostate{};
					}
					sent += static_cast<size_t>(wrote);
				}

				return static_cast<int64_t>(sent);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".close",
		MakeNative(
			"close",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				if (std::holds_alternative<ListenerPtr>(args[0]))
				{
					return CloseListener(std::get<ListenerPtr>(args[0]));
				}
				if (std::holds_alternative<ConnectionPtr>(args[0]))
				{
					return CloseConnection(std::get<ConnectionPtr>(args[0]));
				}

				ctx.RaiseError("std.net.close expects a listener or connection handle");
				return std::monostate{};
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".local_port",
		MakeNative(
			"local_port",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				if (std::holds_alternative<ListenerPtr>(args[0]))
				{
					const auto listener = std::get<ListenerPtr>(args[0]);
					if (!listener)
					{
						ctx.RaiseError("std.net.local_port expects a valid socket handle");
						return std::monostate{};
					}
					return listener->port;
				}
				if (std::holds_alternative<ConnectionPtr>(args[0]))
				{
					const auto connection = std::get<ConnectionPtr>(args[0]);
					if (!connection)
					{
						ctx.RaiseError("std.net.local_port expects a valid socket handle");
						return std::monostate{};
					}
					return static_cast<int64_t>(connection->port);
				}

				ctx.RaiseError("std.net.local_port expects a listener or connection handle");
				return std::monostate{};
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".set_nodelay",
		MakeNative(
			"set_nodelay",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto connection = RequireConnection(
					ctx,
					args[0],
					"std.net.set_nodelay expects a connection handle");
				if (!connection)
				{
					return std::monostate{};
				}

				if (!std::holds_alternative<bool>(args[1]))
				{
					ctx.RaiseError("std.net.set_nodelay expects a bool enabled flag");
					return std::monostate{};
				}

				const int enabled = std::get<bool>(args[1]) ? 1 : 0;
				std::lock_guard<std::mutex> lock(connection->mutex);
				if (connection->fd < 0)
				{
					ctx.RaiseError("std.net.set_nodelay cannot use a closed connection");
					return std::monostate{};
				}
				if (::setsockopt(connection->fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) != 0)
				{
					ctx.RaiseError("std.net.set_nodelay failed: " + std::string(std::strerror(errno)));
					return std::monostate{};
				}
				return true;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".set_read_timeout",
		MakeNative(
			"set_read_timeout",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto connection = RequireConnection(ctx, args[0], "std.net.set_read_timeout expects a connection handle");
				if (!connection)
				{
					return std::monostate{};
				}

				const int64_t millis = Core::ValueHelper::As<int64_t>(args[1]);
				if (millis < 0)
				{
					ctx.RaiseError("std.net.set_read_timeout expects a non-negative duration");
					return std::monostate{};
				}

				std::lock_guard<std::mutex> lock(connection->mutex);
				if (connection->fd < 0)
				{
					ctx.RaiseError("std.net.set_read_timeout cannot use a closed connection");
					return std::monostate{};
				}
				if (!SetTimeoutOption(connection->fd, SO_RCVTIMEO, millis))
				{
					ctx.RaiseError("std.net.set_read_timeout failed: " + std::string(std::strerror(errno)));
					return std::monostate{};
				}
				return true;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".set_write_timeout",
		MakeNative(
			"set_write_timeout",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto connection = RequireConnection(ctx, args[0], "std.net.set_write_timeout expects a connection handle");
				if (!connection)
				{
					return std::monostate{};
				}

				const int64_t millis = Core::ValueHelper::As<int64_t>(args[1]);
				if (millis < 0)
				{
					ctx.RaiseError("std.net.set_write_timeout expects a non-negative duration");
					return std::monostate{};
				}

				std::lock_guard<std::mutex> lock(connection->mutex);
				if (connection->fd < 0)
				{
					ctx.RaiseError("std.net.set_write_timeout cannot use a closed connection");
					return std::monostate{};
				}
				if (!SetTimeoutOption(connection->fd, SO_SNDTIMEO, millis))
				{
					ctx.RaiseError("std.net.set_write_timeout failed: " + std::string(std::strerror(errno)));
					return std::monostate{};
				}
				return true;
			}));
}

} // namespace VM::Runtime
