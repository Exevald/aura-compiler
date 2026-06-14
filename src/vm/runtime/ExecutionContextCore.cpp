#include "ExecutionContext.h"

#include "../core/values/ValueHelper.h"

#include <iostream>
#include <ranges>
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

void ExecutionContext::SetCallableInvoker(CallableInvoker invoker)
{
	m_callableInvoker = std::move(invoker);
}

std::optional<std::string> ExecutionContext::InvokeCallable(
	const Core::Value& callee,
	const std::vector<Core::Value>& args,
	Core::Value& result) const
{
	if (!m_callableInvoker)
	{
		return "No callable invoker is installed";
	}
	return m_callableInvoker(callee, args, result);
}

void ExecutionContext::DefineGlobal(const std::string& name, Core::Value val) const
{
	m_runtime->DefineGlobal(name, std::move(val));
}

bool ExecutionContext::GetGlobal(const std::string& name, Core::Value& outValue) const
{
	for (auto it = m_activeTransactions.rbegin(); it != m_activeTransactions.rend(); ++it)
	{
		if (const auto pendingIt = it->pendingGlobals.find(name); pendingIt != it->pendingGlobals.end())
		{
			outValue = pendingIt->second;
			return true;
		}
	}
	return m_runtime->GetGlobal(name, outValue);
}

bool ExecutionContext::SetGlobal(const std::string& name, Core::Value val) const
{
	if (!m_activeTransactions.empty())
	{
		auto& transaction = const_cast<ActiveTransaction&>(m_activeTransactions.back());
		if (!transaction.priorGlobals.contains(name))
		{
			if (Core::Value previousValue; m_runtime->GetGlobal(name, previousValue))
			{
				transaction.priorGlobals.emplace(name, previousValue);
			}
		}
		transaction.pendingGlobals[name] = std::move(val);
		return true;
	}
	return m_runtime->SetGlobal(name, std::move(val));
}

void ExecutionContext::DefineThreadLocalGlobal(const std::string& name, Core::Value val)
{
	m_threadLocalGlobals[name] = std::move(val);
}

bool ExecutionContext::GetThreadLocalGlobal(const std::string& name, Core::Value& outValue) const
{
	for (auto it = m_activeTransactions.rbegin(); it != m_activeTransactions.rend(); ++it)
	{
		if (const auto pendingIt = it->pendingThreadLocals.find(name); pendingIt != it->pendingThreadLocals.end())
		{
			outValue = pendingIt->second;
			return true;
		}
	}
	if (const auto it = m_threadLocalGlobals.find(name); it != m_threadLocalGlobals.end())
	{
		outValue = it->second;
		return true;
	}
	return false;
}

bool ExecutionContext::SetThreadLocalGlobal(const std::string& name, Core::Value val)
{
	if (const auto it = m_threadLocalGlobals.find(name); it != m_threadLocalGlobals.end())
	{
		if (!m_activeTransactions.empty())
		{
			auto& transaction = m_activeTransactions.back();
			if (!transaction.priorThreadLocals.contains(name))
			{
				transaction.priorThreadLocals.emplace(name, it->second);
			}
			transaction.pendingThreadLocals[name] = std::move(val);
			return true;
		}
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
	for (const auto* scope = m_currentScope.get(); scope != nullptr; scope = scope->m_parent.get())
	{
		++depth;
	}

	if (depth >= SCOPE_DEPTH_MAX)
	{
		throw std::overflow_error("Scope depth limit exceeded");
	}

	const auto newScope = std::make_shared<Scope>();
	newScope->m_parent = m_currentScope;
	newScope->m_isFunctionScope = isFunction;

	m_currentScope = newScope;
	return m_currentScope.get();
}

Scope* ExecutionContext::ExitScope()
{
	if (!m_currentScope)
	{
		return nullptr;
	}

	Scope* previous = m_currentScope->m_parent.get();
	m_currentScope = m_currentScope->m_parent;
	return previous;
}

void Scope::SetVariable(std::string name, Core::Value value)
{
	m_variables[std::move(name)] = std::move(value);
}

Core::Value* Scope::GetVariable(const std::string& name)
{
	const auto it = m_variables.find(name);
	if (it != m_variables.end())
	{
		return &it->second;
	}
	if (m_parent)
	{
		return m_parent->GetVariable(name);
	}
	return nullptr;
}

bool Scope::HasVariable(const std::string& name) const
{
	if (m_variables.contains(name))
	{
		return true;
	}
	if (m_parent)
	{
		return m_parent->HasVariable(name);
	}
	return false;
}

void* ExecutionContext::Allocate(const size_t size) const
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
