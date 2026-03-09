#pragma once

#include "../core/Value.h"

#include <map>
#include <memory>
#include <stack>
#include <string>
#include <vector>

namespace VM::Execution
{

struct CallFrame
{
	size_t ip;
	size_t stackBase;
	Core::Value returnValue;
	bool hasReturnValue{ false };

	CallFrame(const size_t ip_, const size_t base_)
		: ip(ip_)
		, stackBase(base_)
	{
	}
};

class Scope
{
public:
	using VariableMap = std::map<std::string, Core::Value>;

	std::shared_ptr<Scope> parent;
	VariableMap variables;
	bool isFunctionScope{ false };

	void SetVariable(std::string name, Core::Value value);
	Core::Value* GetVariable(const std::string& name);
	[[nodiscard]] bool HasVariable(const std::string& name) const;
};

class ExecutionContext
{
public:
	ExecutionContext();

	void PushValue(Core::Value value);
	Core::Value PopValue();
	Core::Value& PeekValue(size_t depth = 0);
	[[nodiscard]] const Core::Value& PeekValue(size_t depth = 0) const;
	[[nodiscard]] size_t StackSize() const;
	[[nodiscard]] bool StackEmpty() const;
	void ClearStack();

	Scope* EnterScope(bool isFunction = false);
	Scope* ExitScope();
	[[nodiscard]] Scope* CurrentScope() const { return m_currentScope.get(); }

	void PushCallFrame(size_t ip, size_t stackBase);
	CallFrame* PopCallFrame();
	CallFrame* CurrentFrame() { return m_callStack.empty() ? nullptr : &m_callStack.top(); }

	void* Allocate(size_t size);
	void Release(const void* ptr);

	void RaiseError(const std::string& message);
	[[nodiscard]] bool HasError() const { return !m_errorMessage.empty(); }
	[[nodiscard]] std::string_view GetError() const { return m_errorMessage; }
	void ClearError();

	void Jump(size_t targetIp);
	void JumpIf(bool condition, size_t targetIp);

	void PrintStack() const;

private:
	std::vector<Core::Value> m_valueStack;
	std::stack<CallFrame> m_callStack;
	std::shared_ptr<Scope> m_currentScope;
	std::vector<std::unique_ptr<Scope>> m_scopePool;

	std::vector<std::unique_ptr<uint8_t[]>> m_memoryPool;
	size_t m_memoryOffset{ 0 };
	std::string m_errorMessage;

	static constexpr size_t STACK_MAX = 4096;
	static constexpr size_t SCOPE_DEPTH_MAX = 256;
};

} // namespace VM::Execution