#include "ExecutionContext.h"
#include "../core/values/ValueHelper.h"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace VM::Execution
{

ExecutionContext::ExecutionContext()
	: ExecutionContext(std::make_shared<Runtime::SharedRuntime>())
{
}

ExecutionContext::ExecutionContext(std::shared_ptr<Runtime::SharedRuntime> runtime, const bool useMainThread)
	: m_runtime(std::move(runtime))
{
	m_valueStack.reserve(STACK_MAX);
	m_frames.reserve(FRAMES_MAX);
	m_currentScope = std::make_shared<Scope>();
	if (!m_runtime)
	{
		m_runtime = std::make_shared<Runtime::SharedRuntime>();
	}

	if (useMainThread)
	{
		m_currentThreadId = m_runtime->MainThreadId();
	}
	else
	{
		m_currentThreadId = m_runtime->CreateExecutionThread();
		m_ownsThreadHandle = true;
	}
}

ExecutionContext::~ExecutionContext()
{
	if (m_ownsThreadHandle && m_runtime)
	{
		m_runtime->FinishThread(m_currentThreadId);
	}
}

void ExecutionContext::PushValue(const Core::Value& value)
{
	if (m_valueStack.size() >= STACK_MAX)
	{
		throw std::overflow_error("Value stack overflow");
	}
	m_valueStack.push_back(value);
}

Core::Value ExecutionContext::PopValue()
{
	if (const size_t floor = m_frames.empty() ? 0 : m_frames.back().stackBase;
		m_valueStack.size() <= floor)
	{
		throw std::underflow_error("Value stack underflow");
	}
	const Core::Value val = m_valueStack.back();
	m_valueStack.pop_back();
	return val;
}

Core::Value& ExecutionContext::PeekValue(const size_t depth)
{
	if (depth >= m_valueStack.size())
	{
		throw std::out_of_range("Peek depth exceeds stack size");
	}
	return m_valueStack[m_valueStack.size() - 1 - depth];
}

const Core::Value& ExecutionContext::PeekValue(const size_t depth) const
{
	if (depth >= m_valueStack.size())
	{
		throw std::out_of_range("Peek depth exceeds stack size");
	}
	return m_valueStack[m_valueStack.size() - 1 - depth];
}

size_t ExecutionContext::StackSize() const
{
	return m_valueStack.size();
}

bool ExecutionContext::StackEmpty() const
{
	return m_valueStack.empty();
}

void ExecutionContext::ClearStack()
{
	m_valueStack.clear();
	m_frames.clear();
	m_activeTransactions.clear();
	m_handlerStack.clear();
}

void ExecutionContext::DefineGlobal(const std::string& name, Core::Value val)
{
	m_runtime->DefineGlobal(name, std::move(val));
}

bool ExecutionContext::GetGlobal(const std::string& name, Core::Value& outValue) const
{
	return m_runtime->GetGlobal(name, outValue);
}

bool ExecutionContext::SetGlobal(const std::string& name, Core::Value val) const
{
	return m_runtime->SetGlobal(name, std::move(val));
}

void ExecutionContext::SetAt(const size_t index, const Core::Value& val)
{
	if (index >= m_valueStack.size())
	{
		throw std::out_of_range("Local variable index out of bounds");
	}
	m_valueStack[index] = val;
}

const Core::Value& ExecutionContext::GetAt(const size_t index) const
{
	if (index >= m_valueStack.size())
	{
		throw std::out_of_range("Local variable index out of bounds");
	}
	return m_valueStack[index];
}

const Core::Value& ExecutionContext::Peek(const size_t distance) const
{
	if (distance >= m_valueStack.size())
	{
		throw std::out_of_range("Peek out of bounds");
	}
	return m_valueStack[m_valueStack.size() - 1 - distance];
}

Scope* ExecutionContext::EnterScope(const bool isFunction)
{
	size_t depth = 0;
	for (const auto* scope = m_currentScope.get(); scope != nullptr; scope = scope->parent.get())
	{
		++depth;
	}

	if (depth >= SCOPE_DEPTH_MAX)
	{
		throw std::overflow_error("Scope depth limit exceeded");
	}

	const auto newScope = std::make_shared<Scope>();
	newScope->parent = m_currentScope;
	newScope->isFunctionScope = isFunction;

	m_currentScope = newScope;
	return m_currentScope.get();
}

Scope* ExecutionContext::ExitScope()
{
	if (!m_currentScope)
	{
		return nullptr;
	}

	Scope* previous = m_currentScope->parent.get();
	m_currentScope = m_currentScope->parent;
	return previous;
}

void Scope::SetVariable(std::string name, Core::Value value)
{
	variables[std::move(name)] = std::move(value);
}

Core::Value* Scope::GetVariable(const std::string& name)
{
	const auto it = variables.find(name);
	if (it != variables.end())
	{
		return &it->second;
	}
	if (parent)
	{
		return parent->GetVariable(name);
	}
	return nullptr;
}

bool Scope::HasVariable(const std::string& name) const
{
	if (variables.contains(name))
	{
		return true;
	}
	if (parent)
	{
		return parent->HasVariable(name);
	}
	return false;
}

void ExecutionContext::PushFrame(Core::FunctionPtr func, Core::ClosurePtr closure, const size_t base)
{
	if (m_frames.size() >= FRAMES_MAX)
	{
		throw std::runtime_error("Stack overflow (too many frames)");
	}
	m_frames.emplace_back(
		std::move(func),
		std::move(closure),
		base,
		m_activeTransactions.size(),
		m_handlerStack.size());
}

void ExecutionContext::PopFrame()
{
	if (m_frames.empty())
	{
		throw std::runtime_error("Stack underflow (no frames to pop)");
	}
	m_frames.pop_back();
}

CallFrame& ExecutionContext::CurrentFrame()
{
	if (m_frames.empty())
	{
		throw std::runtime_error("No active call frame");
	}
	return m_frames.back();
}

const CallFrame& ExecutionContext::CurrentFrame() const
{
	if (m_frames.empty())
	{
		throw std::runtime_error("No active call frame");
	}
	return m_frames.back();
}

void ExecutionContext::SetLocal(const size_t index, const Core::Value& val)
{
	const size_t absoluteIndex = m_frames.back().stackBase + index;
	if (absoluteIndex >= m_valueStack.size())
	{
		throw std::out_of_range("Local index out of bounds");
	}
	m_valueStack[absoluteIndex] = val;
}

const Core::Value& ExecutionContext::GetLocal(const size_t index) const
{
	const size_t absoluteIndex = m_frames.back().stackBase + index;
	if (absoluteIndex >= m_valueStack.size())
	{
		throw std::out_of_range("Local index out of bounds");
	}
	return m_valueStack[absoluteIndex];
}

void ExecutionContext::SetUpvalue(const size_t index, const Core::Value& val)
{
	auto& frame = CurrentFrame();
	if (!frame.closure || index >= frame.closure->captures.size())
	{
		throw std::out_of_range("Upvalue index out of bounds");
	}
	frame.closure->captures[index] = val;
}

const Core::Value& ExecutionContext::GetUpvalue(const size_t index) const
{
	const auto& frame = CurrentFrame();
	if (!frame.closure || index >= frame.closure->captures.size())
	{
		throw std::out_of_range("Upvalue index out of bounds");
	}
	return frame.closure->captures[index];
}

void* ExecutionContext::Allocate(const size_t size)
{
	return m_runtime->Allocate(size);
}

bool ExecutionContext::Release(const void* ptr) const
{
	return m_runtime->Release(ptr);
}

const ExecutionContext::AllocationStats& ExecutionContext::GetAllocationStats() const
{
	thread_local AllocationStats stats;
	stats = m_runtime->GetAllocationStats();
	return stats;
}

const ExecutionContext::SyncStats& ExecutionContext::GetSyncStats() const
{
	thread_local SyncStats stats;
	stats = m_runtime->GetSyncStats();
	return stats;
}

Core::ThreadPtr ExecutionContext::CurrentThreadHandle() const
{
	return m_runtime->ThreadHandle(m_currentThreadId);
}

Core::ThreadPtr ExecutionContext::CreateLogicalThread() const
{
	return m_runtime->CreateLogicalThread();
}

Core::MutexPtr ExecutionContext::CreateMutex() const
{
	return m_runtime->CreateMutex();
}

bool ExecutionContext::TryLockMutex(const size_t threadId, const size_t mutexId)
{
	if (!m_runtime->IsThreadActive(threadId))
	{
		RaiseError("Unknown thread handle");
		return false;
	}
	if (!m_runtime->HasMutex(mutexId))
	{
		RaiseError("Unknown mutex handle");
		return false;
	}

	if (const auto owner = m_runtime->MutexOwner(mutexId); owner.has_value())
	{
		if (*owner == threadId)
		{
			RaiseError("Recursive mutex lock is not allowed (self-deadlock)");
			return false;
		}
		if (m_runtime->WouldDeadlockOnMutex(threadId, mutexId))
		{
			RaiseError("Deadlock detected while locking mutex");
			return false;
		}
	}

	if (!m_runtime->TryLockMutex(threadId, mutexId))
	{
		if (!m_runtime->MutexOwner(mutexId).has_value())
		{
			RaiseError("Mutex lock failed");
		}
		return false;
	}
	return true;
}

bool ExecutionContext::WouldDeadlockOnMutex(const size_t threadId, const size_t mutexId) const
{
	return m_runtime->WouldDeadlockOnMutex(threadId, mutexId);
}

bool ExecutionContext::UnlockMutex(const size_t threadId, const size_t mutexId)
{
	if (!m_runtime->IsThreadActive(threadId))
	{
		RaiseError("Unknown thread handle");
		return false;
	}
	if (!m_runtime->HasMutex(mutexId))
	{
		RaiseError("Unknown mutex handle");
		return false;
	}
	if (m_runtime->MutexOwner(mutexId) != threadId)
	{
		RaiseError("Mutex unlock by non-owner is not allowed");
		return false;
	}
	if (!m_runtime->UnlockMutex(threadId, mutexId))
	{
		RaiseError("Mutex unlock failed");
		return false;
	}
	return true;
}

bool ExecutionContext::AssertNoDeadlock()
{
	if (!m_runtime->AssertNoDeadlock())
	{
		RaiseError("Deadlock detected");
		return false;
	}
	return true;
}

bool ExecutionContext::JoinThread(const size_t waitingThreadId, const size_t targetThreadId)
{
	if (!m_runtime->JoinThread(waitingThreadId, targetThreadId))
	{
		RaiseError("Thread join failed");
		return false;
	}
	return true;
}

bool ExecutionContext::FinishThread(const size_t threadId)
{
	if (!m_runtime->FinishThread(threadId))
	{
		RaiseError("Unknown thread handle");
		return false;
	}
	return true;
}

bool ExecutionContext::IsMutexLocked(const size_t mutexId) const
{
	return m_runtime->IsMutexLocked(mutexId);
}

std::optional<size_t> ExecutionContext::MutexOwner(const size_t mutexId) const
{
	return m_runtime->MutexOwner(mutexId);
}

bool ExecutionContext::BeginTransaction(const Core::MutexPtr& mutex)
{
	if (!mutex)
	{
		RaiseError("Expected a mutex handle for transaction");
		return false;
	}
	if (!TryLockMutex(m_currentThreadId, mutex->id))
	{
		return false;
	}
	m_activeTransactions.push_back({ mutex->id });
	return true;
}

bool ExecutionContext::EndTransaction()
{
	if (m_activeTransactions.empty())
	{
		RaiseError("Transaction stack underflow");
		return false;
	}
	const auto txn = m_activeTransactions.back();
	m_activeTransactions.pop_back();
	return UnlockMutex(m_currentThreadId, txn.mutexId);
}

void ExecutionContext::UnwindTransactions(const size_t base)
{
	while (m_activeTransactions.size() > base)
	{
		const auto txn = m_activeTransactions.back();
		m_activeTransactions.pop_back();
		m_runtime->UnlockMutex(m_currentThreadId, txn.mutexId);
	}
}

void ExecutionContext::PushHandlerMap(const Core::HandlerMapPtr& handlerMap)
{
	m_handlerStack.push_back(handlerMap);
}

bool ExecutionContext::PopHandlerMap()
{
	if (m_handlerStack.empty())
	{
		RaiseError("Handler stack underflow");
		return false;
	}
	m_handlerStack.pop_back();
	return true;
}

void ExecutionContext::UnwindHandlers(const size_t base)
{
	while (m_handlerStack.size() > base)
	{
		m_handlerStack.pop_back();
	}
}

Core::Value ExecutionContext::ResolveHandledEffect(const std::string& effectName) const
{
	for (auto it = m_handlerStack.rbegin(); it != m_handlerStack.rend(); ++it)
	{
		if (*it && (*it)->handlers.contains(effectName))
		{
			return (*it)->handlers.at(effectName);
		}
	}
	return std::monostate{};
}

void ExecutionContext::UnwindAllTransactions()
{
	UnwindTransactions(0);
}

void ExecutionContext::ClearHandlers()
{
	m_handlerStack.clear();
}

void ExecutionContext::RaiseError(const std::string& message)
{
	if (m_errorMessage.empty())
	{
		m_errorMessage = message;
	}
}

void ExecutionContext::ClearError()
{
	m_errorMessage.clear();
}

void ExecutionContext::PrintStack() const
{
	std::cout << "[ ";
	for (const auto& val : m_valueStack)
	{
		Core::ValueHelper::PrintValue(val, std::cout);
		std::cout << " ";
	}
	std::cout << "]\n";
}

} // namespace VM::Execution