#include "HttpServerModule.h"
#include "HttpServerModuleInternal.h"

#include "../../../core/values/ValueHelper.h"
#include "../../../execution/VirtualMachine.h"
#include "../../NativeModuleSupport.h"

#include <atomic>
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <stop_token>
#include <sys/socket.h>
#include <thread>
#include <vector>

namespace VM::Runtime
{

using Core::ConnectionPtr;
using Core::Value;
using Execution::ExecutionContext;

namespace
{

struct ActiveRequest
{
	std::shared_ptr<std::atomic<bool>> completed = std::make_shared<std::atomic<bool>>(false);
	std::jthread worker;
};

bool ServeConnection(
	ExecutionContext& ctx,
	const ConnectionPtr& connection,
	const Value& routeFn,
	const HttpServerInternal::ServerOptions& options)
{
	HttpServerInternal::PrepareConnection(
		connection,
		options.readTimeoutMs,
		options.writeTimeoutMs);

	std::string method;
	std::string path;
	std::string body;
	std::unordered_map<std::string, std::string> headers;
	if (!HttpServerInternal::ReadParsedRequest(ctx, connection, method, path, body, headers))
	{
		return !ctx.HasError();
	}

	const Value request = HttpServerInternal::MakeRequestHandle(method, path, body, headers);
	Value response;
	if (const auto invokeError = ctx.InvokeCallable(routeFn, { request }, response))
	{
		ctx.RaiseError(*invokeError);
		return false;
	}

	const auto responseText = RequireString(
		ctx,
		response,
		"std.http.server_native route_fn must return a response string");
	if (!responseText)
	{
		return false;
	}

	{
		std::lock_guard lock(connection->mutex);
		if (connection->fd < 0)
		{
			ctx.RaiseError("std.http.server_native cannot write to a closed connection");
			return false;
		}

		size_t sent = 0;
		while (sent < responseText->size())
		{
			const ssize_t written = ::send(
				connection->fd,
				responseText->data() + sent,
				responseText->size() - sent,
				0);
			if (written <= 0)
			{
				ctx.RaiseError("std.http.server_native failed to write response: " + std::string(std::strerror(errno)));
				return false;
			}
			sent += static_cast<size_t>(written);
		}

		// Half-close after flushing the response and briefly drain the peer's
		// remaining input so BSD/macOS does not convert the close into a reset.
		(void)::shutdown(connection->fd, SHUT_WR);

		std::array<char, 512> discard{};
		while (true)
		{
			const ssize_t received = ::recv(connection->fd, discard.data(), discard.size(), 0);
			if (received == 0)
			{
				break;
			}
			if (received < 0)
			{
				if (errno == EINTR)
				{
					continue;
				}
				if (errno == EAGAIN || errno == EWOULDBLOCK)
				{
					break;
				}
				break;
			}
		}
	}

	return true;
}

void JoinFinishedRequests(std::vector<ActiveRequest>& activeRequests)
{
	for (auto it = activeRequests.begin(); it != activeRequests.end();)
	{
		if (!it->completed || !it->completed->load())
		{
			++it;
			continue;
		}

		if (it->worker.joinable())
		{
			it->worker.join();
		}
		it = activeRequests.erase(it);
	}
}

std::optional<std::string> RequestFieldValue(
	ExecutionContext& ctx,
	const Value& requestValue,
	const size_t index,
	const std::string& fieldName)
{
	if (std::holds_alternative<Core::HttpRequestPtr>(requestValue))
	{
		const auto request = std::get<Core::HttpRequestPtr>(requestValue);
		if (!request)
		{
			ctx.RaiseError("std.http.server_native." + fieldName + " expects a request handle");
			return std::nullopt;
		}
		switch (index)
		{
		case 0:
			return request->method;
		case 1:
			return request->path;
		case 2:
			return request->body;
		default:
			ctx.RaiseError("std.http.server_native." + fieldName + " field index out of bounds");
			return std::nullopt;
		}
	}

	const auto request = RequireArray(
		ctx,
		requestValue,
		"std.http.server_native." + fieldName + " expects a request tuple");
	if (!request)
	{
		return std::nullopt;
	}
	if (request->elements.size() <= index)
	{
		ctx.RaiseError("std.http.server_native." + fieldName + " expects a [method, path, body] request tuple");
		return std::nullopt;
	}
	const auto stringValue = RequireString(
		ctx,
		request->elements[index],
		"std.http.server_native." + fieldName + " expects a string field");
	if (!stringValue)
	{
		return std::nullopt;
	}
	return *stringValue;
}

Value ServeWithRequestLimit(
	ExecutionContext& ctx,
	const std::vector<Value>& args,
	const HttpServerInternal::ServerOptions& options)
{
	const auto address = RequireString(
		ctx,
		args[0],
		"std.http.server_native expects a string address");
	if (!address)
	{
		return std::monostate{};
	}

	const int64_t port = Core::ValueHelper::As<int64_t>(args[1]);
	const Value routeFn = args[2];
	const auto listener = HttpServerInternal::OpenListener(ctx, *address, port);
	if (!listener)
	{
		return std::monostate{};
	}

	int64_t served = 0;
	std::vector<ActiveRequest> activeRequests;
	while (!options.maxRequests || served < *options.maxRequests)
	{
		JoinFinishedRequests(activeRequests);

		if (options.shutdownContext
			&& ctx.GetRuntime().IsContextCancelled(options.shutdownContext->id))
		{
			break;
		}

		const auto connection = HttpServerInternal::NextConnection(
			ctx,
			listener,
			options.shutdownContext);
		if (!connection)
		{
			if (options.shutdownContext
				&& ctx.GetRuntime().IsContextCancelled(options.shutdownContext->id)
				&& !ctx.HasError())
			{
				break;
			}
			if (!ctx.HasError())
			{
				continue;
			}
			HttpServerInternal::CloseListenerHandle(listener);
			return std::monostate{};
		}

		auto request = ActiveRequest{};
		const auto completed = request.completed;
		auto runtime = ctx.GetSharedRuntime();
		request.worker = std::jthread(
			[connection, routeFn, options, runtime, completed](std::stop_token) {
				Execution::VirtualMachine workerVm(std::move(runtime), false);

					const bool keepRunning = ServeConnection(workerVm.GetContext(), connection, routeFn, options);
					if (!keepRunning)
					{
						if (workerVm.GetContext().HasError())
						{
							std::cerr << "HTTP request handler failed: "
								<< workerVm.GetContext().GetError() << '\n';
						}
						workerVm.GetContext().UnwindAllTransactions();
					workerVm.GetContext().ClearHandlers();
				}

				HttpServerInternal::CloseConnectionHandle(connection);
				completed->store(true);
			});
		activeRequests.push_back(std::move(request));
		++served;
	}

	HttpServerInternal::CloseListenerHandle(listener);
	for (auto& request : activeRequests)
	{
		if (request.worker.joinable())
		{
			request.worker.join();
		}
	}
	return std::monostate{};
}

} // namespace

Value HttpServerInternal::ServeWithOptions(
	ExecutionContext& ctx,
	const std::vector<Value>& args,
	const ServerOptions& options)
{
	return ServeWithRequestLimit(ctx, args, options);
}

std::optional<std::string> HttpServerInternal::RequestField(
	ExecutionContext& ctx,
	const Value& requestValue,
	const size_t index,
	const std::string& fieldName)
{
	return RequestFieldValue(ctx, requestValue, index, fieldName);
}

std::string HttpServerInternal::RequestHeader(
	ExecutionContext& ctx,
	const Value& requestValue,
	const std::string& name)
{
	if (std::holds_alternative<Core::HttpRequestPtr>(requestValue))
	{
		const auto request = std::get<Core::HttpRequestPtr>(requestValue);
		if (!request)
		{
			ctx.RaiseError("std.http.server_native.header expects a request handle");
			return {};
		}
		if (const auto it = request->headers.find(name); it != request->headers.end())
		{
			return it->second;
		}
		return {};
	}

	const auto request = RequireArray(
		ctx,
		requestValue,
		"std.http.server_native.header expects a request handle");
	if (!request)
	{
		return {};
	}
	return {};
}

} // namespace VM::Runtime
