#include "HttpServerModule.h"
#include "HttpServerModuleInternal.h"

#include "../../NativeModuleSupport.h"

#include <algorithm>
#include <array>
#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace VM::Runtime
{

using Core::ConnectionPtr;
using Core::HttpRequestHandle;
using Core::HttpRequestPtr;
using Core::ListenerHandle;
using Core::ListenerPtr;
using Execution::ExecutionContext;

namespace
{

std::string ToLower(std::string value)
{
	std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return value;
}

std::string_view TrimView(std::string_view value)
{
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
	{
		value.remove_prefix(1);
	}
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
	{
		value.remove_suffix(1);
	}
	return value;
}

bool ParseContentLength(const std::string_view headerBlock, size_t& contentLength, std::string& error)
{
	contentLength = 0;
	size_t cursor = 0;
	while (cursor < headerBlock.size())
	{
		const size_t lineEnd = headerBlock.find("\r\n", cursor);
		const std::string_view line = lineEnd == std::string_view::npos
			? headerBlock.substr(cursor)
			: headerBlock.substr(cursor, lineEnd - cursor);
		cursor = lineEnd == std::string_view::npos ? headerBlock.size() : lineEnd + 2;
		if (line.empty())
		{
			continue;
		}

		const size_t colon = line.find(':');
		if (colon == std::string_view::npos)
		{
			error = "Malformed HTTP header";
			return false;
		}

		const std::string name = ToLower(std::string(TrimView(line.substr(0, colon))));
		const std::string_view value = TrimView(line.substr(colon + 1));
		if (name == "content-length")
		{
			try
			{
				contentLength = static_cast<size_t>(std::stoll(std::string(value)));
			}
			catch (const std::exception&)
			{
				error = "Invalid Content-Length header";
				return false;
			}
		}
		if (name == "transfer-encoding" && ToLower(std::string(value)) != "identity")
		{
			error = "Chunked or custom transfer encodings are not supported";
			return false;
		}
	}
	return true;
}

bool ParseRequestParts(
	const std::string_view request,
	std::string& method,
	std::string& path,
	std::string& body,
	std::unordered_map<std::string, std::string>& headers,
	std::string& error)
{
	const size_t headersEnd = request.find("\r\n\r\n");
	if (headersEnd == std::string_view::npos)
	{
		error = "Incomplete HTTP request";
		return false;
	}

	const size_t lineEnd = request.find("\r\n");
	if (lineEnd == std::string_view::npos)
	{
		error = "Malformed HTTP request line";
		return false;
	}

	const std::string_view requestLine = request.substr(0, lineEnd);
	const size_t firstSpace = requestLine.find(' ');
	const size_t secondSpace = requestLine.find(
		' ',
		firstSpace == std::string_view::npos ? firstSpace : firstSpace + 1);
	if (firstSpace == std::string_view::npos || secondSpace == std::string_view::npos)
	{
		error = "Malformed HTTP request line";
		return false;
	}

	method = std::string(requestLine.substr(0, firstSpace));
	path = std::string(requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1));
	if (const std::string_view version = requestLine.substr(secondSpace + 1);
		version != "HTTP/1.1" && version != "HTTP/1.0")
	{
		error = "Unsupported HTTP version";
		return false;
	}

	size_t contentLength = 0;
	const auto headerBlock = request.substr(lineEnd + 2, headersEnd - (lineEnd + 2));
	if (!ParseContentLength(
			headerBlock,
			contentLength,
			error))
	{
		return false;
	}

	headers.clear();
	size_t cursor = 0;
	while (cursor < headerBlock.size())
	{
		const size_t currentLineEnd = headerBlock.find("\r\n", cursor);
		const std::string_view line = currentLineEnd == std::string_view::npos
			? headerBlock.substr(cursor)
			: headerBlock.substr(cursor, currentLineEnd - cursor);
		cursor = currentLineEnd == std::string_view::npos ? headerBlock.size() : currentLineEnd + 2;
		if (line.empty())
		{
			continue;
		}

		const size_t colon = line.find(':');
		if (colon == std::string_view::npos)
		{
			error = "Malformed HTTP header";
			return false;
		}

		const auto name = ToLower(std::string(TrimView(line.substr(0, colon))));
		headers.insert_or_assign(name, std::string(TrimView(line.substr(colon + 1))));
	}

	const std::string_view bodyView = request.substr(headersEnd + 4);
	if (bodyView.size() < contentLength)
	{
		error = "Incomplete HTTP request body";
		return false;
	}

	body.assign(bodyView.substr(0, contentLength));
	return true;
}

bool ReadRequestFromSocket(
	const ConnectionPtr& connection,
	std::string& request,
	std::string& error)
{
	request.clear();
	std::array<char, 4096> buffer{};
	size_t contentLength = 0;
	bool contentLengthKnown = false;

	while (request.size() < 1'048'576)
	{
		const ssize_t received = ::recv(connection->fd, buffer.data(), buffer.size(), 0);
		if (received < 0)
		{
			error = "std.http.server_native failed to read from socket: " + std::string(std::strerror(errno));
			return false;
		}
		if (received == 0)
		{
			error = "connection closed before full request";
			return false;
		}

		request.append(buffer.data(), static_cast<size_t>(received));

		const size_t headersEnd = request.find("\r\n\r\n");
		if (headersEnd == std::string::npos)
		{
			continue;
		}

		if (!contentLengthKnown)
		{
			const size_t requestLineEnd = request.find("\r\n");
			if (requestLineEnd == std::string::npos || requestLineEnd > headersEnd)
			{
				error = "Malformed HTTP request line";
				return false;
			}
			if (!ParseContentLength(
					request.substr(requestLineEnd + 2, headersEnd - (requestLineEnd + 2)),
					contentLength,
					error))
			{
				return false;
			}
			contentLengthKnown = true;
		}

		if (request.size() >= headersEnd + 4 + contentLength)
		{
			request.resize(headersEnd + 4 + contentLength);
			return true;
		}
	}

	error = "request exceeds size limit";
	return false;
}

ListenerPtr BindListener(ExecutionContext& ctx, const std::string& address, const int64_t portValue)
{
	if (portValue < 0 || portValue > 65535)
	{
		ctx.RaiseError("std.http.server_native expects a port in range 0..65535");
		return {};
	}

	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		ctx.RaiseError("std.http.server_native failed to create socket");
		return {};
	}

	constexpr int reuse = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	sockaddr_in addr{};
#ifdef __APPLE__
	addr.sin_len = sizeof(addr);
#endif
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<uint16_t>(portValue));
	if (::inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1)
	{
		::close(fd);
		ctx.RaiseError("std.http.server_native expects an IPv4 address");
		return {};
	}

	if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
	{
		const auto error = std::string(std::strerror(errno));
		::close(fd);
		ctx.RaiseError("std.http.server_native failed to bind socket: " + error);
		return {};
	}

	if (::listen(fd, 128) != 0)
	{
		const auto error = std::string(std::strerror(errno));
		::close(fd);
		ctx.RaiseError("std.http.server_native failed to listen on socket: " + error);
		return {};
	}

	auto listener = std::make_shared<ListenerHandle>();
	listener->fd = fd;
	listener->port = static_cast<uint16_t>(portValue);
	return listener;
}

bool WaitForIncomingConnection(
	ExecutionContext& ctx,
	const ListenerPtr& listener,
	const Core::ContextPtr& shutdownContext)
{
	int fd = -1;
	{
		std::lock_guard lock(listener->mutex);
		if (listener->fd < 0)
		{
			ctx.RaiseError("std.http.server_native cannot use a closed listener");
			return false;
		}
		fd = listener->fd;
	}

	while (true)
	{
		if (shutdownContext && ctx.GetRuntime().IsContextCancelled(shutdownContext->id))
		{
			return false;
		}

		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(fd, &readSet);

		timeval timeout{};
		timeout.tv_sec = 0;
		timeout.tv_usec = 200000;
		const int ready = ::select(fd + 1, &readSet, nullptr, nullptr, &timeout);
		if (ready > 0)
		{
			return true;
		}
		if (ready == 0 || errno == EINTR)
		{
			continue;
		}

		ctx.RaiseError("std.http.server_native accept failed: " + std::string(std::strerror(errno)));
		return false;
	}
}

ConnectionPtr AcceptConnection(
	ExecutionContext& ctx,
	const ListenerPtr& listener,
	const Core::ContextPtr& shutdownContext)
{
	if (!WaitForIncomingConnection(ctx, listener, shutdownContext))
	{
		return {};
	}

	std::lock_guard lock(listener->mutex);
	if (listener->fd < 0)
	{
		ctx.RaiseError("std.http.server_native cannot use a closed listener");
		return {};
	}

	sockaddr_in peer{};
#ifdef __APPLE__
	peer.sin_len = sizeof(peer);
#endif
	socklen_t size = sizeof(peer);
	const int fd = ::accept(listener->fd, reinterpret_cast<sockaddr*>(&peer), &size);
	if (fd < 0)
	{
		if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return {};
		}
		ctx.RaiseError("std.http.server_native accept failed: " + std::string(std::strerror(errno)));
		return {};
	}

	auto connection = std::make_shared<Core::ConnectionHandle>();
	connection->fd = fd;
	connection->port = ntohs(peer.sin_port);
	return connection;
}

void ConfigureConnection(
	const ConnectionPtr& connection,
	const int64_t readTimeoutMs,
	const int64_t writeTimeoutMs)
{
	constexpr int enabled = 1;
	setsockopt(connection->fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));

	timeval readTimeout{};
	readTimeout.tv_sec = static_cast<time_t>(readTimeoutMs / 1000);
	readTimeout.tv_usec = static_cast<suseconds_t>((readTimeoutMs % 1000) * 1000);
	setsockopt(connection->fd, SOL_SOCKET, SO_RCVTIMEO, &readTimeout, sizeof(readTimeout));

	timeval writeTimeout{};
	writeTimeout.tv_sec = static_cast<time_t>(writeTimeoutMs / 1000);
	writeTimeout.tv_usec = static_cast<suseconds_t>((writeTimeoutMs % 1000) * 1000);
	setsockopt(connection->fd, SOL_SOCKET, SO_SNDTIMEO, &writeTimeout, sizeof(writeTimeout));
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
	::close(listener->fd);
	listener->fd = -1;
	return true;
}

HttpRequestPtr BuildRequestHandle(const std::string& method, const std::string& path, const std::string& body)
{
	auto request = std::make_shared<HttpRequestHandle>();
	request->method = method;
	request->path = path;
	request->body = body;
	return request;
}

} // namespace

namespace HttpServerInternal
{

bool ReadParsedRequest(
	ExecutionContext& ctx,
	const ConnectionPtr& connection,
	std::string& method,
	std::string& path,
	std::string& body,
	std::unordered_map<std::string, std::string>& headers)
{
	std::string requestBuffer;
	std::string error;
	{
		std::lock_guard lock(connection->mutex);
		if (connection->fd < 0)
		{
			ctx.RaiseError("std.http.server_native cannot use a closed connection");
			return false;
		}
		if (!ReadRequestFromSocket(connection, requestBuffer, error))
		{
			if (error == "connection closed before full request")
			{
				return true;
			}
			ctx.RaiseError("std.http.server_native failed: " + error);
			return false;
		}
	}

	if (!ParseRequestParts(requestBuffer, method, path, body, headers, error))
	{
		ctx.RaiseError("std.http.server_native failed: " + error);
		return false;
	}
	return true;
}

ListenerPtr OpenListener(ExecutionContext& ctx, const std::string& address, int64_t port)
{
	return BindListener(ctx, address, port);
}

ConnectionPtr NextConnection(
	ExecutionContext& ctx,
	const ListenerPtr& listener,
	const Core::ContextPtr& shutdownContext)
{
	return AcceptConnection(ctx, listener, shutdownContext);
}

void PrepareConnection(
	const ConnectionPtr& connection,
	const int64_t readTimeoutMs,
	const int64_t writeTimeoutMs)
{
	ConfigureConnection(connection, readTimeoutMs, writeTimeoutMs);
}

bool CloseConnectionHandle(const ConnectionPtr& connection)
{
	return CloseConnection(connection);
}

bool CloseListenerHandle(const ListenerPtr& listener)
{
	return CloseListener(listener);
}

HttpRequestPtr MakeRequestHandle(
	const std::string& method,
	const std::string& path,
	const std::string& body,
	const std::unordered_map<std::string, std::string>& headers)
{
	auto request = BuildRequestHandle(method, path, body);
	request->headers = headers;
	return request;
}

} // namespace HttpServerInternal

} // namespace VM::Runtime
