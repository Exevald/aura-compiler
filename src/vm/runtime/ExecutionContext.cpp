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

void ExecutionContext::PushFrame(Core::FunctionPtr func, size_t base)
{
	if (m_frames.size() >= FRAMES_MAX)
	{
		throw std::runtime_error("Stack overflow (too many frames)");
	}
	m_frames.emplace_back(std::move(func), base);
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