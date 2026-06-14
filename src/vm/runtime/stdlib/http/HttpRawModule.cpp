#include "HttpRawModule.h"
#include "../../NativeModuleSupport.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <sys/socket.h>

namespace VM::Runtime
{

using Core::Array;
using Core::ArrayPtr;
using Core::ConnectionPtr;
using Core::Value;
using Execution::ExecutionContext;

namespace
{

ConnectionPtr RequireConnection(ExecutionContext& ctx, const Value& value, const std::string& error)
{
	if (!std::holds_alternative<ConnectionPtr>(value) || !std::get<ConnectionPtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<ConnectionPtr>(value);
}

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
	if (!ParseContentLength(
			request.substr(lineEnd + 2, headersEnd - (lineEnd + 2)),
			contentLength,
			error))
	{
		return false;
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
			error = "std.http.raw.read_request failed to read from socket";
			return false;
		}
		if (received == 0)
		{
			error = "std.http.raw.read_request connection closed before full request";
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
					request.substr(
						requestLineEnd + 2,
						headersEnd - (requestLineEnd + 2)),
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

	error = "std.http.raw.read_request request exceeds size limit";
	return false;
}

ArrayPtr BuildRequestTuple(const std::string& method, const std::string& path, const std::string& body)
{
	auto tuple = std::make_shared<Array>();
	tuple->elements.push_back(std::make_shared<const std::string>(method));
	tuple->elements.push_back(std::make_shared<const std::string>(path));
	tuple->elements.push_back(std::make_shared<const std::string>(body));
	return tuple;
}

ArrayPtr BuildEmptyRequestTuple()
{
	return std::make_shared<Array>();
}

} // namespace

void HttpRawModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(
		std::string(ModuleName()),
		MakeModule(std::string(ModuleName())));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".parse_request",
		MakeNative(
			"parse_request",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto raw = RequireString(
					ctx,
					args[0],
					"std.http.raw.parse_request expects a request string");
				if (!raw)
				{
					return std::monostate{};
				}

				std::string method;
				std::string path;
				std::string body;
				if (std::string error;
					!ParseRequestParts(*raw, method, path, body, error))
				{
					ctx.RaiseError("std.http.raw.parse_request failed: " + error);
					return std::monostate{};
				}
				return BuildRequestTuple(method, path, body);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".read_request",
		MakeNative(
			"read_request",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto connection = RequireConnection(
					ctx,
					args[0],
					"std.http.raw.read_request expects a connection handle");
				if (!connection)
				{
					return std::monostate{};
				}

				std::string request;
				std::string error;
				{
					std::lock_guard lock(connection->mutex);
					if (connection->fd < 0)
					{
						ctx.RaiseError("std.http.raw.read_request cannot use a closed connection");
						return std::monostate{};
					}
					if (!ReadRequestFromSocket(connection, request, error))
					{
						ctx.RaiseError(error);
						return std::monostate{};
					}
				}

				std::string method;
				std::string path;
				std::string body;
				if (!ParseRequestParts(request, method, path, body, error))
				{
					ctx.RaiseError("std.http.raw.read_request failed: " + error);
					return std::monostate{};
				}
				return BuildRequestTuple(method, path, body);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".try_read_request",
		MakeNative(
			"try_read_request",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto connection = RequireConnection(
					ctx,
					args[0],
					"std.http.raw.try_read_request expects a connection handle");
				if (!connection)
				{
					return std::monostate{};
				}

				std::string request;
				std::string error;
				{
					std::lock_guard lock(connection->mutex);
					if (connection->fd < 0)
					{
						return BuildEmptyRequestTuple();
					}
					if (!ReadRequestFromSocket(connection, request, error))
					{
						return BuildEmptyRequestTuple();
					}
				}

				std::string method;
				std::string path;
				std::string body;
				if (!ParseRequestParts(request, method, path, body, error))
				{
					return BuildEmptyRequestTuple();
				}
				return BuildRequestTuple(method, path, body);
			}));
}

} // namespace VM::Runtime
