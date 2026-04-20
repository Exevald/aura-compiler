#pragma once

#include "../core/values/Value.h"
#include "SharedRuntime.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace VM::Execution
{

struct CallFrame
{
	Core::FunctionPtr function;
	Core::ClosurePtr closure;
	size_t ip = 0;
	size_t stackBase = 0;
	size_t transactionBase = 0;
	size_t handlerBase = 0;

	CallFrame(
		Core::FunctionPtr func,
		Core::ClosurePtr activeClosure,
		size_t base,
		size_t transactionBaseIndex = 0,
		size_t handlerBaseIndex = 0)
		: function(std::move(func))
		, closure(std::move(activeClosure))
		, stackBase(base)
		, transactionBase(transactionBaseIndex)
		, handlerBase(handlerBaseIndex)
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
	using AllocationStats = Runtime::SharedRuntime::AllocationStats;
	using SyncStats = Runtime::SharedRuntime::SyncStats;

	ExecutionContext();
	explicit ExecutionContext(std::shared_ptr<Runtime::SharedRuntime> runtime, bool useMainThread = true);
	~ExecutionContext();

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
	bool GetGlobal(const std::string& name, Core::Value& outValue) const;
	bool SetGlobal(const std::string& name, Core::Value val) const;
	[[nodiscard]] Runtime::SharedRuntime& GetRuntime() { return *m_runtime; }
	[[nodiscard]] const Runtime::SharedRuntime& GetRuntime() const { return *m_runtime; }
	[[nodiscard]] std::shared_ptr<Runtime::SharedRuntime> GetSharedRuntime() const { return m_runtime; }
	[[nodiscard]] size_t CurrentThreadId() const { return m_currentThreadId; }

	void PushFrame(Core::FunctionPtr func, Core::ClosurePtr closure, size_t base);
	void PopFrame();
	CallFrame& CurrentFrame();
	const CallFrame& CurrentFrame() const;
	bool HasFrames() const { return !m_frames.empty(); }
	size_t GetFramesCount() const { return m_frames.size(); }

	void SetLocal(size_t index, const Core::Value& val);
	const Core::Value& GetLocal(size_t index) const;
	void SetUpvalue(size_t index, const Core::Value& val);
	const Core::Value& GetUpvalue(size_t index) const;

	Scope* EnterScope(bool isFunction = false);
	Scope* ExitScope();
	Scope* CurrentScope() const { return m_currentScope.get(); }

	void RaiseError(const std::string& message);
	bool HasError() const { return !m_errorMessage.empty(); }
	std::string_view GetError() const { return m_errorMessage; }
	void ClearError();

	void* Allocate(size_t size);
	bool Release(const void* ptr) const;
	[[nodiscard]] const AllocationStats& GetAllocationStats() const;
	[[nodiscard]] const SyncStats& GetSyncStats() const;

	Core::ThreadPtr CurrentThreadHandle() const;
	Core::ThreadPtr CreateLogicalThread() const;
	Core::MutexPtr CreateMutex() const;
	bool TryLockMutex(size_t threadId, size_t mutexId);
	bool WouldDeadlockOnMutex(size_t threadId, size_t mutexId) const;
	bool UnlockMutex(size_t threadId, size_t mutexId);
	bool AssertNoDeadlock();
	bool JoinThread(size_t waitingThreadId, size_t targetThreadId);
	bool FinishThread(size_t threadId);
	[[nodiscard]] bool IsMutexLocked(size_t mutexId) const;
	[[nodiscard]] std::optional<size_t> MutexOwner(size_t mutexId) const;
	bool BeginTransaction(const Core::MutexPtr& mutex);
	bool EndTransaction();
	void UnwindTransactions(size_t base);
	void PushHandlerMap(const Core::HandlerMapPtr& handlerMap);
	bool PopHandlerMap();
	void UnwindHandlers(size_t base);
	[[nodiscard]] Core::Value ResolveHandledEffect(const std::string& effectName) const;
	[[nodiscard]] size_t ActiveTransactionCount() const { return m_activeTransactions.size(); }
	[[nodiscard]] size_t ActiveHandlerCount() const { return m_handlerStack.size(); }
	void UnwindAllTransactions();
	void ClearHandlers();

	void PrintStack() const;

private:
	struct ActiveTransaction
	{
		size_t mutexId = 0;
	};

	std::vector<Core::Value> m_valueStack;
	std::vector<CallFrame> m_frames;
	std::shared_ptr<Scope> m_currentScope;
	std::shared_ptr<Runtime::SharedRuntime> m_runtime;
	std::string m_errorMessage;
	std::vector<ActiveTransaction> m_activeTransactions;
	std::vector<Core::HandlerMapPtr> m_handlerStack;
	size_t m_currentThreadId = 0;
	bool m_ownsThreadHandle = false;

	static constexpr size_t STACK_MAX = 4096;
	static constexpr size_t FRAMES_MAX = 64;
	static constexpr size_t SCOPE_DEPTH_MAX = 256;
};

} // namespace VM::Execution
