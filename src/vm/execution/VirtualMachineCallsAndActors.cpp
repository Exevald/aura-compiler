#include "VirtualMachine.h"
#include "common/VirtualMachineRuntimeSupport.h"

#include <algorithm>
#include <mutex>
#include <thread>
#include <utility>

namespace VM::Execution
{

using Core::Value;
using Detail::Fail;
using Detail::ReadErrorMessage;
using Detail::ReadStringConstant;
using Detail::RuntimeError;

int VirtualMachine::HandleCall(const uint16_t argCount)
{
	const size_t calleeIdx = m_context.StackSize() - argCount - 1;
	const Value callee = m_context.GetAt(calleeIdx);
	return HandleResolvedCall(callee, argCount, calleeIdx);
}

int VirtualMachine::HandleResolvedCall(
	const Value& callee,
	const uint16_t argCount,
	const size_t calleeIdx)
{
	Core::ClosurePtr closure;
	Core::FunctionPtr func;

	if (std::holds_alternative<Core::ClosurePtr>(callee))
	{
		closure = std::get<Core::ClosurePtr>(callee);
		func = closure ? closure->function : nullptr;
	}
	else if (std::holds_alternative<Core::FunctionPtr>(callee))
	{
		func = std::get<Core::FunctionPtr>(callee);
		closure = std::make_shared<Core::Closure>();
		closure->function = func;
	}
	else if (std::holds_alternative<Core::NativeFunctionPtr>(callee))
	{
		const auto& native = std::get<Core::NativeFunctionPtr>(callee);
		if (!native)
		{
			return Fail(m_context, "Can only call functions");
		}

		if ((!native->variadic && argCount != native->arity)
			|| (native->variadic && argCount < native->arity))
		{
			const std::string expected = native->variadic
				? ("at least " + std::to_string(native->arity))
				: std::to_string(native->arity);
			return Fail(m_context, "Expected " + expected + " args");
		}

		std::vector<Value> args;
		args.reserve(argCount);
		for (size_t i = 0; i < argCount; ++i)
		{
			args.push_back(m_context.GetAt(calleeIdx + 1 + i));
		}

		const Value result = native->invoke(m_context, args);
		if (m_context.HasError())
		{
			return RuntimeError;
		}

		while (m_context.StackSize() > calleeIdx)
		{
			m_context.PopValue();
		}

		m_context.PushValue(result);
		return 0;
	}
	else
	{
		return Fail(m_context, "Can only call functions");
	}

	if (!func)
	{
		return Fail(m_context, "Can only call functions");
	}

	if (argCount != func->arity)
	{
		return Fail(m_context, "Expected " + std::to_string(func->arity) + " args");
	}

	const size_t newBase = m_context.StackSize() - argCount;
	m_context.PushFrame(func, closure, newBase);
	return 0;
}

int VirtualMachine::HandleReturn()
{
	const CallFrame& frame = m_context.CurrentFrame();
	Value result = std::monostate{};
	if (m_context.StackSize() > frame.stackBase)
	{
		result = m_context.PopValue();
	}

	const size_t funcSlot = frame.stackBase - 1;
	m_context.UnwindTransactions(frame.transactionBase);
	m_context.UnwindHandlers(frame.handlerBase);
	m_context.PopFrame();

	while (m_context.StackSize() > funcSlot)
	{
		m_context.PopValue();
	}

	m_context.PushValue(result);
	return 0;
}

int VirtualMachine::HandleGlobal(
	const Core::OpCode opcode,
	const uint16_t operand,
	const std::vector<Core::Value>& constants)
{
	const auto name = ReadStringConstant(constants, operand, m_context);
	if (!name)
	{
		return RuntimeError;
	}

	if (opcode == Core::OpCode::OP_DEFINE_GLOBAL)
	{
		m_context.DefineGlobal(*name, m_context.PopValue());
		return 0;
	}

	if (opcode == Core::OpCode::OP_GET_GLOBAL)
	{
		Value val;
		if (!m_context.GetGlobal(*name, val))
		{
			return Fail(m_context, "Undefined variable: " + *name);
		}
		m_context.PushValue(val);
		return 0;
	}

	if (!m_context.SetGlobal(*name, m_context.PeekValue(0)))
	{
		return Fail(m_context, "Undefined variable: " + *name);
	}
	return 0;
}

int VirtualMachine::HandleClosure(const uint16_t operand, const std::vector<Core::Value>& constants)
{
	if (operand >= constants.size())
	{
		return Fail(m_context, "Constant index out of bounds");
	}

	if (!std::holds_alternative<Core::FunctionPtr>(constants[operand]))
	{
		return Fail(m_context, "OP_CLOSURE requires function constant");
	}

	const auto& function = std::get<Core::FunctionPtr>(constants[operand]);
	auto closure = std::make_shared<Core::Closure>();
	closure->function = function;
	closure->captures.resize(function->captureNames.size());

	for (size_t i = function->captureNames.size(); i > 0; --i)
	{
		closure->captures[i - 1] = m_context.PopValue();
	}

	m_context.PushValue(closure);
	return 0;
}

std::optional<std::string> VirtualMachine::InvokeCallable(
	const Value& callee,
	const std::vector<Value>& args,
	Value& result)
{
	const size_t stackBase = m_context.StackSize();
	const size_t frameCount = m_context.GetFramesCount();
	m_context.ClearError();

	m_context.PushValue(callee);
	for (const auto& arg : args)
	{
		m_context.PushValue(arg);
	}

	const int callResult = HandleResolvedCall(callee, static_cast<uint16_t>(args.size()), stackBase);
	if (callResult < 0)
	{
		const auto error = ReadErrorMessage(m_context);
		m_context.UnwindAllTransactions();
		m_context.ClearHandlers();
		m_context.ClearStack();
		return error;
	}

	if (m_context.GetFramesCount() > frameCount && Run() != ExecutionResult::Success)
	{
		const auto error = ReadErrorMessage(m_context);
		m_context.UnwindAllTransactions();
		m_context.ClearHandlers();
		m_context.ClearStack();
		return error;
	}

	result = m_context.StackSize() > stackBase ? m_context.PopValue() : Value(std::monostate{});
	while (m_context.StackSize() > stackBase)
	{
		m_context.PopValue();
	}
	return std::nullopt;
}

void VirtualMachine::StartActorWorker(const Core::ActorPtr& actor) const
{
	if (!actor)
	{
		return;
	}

	auto runtime = m_runtime;
	actor->worker = std::thread([actor, runtime]() {
		const VirtualMachine workerHost(std::move(runtime), false);
		workerHost.RunActorWorker(actor);
	});
}

void VirtualMachine::RunActorWorker(const Core::ActorPtr& actor) const
{
	for (;;)
	{
		Core::Actor::MailboxItem item;
		{
			std::unique_lock lock(actor->mutex);
			actor->cv.wait(lock, [&actor]() {
				return actor->stopping || !actor->mailbox.empty();
			});

			if (actor->stopping && actor->mailbox.empty())
			{
				return;
			}

			item = std::move(actor->mailbox.front());
			actor->mailbox.pop_front();
			actor->workerThreadId = std::this_thread::get_id();
		}

		Value methodValue;
		if (!actor->methods || !actor->methods->methods.contains(item.methodName))
		{
			const std::string error = "Unknown actor method: " + item.methodName;
			if (item.kind == Core::Actor::MailboxItem::Kind::Query)
			{
				std::shared_ptr<Core::Actor::QueryResult> pending;
				{
					std::lock_guard lock(actor->mutex);
					if (const auto it = actor->pendingQueries.find(item.requestId); it != actor->pendingQueries.end())
					{
						pending = it->second;
					}
				}
				if (pending)
				{
					std::lock_guard resultLock(pending->mutex);
					pending->ready = true;
					pending->error = error;
					pending->cv.notify_all();
				}
			}
			else
			{
				std::lock_guard lock(actor->mutex);
				actor->failures.push_back(error);
			}
			continue;
		}
		methodValue = actor->methods->methods.at(item.methodName);

		VirtualMachine workerVm(m_runtime, false);
		workerVm.m_activeActor = actor;

		Value result = std::monostate{};
		std::vector<Value> args;
		args.reserve(item.args.size() + 1);
		args.push_back(actor->state);
		for (const auto& arg : item.args)
		{
			args.push_back(arg);
		}

		const auto error = workerVm.InvokeCallable(methodValue, args, result);
		if (item.kind == Core::Actor::MailboxItem::Kind::Query)
		{
			std::shared_ptr<Core::Actor::QueryResult> pending;
			{
				std::lock_guard lock(actor->mutex);
				if (const auto it = actor->pendingQueries.find(item.requestId); it != actor->pendingQueries.end())
				{
					pending = it->second;
				}
			}
			if (pending)
			{
				std::lock_guard resultLock(pending->mutex);
				pending->ready = true;
				if (error)
				{
					pending->error = *error;
				}
				else
				{
					pending->value = result;
				}
				pending->cv.notify_all();
			}
		}
		else if (error)
		{
			std::lock_guard lock(actor->mutex);
			actor->failures.push_back(*error);
		}
	}
}

int VirtualMachine::EnqueueActorSend(const Core::ActorPtr& actor, std::string methodName, std::vector<Value> args)
{
	{
		std::lock_guard lock(actor->mutex);
		Core::Actor::MailboxItem item;
		item.kind = Core::Actor::MailboxItem::Kind::Send;
		item.methodName = std::move(methodName);
		item.args = std::move(args);
		actor->mailbox.push_back(std::move(item));
	}
	actor->cv.notify_one();
	m_context.PushValue(std::monostate{});
	return 0;
}

int VirtualMachine::EnqueueActorQuery(const Core::ActorPtr& actor, std::string methodName, std::vector<Value> args)
{
	if (m_activeActor && actor == m_activeActor)
	{
		return Fail(m_context, "Actor query on self would deadlock");
	}

	auto pending = std::make_shared<Core::Actor::QueryResult>();
	uint64_t requestId = 0;
	{
		std::lock_guard lock(actor->mutex);
		requestId = actor->nextRequestId++;
		actor->pendingQueries.emplace(requestId, pending);
		Core::Actor::MailboxItem item;
		item.kind = Core::Actor::MailboxItem::Kind::Query;
		item.methodName = std::move(methodName);
		item.args = std::move(args);
		item.requestId = requestId;
		actor->mailbox.push_back(std::move(item));
	}
	actor->cv.notify_one();

	std::unique_lock waitLock(pending->mutex);
	pending->cv.wait(waitLock, [&pending]() {
		return pending->ready;
	});
	waitLock.unlock();

	{
		std::lock_guard lock(actor->mutex);
		actor->pendingQueries.erase(requestId);
	}

	if (!pending->error.empty())
	{
		return Fail(m_context, pending->error);
	}

	m_context.PushValue(pending->value);
	return 0;
}

} // namespace VM::Execution