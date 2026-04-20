#pragma once

#include "../core/values/Value.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
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

	mutable std::mutex m_actorMutex;
	std::unordered_map<size_t, std::weak_ptr<Core::Actor>> m_actors;
	size_t m_nextActorId = 1;
};

} // namespace VM::Runtime
