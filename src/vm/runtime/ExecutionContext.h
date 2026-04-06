#pragma once

#include "../core/values/Value.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace VM::Execution
{

struct CallFrame
{
	Core::FunctionPtr function;
	Core::ClosurePtr closure;
	size_t ip = 0;
	size_t stackBase = 0;

	CallFrame(Core::FunctionPtr func, Core::ClosurePtr activeClosure, size_t base)
		: function(std::move(func))
		, closure(std::move(activeClosure))
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
	struct AllocationStats
	{
		size_t activeAllocations = 0;
		size_t activeBytes = 0;
		size_t totalAllocations = 0;
		size_t totalBytes = 0;
	};

	struct SyncStats
	{
		size_t threadCount = 0;
		size_t mutexCount = 0;
		size_t waitEdgeCount = 0;
	};

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
	bool Release(const void* ptr);
	[[nodiscard]] const AllocationStats& GetAllocationStats() const { return m_allocationStats; }
	[[nodiscard]] const SyncStats& GetSyncStats() const { return m_syncStats; }

	Core::ThreadPtr CurrentThreadHandle() const;
	Core::ThreadPtr CreateLogicalThread();
	Core::MutexPtr CreateMutex();
	bool TryLockMutex(size_t threadId, size_t mutexId);
	bool WouldDeadlockOnMutex(size_t threadId, size_t mutexId) const;
	bool UnlockMutex(size_t threadId, size_t mutexId);
	bool AssertNoDeadlock();
	bool JoinThread(size_t waitingThreadId, size_t targetThreadId);
	bool FinishThread(size_t threadId);
	[[nodiscard]] bool IsMutexLocked(size_t mutexId) const;
	[[nodiscard]] std::optional<size_t> MutexOwner(size_t mutexId) const;

	void PrintStack() const;

private:
	struct AllocationBlock
	{
		size_t size = 0;
		std::unique_ptr<uint8_t[]> data;
	};

	struct SyncThreadState
	{
		bool active = true;
		std::unordered_set<size_t> ownedMutexes;
	};

	struct SyncMutexState
	{
		std::optional<size_t> ownerThreadId;
	};

	[[nodiscard]] bool SyncHandleExists(size_t threadId) const;
	[[nodiscard]] bool MutexExists(size_t mutexId) const;
	void ClearWaitingEdgesFrom(size_t threadId);
	void AddWaitEdge(size_t fromThreadId, size_t toThreadId);
	[[nodiscard]] bool WouldIntroduceCycle(size_t fromThreadId, size_t toThreadId) const;
	[[nodiscard]] bool HasCycle() const;
	void RecomputeSyncStats();

	std::vector<Core::Value> m_valueStack;
	std::vector<CallFrame> m_frames;
	std::shared_ptr<Scope> m_currentScope;
	std::unordered_map<std::string, Core::Value> m_globals;
	std::string m_errorMessage;
	std::unordered_map<const void*, AllocationBlock> m_memoryPool;
	AllocationStats m_allocationStats;
	std::unordered_map<size_t, SyncThreadState> m_threads;
	std::unordered_map<size_t, SyncMutexState> m_mutexes;
	std::unordered_map<size_t, std::unordered_set<size_t>> m_waitGraph;
	SyncStats m_syncStats;
	size_t m_nextThreadId = 1;
	size_t m_nextMutexId = 1;
	size_t m_mainThreadId = 0;

	static constexpr size_t STACK_MAX = 4096;
	static constexpr size_t FRAMES_MAX = 64;
	static constexpr size_t SCOPE_DEPTH_MAX = 256;
};

} // namespace VM::Execution
