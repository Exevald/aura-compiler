#include "ExecutionContext.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace VM::Execution
{

ExecutionContext::ExecutionContext()
{
	m_valueStack.reserve(STACK_MAX);
	m_currentScope = std::make_shared<Scope>();
}

void ExecutionContext::PushValue(const Core::Value value)
{
	if (m_valueStack.size() >= STACK_MAX)
	{
		throw std::overflow_error("Value stack overflow");
	}
	m_valueStack.push_back(value);
}

Core::Value ExecutionContext::PopValue()
{
	if (m_valueStack.empty())
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
	variables[std::move(name)] = value;
}

Core::Value* Scope::GetVariable(const std::string& name)
{
	auto it = variables.find(name);
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

void ExecutionContext::PushCallFrame(size_t ip, size_t stackBase)
{
	m_callStack.emplace(ip, stackBase);
}

CallFrame* ExecutionContext::PopCallFrame()
{
	if (m_callStack.empty())
	{
		return nullptr;
	}
	m_callStack.pop();
	return m_callStack.empty() ? nullptr : &m_callStack.top();
}

void* ExecutionContext::Allocate(const size_t size)
{
	auto block = std::make_unique<uint8_t[]>(size);
	void* ptr = block.get();
	m_memoryPool.push_back(std::move(block));
	return ptr;
}

void ExecutionContext::Release(const void* ptr)
{
	(void)ptr;
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

void ExecutionContext::Jump(const size_t targetIp)
{
	(void)targetIp;
}

void ExecutionContext::JumpIf(bool condition, const size_t targetIp)
{
	if (!condition)
	{
		Jump(targetIp);
	}
}

void ExecutionContext::PrintStack() const
{
	std::cout << "=== Stack Trace ===\n";
	std::cout << "Value stack size: " << m_valueStack.size() << "\n";
	std::cout << "Call frames: " << m_callStack.size() << "\n";

	if (!m_errorMessage.empty())
	{
		std::cout << "Error: " << m_errorMessage << "\n";
	}
}

} // namespace VM::Execution