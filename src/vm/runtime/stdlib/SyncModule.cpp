#include "SyncModule.h"
#include "../NativeModuleSupport.h"
#include "../SharedRuntime.h"

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

void SyncModule::Install(SharedRuntime& runtime)
{
	const auto installPrefix = [&runtime](const std::string& moduleName) {
		auto module = std::make_shared<Module>();
		module->name = moduleName;
		runtime.DefineGlobal(module->name, module);

		runtime.DefineGlobal(
			moduleName + ".current_thread",
			MakeNative(
				"current_thread",
				0,
				[](const ExecutionContext& ctx, const std::vector<Value>&) -> Value {
					return ctx.CurrentThreadHandle();
				}));

		runtime.DefineGlobal(
			moduleName + ".spawn",
			MakeNative(
				"spawn",
				0, [](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
					return ctx.CreateLogicalThread();
				}));

		runtime.DefineGlobal(
			moduleName + ".mutex",
			MakeNative(
				"mutex",
				0,
				[](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
					return ctx.CreateMutex();
				}));

		runtime.DefineGlobal(
			moduleName + ".lock",
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
					return ctx.TryLockMutex(
						thread->id,
						mutex->id);
				}));

		runtime.DefineGlobal(
			moduleName + ".unlock",
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
					return ctx.UnlockMutex(
						thread->id,
						mutex->id);
				}));

		runtime.DefineGlobal(
			moduleName + ".would_deadlock",
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
					return ctx.WouldDeadlockOnMutex(
						thread->id,
						mutex->id);
				}));

		runtime.DefineGlobal(
			moduleName + ".assert_no_deadlock",
			MakeNative(
				"assert_no_deadlock",
				0,
				[](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
					return ctx.AssertNoDeadlock();
				}));

		runtime.DefineGlobal(
			moduleName + ".join",
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
					return ctx.JoinThread(
						waitingThread->id,
						targetThread->id);
				}));

		runtime.DefineGlobal(
			moduleName + ".finish",
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

		runtime.DefineGlobal(
			moduleName + ".is_locked",
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

		runtime.DefineGlobal(
			moduleName + ".owner_id",
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

		runtime.DefineGlobal(
			moduleName + ".thread_id",
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

		runtime.DefineGlobal(
			moduleName + ".thread_count",
			MakeNative(
				"thread_count",
				0,
				[](const ExecutionContext& ctx, const std::vector<Value>&) -> Value {
					return static_cast<int64_t>(ctx.GetSyncStats().threadCount);
				}));

		runtime.DefineGlobal(
			moduleName + ".mutex_count",
			MakeNative(
				"mutex_count",
				0,
				[](const ExecutionContext& ctx, const std::vector<Value>&) -> Value {
					return static_cast<int64_t>(ctx.GetSyncStats().mutexCount);
				}));

		runtime.DefineGlobal(
			moduleName + ".wait_edge_count",
			MakeNative(
				"wait_edge_count",
				0,
				[](const ExecutionContext& ctx, const std::vector<Value>&) -> Value {
					return static_cast<int64_t>(ctx.GetSyncStats().waitEdgeCount);
				}));
	};

	installPrefix(ModuleName());
}

} // namespace VM::Runtime
