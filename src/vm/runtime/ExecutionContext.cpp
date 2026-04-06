#include "ExecutionContext.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace VM::Execution
{

ExecutionContext::ExecutionContext()
{
	m_valueStack.reserve(STACK_MAX);
	m_frames.reserve(FRAMES_MAX);
	m_currentScope = std::make_shared<Scope>();
	m_threads.emplace(m_mainThreadId, SyncThreadState{});
	RecomputeSyncStats();
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

	if (size_t floor = m_frames.empty() ? 0 : m_frames.back().stackBase;
		m_valueStack.size() <= floor)
	{
		throw std::underflow_error("Value stack underflow");
	}
	const Core::Value val = m_valueStack.back();
	m_valueStack.pop_back();
	return val;
}
Core::Value& ExecutionContext::PeekValue(size_t depth)
{
	if (depth >= m_valueStack.size())
	{
		throw std::out_of_range("Peek depth exceeds stack size");
	}
	return m_valueStack[m_valueStack.size() - 1 - depth];
}

const Core::Value& ExecutionContext::PeekValue(size_t depth) const
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
}

void ExecutionContext::DefineGlobal(const std::string& name, Core::Value val)
{
	m_globals[name] = std::move(val);
}

bool ExecutionContext::GetGlobal(const std::string& name, Core::Value& outValue)
{
	if (const auto it = m_globals.find(name); it != m_globals.end())
	{
		outValue = it->second;
		return true;
	}
	return false;
}

bool ExecutionContext::SetGlobal(const std::string& name, Core::Value val)
{
	if (const auto it = m_globals.find(name); it != m_globals.end())
	{
		it->second = std::move(val);
		return true;
	}
	return false;
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

void ExecutionContext::PushFrame(Core::FunctionPtr func, Core::ClosurePtr closure, size_t base)
{
	if (m_frames.size() >= FRAMES_MAX)
	{
		throw std::runtime_error("Stack overflow (too many frames)");
	}
	m_frames.emplace_back(std::move(func), std::move(closure), base);
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
	auto block = std::make_unique<uint8_t[]>(size);
	void* ptr = block.get();

	m_memoryPool.emplace(ptr, AllocationBlock{ size, std::move(block) });
	++m_allocationStats.activeAllocations;
	m_allocationStats.activeBytes += size;
	++m_allocationStats.totalAllocations;
	m_allocationStats.totalBytes += size;

	return ptr;
}

bool ExecutionContext::Release(const void* ptr)
{
	if (const auto it = m_memoryPool.find(ptr); it != m_memoryPool.end())
	{
		--m_allocationStats.activeAllocations;
		m_allocationStats.activeBytes -= it->second.size;
		m_memoryPool.erase(it);
		return true;
	}

	return false;
}

Core::ThreadPtr ExecutionContext::CurrentThreadHandle() const
{
	auto thread = std::make_shared<Core::ThreadHandle>();
	thread->id = m_mainThreadId;
	return thread;
}

Core::ThreadPtr ExecutionContext::CreateLogicalThread()
{
	auto thread = std::make_shared<Core::ThreadHandle>();
	thread->id = m_nextThreadId++;
	m_threads.emplace(thread->id, SyncThreadState{});
	RecomputeSyncStats();
	return thread;
}

Core::MutexPtr ExecutionContext::CreateMutex()
{
	auto mutex = std::make_shared<Core::MutexHandle>();
	mutex->id = m_nextMutexId++;
	m_mutexes.emplace(mutex->id, SyncMutexState{});
	RecomputeSyncStats();
	return mutex;
}

bool ExecutionContext::SyncHandleExists(const size_t threadId) const
{
	return m_threads.contains(threadId) && m_threads.at(threadId).active;
}

bool ExecutionContext::MutexExists(const size_t mutexId) const
{
	return m_mutexes.contains(mutexId);
}

void ExecutionContext::ClearWaitingEdgesFrom(const size_t threadId)
{
	m_waitGraph.erase(threadId);
	RecomputeSyncStats();
}

void ExecutionContext::AddWaitEdge(const size_t fromThreadId, const size_t toThreadId)
{
	m_waitGraph[fromThreadId].insert(toThreadId);
	RecomputeSyncStats();
}

bool ExecutionContext::WouldIntroduceCycle(const size_t fromThreadId, const size_t toThreadId) const
{
	if (fromThreadId == toThreadId)
	{
		return true;
	}

	std::unordered_set<size_t> visited;
	std::vector<size_t> stack{ toThreadId };

	while (!stack.empty())
	{
		const size_t current = stack.back();
		stack.pop_back();
		if (!visited.insert(current).second)
		{
			continue;
		}
		if (current == fromThreadId)
		{
			return true;
		}
		if (const auto it = m_waitGraph.find(current); it != m_waitGraph.end())
		{
			for (const size_t next : it->second)
			{
				stack.push_back(next);
			}
		}
	}

	return false;
}

bool ExecutionContext::HasCycle() const
{
	for (const auto& [fromThreadId, waitingOn] : m_waitGraph)
	{
		for (const size_t toThreadId : waitingOn)
		{
			if (WouldIntroduceCycle(fromThreadId, toThreadId))
			{
				return true;
			}
		}
	}
	return false;
}

void ExecutionContext::RecomputeSyncStats()
{
	m_syncStats.threadCount = 0;
	for (const auto& [threadId, state] : m_threads)
	{
		(void)threadId;
		if (state.active)
		{
			++m_syncStats.threadCount;
		}
	}
	m_syncStats.mutexCount = m_mutexes.size();
	m_syncStats.waitEdgeCount = 0;
	for (const auto& [fromThreadId, waitingOn] : m_waitGraph)
	{
		(void)fromThreadId;
		m_syncStats.waitEdgeCount += waitingOn.size();
	}
}

bool ExecutionContext::TryLockMutex(const size_t threadId, const size_t mutexId)
{
	if (!SyncHandleExists(threadId))
	{
		RaiseError("Unknown thread handle");
		return false;
	}
	if (!MutexExists(mutexId))
	{
		RaiseError("Unknown mutex handle");
		return false;
	}

	auto& thread = m_threads.at(threadId);
	auto& mutex = m_mutexes.at(mutexId);
	if (!mutex.ownerThreadId)
	{
		mutex.ownerThreadId = threadId;
		thread.ownedMutexes.insert(mutexId);
		ClearWaitingEdgesFrom(threadId);
		return true;
	}

	if (*mutex.ownerThreadId == threadId)
	{
		RaiseError("Potential self-deadlock: thread already owns mutex");
		return false;
	}

	if (WouldIntroduceCycle(threadId, *mutex.ownerThreadId))
	{
		RaiseError("Deadlock detected in thread wait graph");
		return false;
	}

	ClearWaitingEdgesFrom(threadId);
	AddWaitEdge(threadId, *mutex.ownerThreadId);
	return false;
}

bool ExecutionContext::WouldDeadlockOnMutex(const size_t threadId, const size_t mutexId) const
{
	if (!SyncHandleExists(threadId) || !MutexExists(mutexId))
	{
		return false;
	}

	const auto& mutex = m_mutexes.at(mutexId);
	if (!mutex.ownerThreadId)
	{
		return false;
	}
	return WouldIntroduceCycle(threadId, *mutex.ownerThreadId);
}

bool ExecutionContext::UnlockMutex(const size_t threadId, const size_t mutexId)
{
	if (!SyncHandleExists(threadId))
	{
		RaiseError("Unknown thread handle");
		return false;
	}
	if (!MutexExists(mutexId))
	{
		RaiseError("Unknown mutex handle");
		return false;
	}

	auto& mutex = m_mutexes.at(mutexId);
	if (!mutex.ownerThreadId)
	{
		RaiseError("Mutex is not locked");
		return false;
	}
	if (*mutex.ownerThreadId != threadId)
	{
		RaiseError("Mutex unlock attempted by non-owner thread");
		return false;
	}

	mutex.ownerThreadId.reset();
	m_threads.at(threadId).ownedMutexes.erase(mutexId);
	return true;
}

bool ExecutionContext::AssertNoDeadlock()
{
	if (HasCycle())
	{
		RaiseError("Deadlock detected in thread wait graph");
		return false;
	}
	return true;
}

bool ExecutionContext::JoinThread(const size_t waitingThreadId, const size_t targetThreadId)
{
	if (!SyncHandleExists(waitingThreadId) || !SyncHandleExists(targetThreadId))
	{
		RaiseError("Unknown thread handle");
		return false;
	}
	if (waitingThreadId == targetThreadId)
	{
		RaiseError("Thread cannot join itself");
		return false;
	}
	if (WouldIntroduceCycle(waitingThreadId, targetThreadId))
	{
		RaiseError("Deadlock detected in thread wait graph");
		return false;
	}

	ClearWaitingEdgesFrom(waitingThreadId);
	AddWaitEdge(waitingThreadId, targetThreadId);
	return true;
}

bool ExecutionContext::FinishThread(const size_t threadId)
{
	if (!SyncHandleExists(threadId))
	{
		RaiseError("Unknown thread handle");
		return false;
	}
	if (!m_threads.at(threadId).ownedMutexes.empty())
	{
		RaiseError("Cannot finish thread while it still owns mutexes");
		return false;
	}

	m_threads.at(threadId).active = false;
	m_waitGraph.erase(threadId);
	for (auto& [fromThreadId, waitingOn] : m_waitGraph)
	{
		(void)fromThreadId;
		waitingOn.erase(threadId);
	}
	RecomputeSyncStats();
	return true;
}

bool ExecutionContext::IsMutexLocked(const size_t mutexId) const
{
	if (!MutexExists(mutexId))
	{
		return false;
	}
	return m_mutexes.at(mutexId).ownerThreadId.has_value();
}

std::optional<size_t> ExecutionContext::MutexOwner(const size_t mutexId) const
{
	if (!MutexExists(mutexId))
	{
		return std::nullopt;
	}
	return m_mutexes.at(mutexId).ownerThreadId;
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
	std::cout << "=== Stack Trace ===\n";
	std::cout << "Value stack size: " << m_valueStack.size() << "\n";
	std::cout << "Call frames depth: " << m_frames.size() << "\n";
	if (!m_errorMessage.empty())
	{
		std::cout << "Error: " << m_errorMessage << "\n";
	}
}

} // namespace VM::Execution