#include "ContextModule.h"
#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

namespace VM::Runtime
{

using Core::ContextPtr;
using Core::Value;
using Execution::ExecutionContext;

namespace
{

ContextPtr RequireContext(ExecutionContext& ctx, const Value& value)
{
	if (!std::holds_alternative<ContextPtr>(value) || !std::get<ContextPtr>(value))
	{
		ctx.RaiseError("Expected a context handle");
		return {};
	}
	return std::get<ContextPtr>(value);
}

} // namespace

void ContextModule::Install(SharedRuntime& runtime)
{
	const std::string moduleName = ModuleName();
	auto module = MakeModule(moduleName);
	runtime.DefineGlobal(module->name, module);

	auto backgroundNative = MakeNative(
		"background",
		0,
		[](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
			return ctx.GetRuntime().BackgroundContext();
		});
	auto withCancelNative = MakeNative(
		"with_cancel",
		1,
		[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
			const auto parent = RequireContext(ctx, args[0]);
			if (ctx.HasError())
			{
				return std::monostate{};
			}
			auto child = ctx.GetRuntime().CreateChildContext(parent->id);
			if (!child)
			{
				ctx.RaiseError("Unknown parent context handle");
				return std::monostate{};
			}
			return child;
		});
	auto cancelNative = MakeNative(
		"cancel",
		1,
		[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
			const auto context = RequireContext(ctx, args[0]);
			if (ctx.HasError())
			{
				return std::monostate{};
			}
			return ctx.GetRuntime().CancelContext(context->id);
		});
	auto isCancelledNative = MakeNative(
		"is_cancelled",
		1,
		[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
			const auto context = RequireContext(ctx, args[0]);
			if (ctx.HasError())
			{
				return std::monostate{};
			}
			return ctx.GetRuntime().IsContextCancelled(context->id);
		});

	runtime.DefineGlobal(
		moduleName + ".background",
		backgroundNative);
	runtime.DefineGlobal(
		moduleName + ".with_cancel",
		withCancelNative);
	runtime.DefineGlobal(
		moduleName + ".cancel",
		cancelNative);
	runtime.DefineGlobal(
		moduleName + ".is_cancelled",
		isCancelledNative);
}

} // namespace VM::Runtime
