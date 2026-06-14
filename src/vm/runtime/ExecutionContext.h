#pragma once

#include "../core/values/Value.h"
#include "SharedRuntime.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
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
		const size_t base,
		const size_t transactionBaseIndex = 0,
		const size_t handlerBaseIndex = 0)
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
	std::shared_ptr<Scope> m_parent;
	VariableMap m_variables;
	bool m_isFunctionScope{ false };

	void SetVariable(std::string name, Core::Value value);
	Core::Value* GetVariable(const std::string& name);
	[[nodiscard]] bool HasVariable(const std::string& name) const;
};

class ExecutionContext
{
public:
	using AllocationStats = Runtime::SharedRuntime::AllocationStats;
	using SyncStats = Runtime::SharedRuntime::SyncStats;
	using CallableInvoker = std::function<
		std::optional<std::string>(const Core::Value&, const std::vector<Core::Value>&, Core::Value&)>;

	ExecutionContext();
	explicit ExecutionContext(
		std::shared_ptr<Runtime::SharedRuntime> runtime,
		bool useMainThread = true);
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

	void DefineGlobal(const std::string& name, Core::Value val) const;
	bool GetGlobal(const std::string& name, Core::Value& outValue) const;
	bool SetGlobal(const std::string& name, Core::Value val) const;
	void DefineThreadLocalGlobal(const std::string& name, Core::Value val);
	bool GetThreadLocalGlobal(const std::string& name, Core::Value& outValue) const;
	bool SetThreadLocalGlobal(const std::string& name, Core::Value val);
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

	void* Allocate(size_t size) const;
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
	bool BeginTransaction(const std::vector<Core::MutexPtr>& mutexes);
	bool EndTransaction();
	void UnwindTransactions(size_t base);
	bool GetArraySize(const Core::ArrayPtr& array, size_t& outSize) const;
	bool GetArrayElement(const Core::ArrayPtr& array, size_t index, Core::Value& outValue) const;
	bool SetArrayElement(const Core::ArrayPtr& array, size_t index, Core::Value value);
	bool ReplaceArray(const Core::ArrayPtr& array, std::vector<Core::Value> elements);
	bool PushArrayElement(const Core::ArrayPtr& array, Core::Value value);
	bool PopArrayElement(const Core::ArrayPtr& array, Core::Value& outValue);
	void RecordArrayWrite(
		const Core::ArrayPtr& array,
		size_t index,
		const Core::Value& previousValue);
	bool GetInstanceField(const Core::InstancePtr& instance, size_t index, Core::Value& outValue) const;
	bool SetInstanceField(const Core::InstancePtr& instance, size_t index, Core::Value value);
	void RecordInstanceFieldWrite(
		const Core::InstancePtr& instance,
		size_t index,
		const Core::Value& previousValue);
	void PushHandlerMap(const Core::HandlerMapPtr& handlerMap);
	bool PopHandlerMap();
	void UnwindHandlers(size_t base);
	[[nodiscard]] Core::Value ResolveHandledEffect(const std::string& effectName) const;
	[[nodiscard]] size_t ActiveTransactionCount() const { return m_activeTransactions.size(); }
	[[nodiscard]] size_t ActiveHandlerCount() const { return m_handlerStack.size(); }
	void UnwindAllTransactions();
	void ClearHandlers();
	void SetCallableInvoker(CallableInvoker invoker);
	[[nodiscard]] std::optional<std::string> InvokeCallable(
		const Core::Value& callee,
		const std::vector<Core::Value>& args,
		Core::Value& result) const;

	void PrintStack() const;

private:
	struct ActiveTransaction
	{
		std::vector<size_t> mutexIds;
		std::vector<size_t> acquiredMutexIds;
		struct RollbackEntry
		{
			std::function<void()> restore;
		};
		std::vector<RollbackEntry> rollbacks;
		std::unordered_map<std::string, Core::Value> priorGlobals;
		std::unordered_map<std::string, Core::Value> pendingGlobals;
		std::unordered_map<std::string, Core::Value> priorThreadLocals;
		std::unordered_map<std::string, Core::Value> pendingThreadLocals;
		std::unordered_map<const void*, std::vector<Core::Value>> priorArrayValues;
		std::unordered_map<const void*, std::vector<Core::Value>> pendingArrayValues;
		std::unordered_map<const void*, std::unordered_map<size_t, Core::Value>> priorInstanceValues;
		std::unordered_map<const void*, std::unordered_map<size_t, Core::Value>> pendingInstanceValues;
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
	std::unordered_map<std::string, Core::Value> m_threadLocalGlobals;
	CallableInvoker m_callableInvoker;

	static constexpr size_t STACK_MAX = 4096;
	static constexpr size_t FRAMES_MAX = 64;
	static constexpr size_t SCOPE_DEPTH_MAX = 256;
};

} // namespace VM::Execution
