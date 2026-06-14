#include "HttpServerModule.h"
#include "HttpServerModuleInternal.h"

#include "../../../core/values/ValueHelper.h"
#include "../../NativeModuleSupport.h"

#include <algorithm>
#include <cctype>

namespace VM::Runtime
{

using Core::Value;
using Execution::ExecutionContext;

namespace
{

std::string LowercaseHeaderName(std::string value)
{
	std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return value;
}

HttpServerInternal::ServerOptions MakeDefaultServerOptions(
	ExecutionContext& ctx,
	std::optional<int64_t> maxRequests)
{
	HttpServerInternal::ServerOptions options;
	options.maxRequests = maxRequests;
	options.shutdownContext = ctx.GetRuntime().BackgroundContext();
	return options;
}

Core::ContextPtr RequireContext(ExecutionContext& ctx, const Value& value)
{
	if (!std::holds_alternative<Core::ContextPtr>(value) || !std::get<Core::ContextPtr>(value))
	{
		ctx.RaiseError("std.http.server_native.listen_and_serve_with expects a shutdown context");
		return {};
	}
	return std::get<Core::ContextPtr>(value);
}

} // namespace

void HttpServerModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(
		std::string(ModuleName()),
		MakeModule(std::string(ModuleName())));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".serve_once",
		MakeNative3(
			"serve_once",
			[](ExecutionContext& ctx, const Value& address, const Value& port, const Value& routeFn) -> Value {
				return HttpServerInternal::ServeWithOptions(ctx, { address, port, routeFn }, MakeDefaultServerOptions(ctx, 1));
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".serve_n",
		MakeNative4(
			"serve_n",
			[](ExecutionContext& ctx, const Value& address, const Value& port, const Value& maxRequestsValue, const Value& routeFn) -> Value {
				const int64_t maxRequests = Core::ValueHelper::As<int64_t>(maxRequestsValue);
				if (maxRequests < 0)
				{
					ctx.RaiseError("std.http.server_native.serve_n expects a non-negative max_requests");
					return std::monostate{};
				}
				return HttpServerInternal::ServeWithOptions(
					ctx,
					{ address, port, routeFn },
					MakeDefaultServerOptions(ctx, maxRequests));
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".listen_and_serve",
		MakeNative3(
			"listen_and_serve",
			[](ExecutionContext& ctx, const Value& address, const Value& port, const Value& routeFn) -> Value {
				return HttpServerInternal::ServeWithOptions(
					ctx,
					{ address, port, routeFn },
					MakeDefaultServerOptions(ctx, std::nullopt));
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".listen_and_serve_with",
		MakeNative(
			"listen_and_serve_with",
			6,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto shutdownContext = RequireContext(ctx, args[3]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}

				const int64_t readTimeoutMs = Core::ValueHelper::As<int64_t>(args[4]);
				if (readTimeoutMs < 0)
				{
					ctx.RaiseError("std.http.server_native.listen_and_serve_with expects a non-negative read_timeout_ms");
					return std::monostate{};
				}

				const int64_t writeTimeoutMs = Core::ValueHelper::As<int64_t>(args[5]);
				if (writeTimeoutMs < 0)
				{
					ctx.RaiseError("std.http.server_native.listen_and_serve_with expects a non-negative write_timeout_ms");
					return std::monostate{};
				}

				HttpServerInternal::ServerOptions options;
				options.shutdownContext = shutdownContext;
				options.readTimeoutMs = readTimeoutMs;
				options.writeTimeoutMs = writeTimeoutMs;
				return HttpServerInternal::ServeWithOptions(
					ctx,
					{ args[0], args[1], args[2] },
					options);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".method",
		MakeNative1(
			"method",
			[](ExecutionContext& ctx, const Value& request) -> Value {
				const auto value = HttpServerInternal::RequestField(ctx, request, 0, "method");
				return value ? Value(std::make_shared<const std::string>(*value)) : Value(std::monostate{});
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".path",
		MakeNative1(
			"path",
			[](ExecutionContext& ctx, const Value& request) -> Value {
				const auto value = HttpServerInternal::RequestField(ctx, request, 1, "path");
				return value ? Value(std::make_shared<const std::string>(*value)) : Value(std::monostate{});
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".body",
		MakeNative1(
			"body",
			[](ExecutionContext& ctx, const Value& request) -> Value {
				const auto value = HttpServerInternal::RequestField(ctx, request, 2, "body");
				return value ? Value(std::make_shared<const std::string>(*value)) : Value(std::monostate{});
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".header",
		MakeNative2(
			"header",
			[](ExecutionContext& ctx, const Value& request, const Value& name) -> Value {
				const auto headerName = RequireString(
					ctx,
					name,
					"std.http.server_native.header expects a string header name");
				if (!headerName)
				{
					return std::monostate{};
				}
				return std::make_shared<const std::string>(
					HttpServerInternal::RequestHeader(ctx, request, LowercaseHeaderName(*headerName)));
			}));
}

} // namespace VM::Runtime
