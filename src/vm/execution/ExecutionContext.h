#pragma once

#include "../core/Value.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace VM::Execution
{

struct CallFrame
{
	Core::FunctionPtr function;
	size_t ip = 0;
	size_t stackBase = 0;

	CallFrame(Core::FunctionPtr func, size_t base)
		: function(std::move(func))
		, stackBase(base)
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

	void PushValue(const Core::Value& value);
	Core::Value PopValue();
	Core::Value& PeekValue(size_t depth = 0);
	const Core::Value& PeekValue(size_t depth = 0) const;
	size_t StackSize() const;
	bool StackEmpty() const;
	void ClearStack();

	void SetAt(size_t index, const Core::Value& val);
	const Core::Value& GetAt(size_t index) const;
	const Core::Value& Peek(size_t distance = 0) const;

	void DefineGlobal(const std::string& name, Core::Value val);
	bool GetGlobal(const std::string& name, Core::Value& outValue);
	bool SetGlobal(const std::string& name, Core::Value val);

	void PushFrame(Core::FunctionPtr func, size_t base);
	void PopFrame();
	CallFrame& CurrentFrame();
	const CallFrame& CurrentFrame() const;
	bool HasFrames() const { return !m_frames.empty(); }

	void SetLocal(size_t index, const Core::Value& val);
	const Core::Value& GetLocal(size_t index) const;

	Scope* EnterScope(bool isFunction = false);
	Scope* ExitScope();
	Scope* CurrentScope() const { return m_currentScope.get(); }

	void RaiseError(const std::string& message);
	bool HasError() const { return !m_errorMessage.empty(); }
	std::string_view GetError() const { return m_errorMessage; }
	void ClearError();

	void* Allocate(size_t size);
	static void Release(const void* ptr);

	void PrintStack() const;

private:
	std::vector<Core::Value> m_valueStack;
	std::vector<CallFrame> m_frames;
	std::shared_ptr<Scope> m_currentScope;
	std::unordered_map<std::string, Core::Value> m_globals;
	std::string m_errorMessage;
	std::vector<std::unique_ptr<uint8_t[]>> m_memoryPool;

	static constexpr size_t STACK_MAX = 4096;
	static constexpr size_t FRAMES_MAX = 64;
	static constexpr size_t SCOPE_DEPTH_MAX = 256;
};

} // namespace VM::Execution