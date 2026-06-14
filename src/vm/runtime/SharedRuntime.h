#pragma once

#include "../core/values/Value.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace VM::Runtime
{

class SharedRuntime
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

	SharedRuntime();
	~SharedRuntime();

	void DefineGlobal(const std::string& name, Core::Value value);
	bool GetGlobal(const std::string& name, Core::Value& outValue) const;
	bool SetGlobal(const std::string& name, Core::Value value);

	void* Allocate(size_t size);
	bool Release(const void* ptr);
	[[nodiscard]] AllocationStats GetAllocationStats() const;

	[[nodiscard]] static Core::ThreadPtr ThreadHandle(size_t threadId);
	[[nodiscard]] static Core::TaskPtr TaskHandle(size_t taskId);
	[[nodiscard]] static Core::ChannelPtr ChannelHandle(size_t channelId);
	[[nodiscard]] static Core::ContextPtr ContextHandle(size_t contextId);
	[[nodiscard]] size_t MainThreadId() const { return m_mainThreadId; }
	[[nodiscard]] size_t CreateExecutionThread();
	Core::ThreadPtr CreateLogicalThread();
	Core::MutexPtr CreateMutex();
	[[nodiscard]] bool IsThreadActive(size_t threadId) const;
	[[nodiscard]] bool HasMutex(size_t mutexId) const;
	bool TryLockMutex(size_t threadId, size_t mutexId);
	bool WouldDeadlockOnMutex(size_t threadId, size_t mutexId) const;
	bool UnlockMutex(size_t threadId, size_t mutexId);
	bool AssertNoDeadlock() const;
	bool JoinThread(size_t waitingThreadId, size_t targetThreadId);
	bool FinishThread(size_t threadId);
	[[nodiscard]] bool IsMutexLocked(size_t mutexId) const;
	[[nodiscard]] std::optional<size_t> MutexOwner(size_t mutexId) const;
	[[nodiscard]] SyncStats GetSyncStats() const;

	struct TaskOutcome
	{
		bool ok = true;
		Core::Value result = std::monostate{};
		std::string error;
	};

	using TaskRunner = std::function<TaskOutcome(std::stop_token)>;

	Core::TaskPtr SpawnTask(TaskRunner runner);
	bool AwaitTask(size_t taskId, Core::Value& outResult, std::string& outError);
	bool CancelTask(size_t taskId);
	[[nodiscard]] bool IsTaskDone(size_t taskId) const;
	[[nodiscard]] std::string TaskError(size_t taskId) const;

	Core::ChannelPtr CreateChannel(size_t capacity);
	bool SendChannel(size_t channelId, Core::Value value);
	bool RecvChannel(size_t channelId, Core::Value& outValue, bool& outOk);
	bool CloseChannel(size_t channelId);

	Core::ContextPtr BackgroundContext();
	Core::ContextPtr ShutdownContext();
	Core::ContextPtr CreateChildContext(size_t parentContextId);
	bool CancelContext(size_t contextId);
	[[nodiscard]] bool IsContextCancelled(size_t contextId) const;

	size_t RegisterActor(const Core::ActorPtr& actor);
	void ShutdownActors();

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

	struct TaskState
	{
		std::mutex mutex;
		std::condition_variable cv;
		std::jthread worker;
		bool completed = false;
		bool joined = false;
		bool cancelled = false;
		Core::Value result = std::monostate{};
		std::string error;
	};

	struct ChannelState
	{
		std::mutex mutex;
		std::condition_variable cvNotEmpty;
		std::condition_variable cvNotFull;
		size_t capacity = 0;
		std::deque<Core::Value> queue;
		bool closed = false;
		bool rendezvousReady = false;
		Core::Value rendezvousValue = std::monostate{};
	};

	struct ContextState
	{
		std::mutex mutex;
		size_t parentId = 0;
		bool cancelled = false;
		bool signalAware = false;
		std::unordered_set<size_t> children;
	};

	bool SyncHandleExistsLocked(size_t threadId) const;
	bool MutexExistsLocked(size_t mutexId) const;
	void ClearWaitingEdgesFromLocked(size_t threadId);
	void AddWaitEdgeLocked(size_t fromThreadId, size_t toThreadId);
	[[nodiscard]] bool WouldIntroduceCycleLocked(size_t fromThreadId, size_t toThreadId) const;
	[[nodiscard]] bool HasCycleLocked() const;
	void RecomputeSyncStatsLocked();

	mutable std::mutex m_globalsMutex;
	std::unordered_map<std::string, Core::Value> m_globals;

	mutable std::mutex m_allocMutex;
	std::unordered_map<const void*, AllocationBlock> m_memoryPool;
	AllocationStats m_allocationStats;

	mutable std::mutex m_syncMutex;
	std::unordered_map<size_t, SyncThreadState> m_threads;
	std::unordered_map<size_t, SyncMutexState> m_mutexes;
	std::unordered_map<size_t, std::unordered_set<size_t>> m_waitGraph;
	SyncStats m_syncStats;
	size_t m_nextThreadId = 1;
	size_t m_nextMutexId = 1;
	size_t m_mainThreadId = 0;

	mutable std::mutex m_taskMutex;
	std::unordered_map<size_t, std::shared_ptr<TaskState>> m_tasks;
	size_t m_nextTaskId = 1;

	mutable std::mutex m_channelMutex;
	std::unordered_map<size_t, std::shared_ptr<ChannelState>> m_channels;
	size_t m_nextChannelId = 1;

	mutable std::mutex m_contextMutex;
	std::unordered_map<size_t, std::shared_ptr<ContextState>> m_contexts;
	size_t m_nextContextId = 1;
	size_t m_backgroundContextId = 0;

	mutable std::mutex m_actorMutex;
	std::unordered_map<size_t, std::weak_ptr<Core::Actor>> m_actors;
	size_t m_nextActorId = 1;
};

} // namespace VM::Runtime
