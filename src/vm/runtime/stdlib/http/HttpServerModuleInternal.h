#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../../SharedRuntime.h"

namespace VM::Runtime::HttpServerInternal
{

using Core::Value;
using Execution::ExecutionContext;

struct ServerOptions
{
	std::optional<int64_t> maxRequests;
	Core::ContextPtr shutdownContext;
	int64_t readTimeoutMs = 1000;
	int64_t writeTimeoutMs = 1000;
};

Value ServeWithOptions(ExecutionContext& ctx, const std::vector<Value>& args, const ServerOptions& options);
std::optional<std::string> RequestField(
	ExecutionContext& ctx,
	const Value& requestValue,
	size_t index,
	const std::string& fieldName);
std::string RequestHeader(
	ExecutionContext& ctx,
	const Value& requestValue,
	const std::string& name);
bool ReadParsedRequest(
	ExecutionContext& ctx,
	const Core::ConnectionPtr& connection,
	std::string& method,
	std::string& path,
	std::string& body,
	std::unordered_map<std::string, std::string>& headers);
Core::ListenerPtr OpenListener(ExecutionContext& ctx, const std::string& address, int64_t port);
Core::ConnectionPtr NextConnection(
	ExecutionContext& ctx,
	const Core::ListenerPtr& listener,
	const Core::ContextPtr& shutdownContext);
void PrepareConnection(
	const Core::ConnectionPtr& connection,
	int64_t readTimeoutMs,
	int64_t writeTimeoutMs);
bool CloseConnectionHandle(const Core::ConnectionPtr& connection);
bool CloseListenerHandle(const Core::ListenerPtr& listener);
Core::HttpRequestPtr MakeRequestHandle(
	const std::string& method,
	const std::string& path,
	const std::string& body,
	const std::unordered_map<std::string, std::string>& headers);

} // namespace VM::Runtime::HttpServerInternal
