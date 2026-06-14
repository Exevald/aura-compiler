#include "ChannelModule.h"
#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

namespace VM::Runtime
{

using Core::ArrayPtr;
using Core::ChannelPtr;
using Core::Value;
using Execution::ExecutionContext;

namespace
{

ChannelPtr RequireChannel(ExecutionContext& ctx, const Value& value)
{
	if (!std::holds_alternative<ChannelPtr>(value) || !std::get<ChannelPtr>(value))
	{
		ctx.RaiseError("Expected a channel handle");
		return {};
	}
	return std::get<ChannelPtr>(value);
}

ArrayPtr RecvResult(const bool ok, Value value)
{
	auto result = std::make_shared<Core::Array>();
	result->elements.push_back(ok);
	result->elements.push_back(std::move(value));
	return result;
}

} // namespace

void ChannelModule::Install(SharedRuntime& runtime)
{
	const std::string moduleName = ModuleName();
	auto module = MakeModule(moduleName);
	runtime.DefineGlobal(module->name, module);

	runtime.DefineGlobal(
		moduleName + ".make",
		MakeNative(
			"make",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto capacity = static_cast<size_t>(Core::ValueHelper::As<int64_t>(args[0]));
				return ctx.GetRuntime().CreateChannel(capacity);
			}));
	runtime.DefineGlobal(
		moduleName + ".send",
		MakeNative(
			"send",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto channel = RequireChannel(ctx, args[0]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				return ctx.GetRuntime().SendChannel(channel->id, args[1]);
			}));
	runtime.DefineGlobal(
		moduleName + ".recv",
		MakeNative(
			"recv",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto channel = RequireChannel(ctx, args[0]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}

				Value value = std::monostate{};
				bool ok = false;
				if (!ctx.GetRuntime().RecvChannel(channel->id, value, ok))
				{
					ctx.RaiseError("Unknown channel handle");
					return std::monostate{};
				}
				return RecvResult(ok, std::move(value));
			}));
	runtime.DefineGlobal(
		moduleName + ".close",
		MakeNative(
			"close",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto channel = RequireChannel(ctx, args[0]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				return ctx.GetRuntime().CloseChannel(channel->id);
			}));
}

} // namespace VM::Runtime