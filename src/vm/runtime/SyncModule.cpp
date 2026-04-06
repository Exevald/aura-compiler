#include "SyncModule.h"
#include "../runtime/ExecutionContext.h"
#include "DiagnosticsModule.h"

#include <utility>

namespace VM::Runtime
{

using Core::Module;
using Core::MutexPtr;
using Core::ThreadPtr;
using Core::Value;
using Execution::ExecutionContext;

namespace
{

template <typename Fn>
auto MakeNative(const std::string& name, const int arity, Fn&& fn)
{
	auto native = std::make_shared<Core::NativeFunction>();
	native->name = name;
	native->arity = arity;
	native->invoke = std::forward<Fn>(fn);
	return native;
}

ThreadPtr RequireThread(ExecutionContext& ctx, const Value& value)
{
	if (!std::holds_alternative<ThreadPtr>(value) || !std::get<ThreadPtr>(value))
	{
		ctx.RaiseError("Expected a thread handle");
		return {};
	}
	return std::get<ThreadPtr>(value);
}

MutexPtr RequireMutex(ExecutionContext& ctx, const Value& value)
{
	if (!std::holds_alternative<MutexPtr>(value) || !std::get<MutexPtr>(value))
	{
		ctx.RaiseError("Expected a mutex handle");
		return {};
	}
	return std::get<MutexPtr>(value);
}

} // namespace

void SyncModule::Install(ExecutionContext& context)
{
	auto module = std::make_shared<Module>();
	module->name = ModuleName();
	context.DefineGlobal(module->name, module);

	context.DefineGlobal(
		std::string(ModuleName()) + ".current_thread",
		MakeNative(
			"current_thread",
			0,
			[](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
				return ctx.CurrentThreadHandle();
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".spawn",
		MakeNative(
			"spawn",
			0, [](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
				return ctx.CreateLogicalThread();
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".mutex",
		MakeNative(
			"mutex",
			0,
			[](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
				return ctx.CreateMutex();
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".lock",
		MakeNative(
			"lock",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto thread = RequireThread(ctx, args[0]);
				const auto mutex = RequireMutex(ctx, args[1]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				return ctx.TryLockMutex(thread->id, mutex->id);
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".unlock",
		MakeNative(
			"unlock",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto thread = RequireThread(ctx, args[0]);
				const auto mutex = RequireMutex(ctx, args[1]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				return ctx.UnlockMutex(thread->id, mutex->id);
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".would_deadlock",
		MakeNative(
			"would_deadlock",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto thread = RequireThread(ctx, args[0]);
				const auto mutex = RequireMutex(ctx, args[1]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				return ctx.WouldDeadlockOnMutex(thread->id, mutex->id);
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".assert_no_deadlock",
		MakeNative(
			"assert_no_deadlock",
			0,
			[](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
				return ctx.AssertNoDeadlock();
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".join",
		MakeNative(
			"join",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto waitingThread = RequireThread(ctx, args[0]);
				const auto targetThread = RequireThread(ctx, args[1]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				return ctx.JoinThread(waitingThread->id, targetThread->id);
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".finish",
		MakeNative(
			"finish",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto thread = RequireThread(ctx, args[0]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				return ctx.FinishThread(thread->id);
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".is_locked",
		MakeNative(
			"is_locked",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto mutex = RequireMutex(ctx, args[0]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				return ctx.IsMutexLocked(mutex->id);
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".owner_id",
		MakeNative(
			"owner_id",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto mutex = RequireMutex(ctx, args[0]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}

				const auto owner = ctx.MutexOwner(mutex->id);
				if (!owner)
				{
					return std::monostate{};
				}
				return static_cast<int64_t>(*owner);
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".thread_id",
		MakeNative(
			"thread_id",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto thread = RequireThread(ctx, args[0]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				return static_cast<int64_t>(thread->id);
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".thread_count",
		MakeNative(
			"thread_count",
			0,
			[](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
				return static_cast<int64_t>(ctx.GetSyncStats().threadCount);
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".mutex_count",
		MakeNative(
			"mutex_count",
			0,
			[](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
				return static_cast<int64_t>(ctx.GetSyncStats().mutexCount);
			}));

	context.DefineGlobal(
		std::string(ModuleName()) + ".wait_edge_count",
		MakeNative(
			"wait_edge_count",
			0,
			[](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
				return static_cast<int64_t>(ctx.GetSyncStats().waitEdgeCount);
			}));
}

} // namespace VM::Runtime
