#include "TaskModule.h"
#include "../../../execution/VirtualMachine.h"
#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

namespace VM::Runtime
{

using Core::TaskPtr;
using Core::Value;
using Core::ArrayPtr;
using Execution::ExecutionContext;
using Execution::VirtualMachine;

namespace
{

TaskPtr RequireTask(ExecutionContext& ctx, const Value& value)
{
	if (!std::holds_alternative<TaskPtr>(value) || !std::get<TaskPtr>(value))
	{
		ctx.RaiseError("Expected a task handle");
		return {};
	}
	return std::get<TaskPtr>(value);
}

Core::ArrayPtr MakeJoinResult(const bool ok, Value value, const std::string& error)
{
	auto result = std::make_shared<Core::Array>();
	result->elements.push_back(ok);
	result->elements.push_back(std::move(value));
	result->elements.push_back(std::make_shared<const std::string>(error));
	return result;
}

} // namespace

void TaskModule::Install(SharedRuntime& runtime)
{
	const std::string moduleName = ModuleName();
	auto module = MakeModule(ModuleName());
	runtime.DefineGlobal(module->name, module);

	auto goNative = MakeNative(
		"go_call",
		1,
		[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
			const Value callee = args[0];
			std::vector<Value> taskArgs;
			if (args.size() == 2 && std::holds_alternative<ArrayPtr>(args[1]) && std::get<ArrayPtr>(args[1]))
			{
				for (const auto& value : std::get<ArrayPtr>(args[1])->elements)
				{
					taskArgs.push_back(value);
				}
			}
			else
			{
				taskArgs.reserve(args.size() > 1 ? args.size() - 1 : 0);
				for (size_t i = 1; i < args.size(); ++i)
				{
					taskArgs.push_back(args[i]);
				}
			}

			auto runtime = ctx.GetSharedRuntime();
			return runtime->SpawnTask(
				[runtime, callee, taskArgs = std::move(taskArgs)](std::stop_token) mutable {
					VirtualMachine workerVm(runtime, false);
					Value result = std::monostate{};
					if (const auto error = workerVm.RunCallable(callee, taskArgs, result))
					{
						return SharedRuntime::TaskOutcome{ false, std::monostate{}, *error };
					}
					return SharedRuntime::TaskOutcome{ true, std::move(result), {} };
				});
		},
		true);

	auto joinNative = MakeNative(
		"join",
		1,
		[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
			const auto task = RequireTask(ctx, args[0]);
			if (ctx.HasError())
			{
				return std::monostate{};
			}

			Value result = std::monostate{};
			std::string error;
			if (!ctx.GetRuntime().AwaitTask(task->id, result, error))
			{
				ctx.RaiseError("Unknown task handle");
				return std::monostate{};
			}
			if (!error.empty())
			{
				ctx.RaiseError(error);
				return std::monostate{};
			}
			return result;
		});

	auto joinResultNative = MakeNative(
		"join_result",
		1,
		[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
			const auto task = RequireTask(ctx, args[0]);
			if (ctx.HasError())
			{
				return std::monostate{};
			}

			Value result = std::monostate{};
			std::string error;
			if (!ctx.GetRuntime().AwaitTask(task->id, result, error))
			{
				return MakeJoinResult(false, std::monostate{}, "Unknown task handle");
			}
			return MakeJoinResult(error.empty(), std::move(result), error);
		});

	runtime.DefineGlobal(moduleName + ".go_call", goNative);
	runtime.DefineGlobal(moduleName + ".join", joinNative);
	runtime.DefineGlobal(moduleName + ".join_result", joinResultNative);
	runtime.DefineGlobal(
		moduleName + ".cancel",
		MakeNative(
			"cancel",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto task = RequireTask(ctx, args[0]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				return ctx.GetRuntime().CancelTask(task->id);
			}));
	runtime.DefineGlobal(
		moduleName + ".is_done",
		MakeNative(
			"is_done",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto task = RequireTask(ctx, args[0]);
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				return ctx.GetRuntime().IsTaskDone(task->id);
			}));

	runtime.DefineGlobal("__task_go", goNative);
	runtime.DefineGlobal("__task_await", joinNative);
}

} // namespace VM::Runtime
