#include "SharedRuntime.h"

#include <ranges>
#include <vector>

namespace VM::Runtime
{

SharedRuntime::SharedRuntime()
{
	std::lock_guard lock(m_syncMutex);
	m_threads.emplace(m_mainThreadId, SyncThreadState{});
	RecomputeSyncStatsLocked();
}

SharedRuntime::~SharedRuntime()
{
	ShutdownActors();
}

void SharedRuntime::DefineGlobal(const std::string& name, Core::Value value)
{
	std::lock_guard lock(m_globalsMutex);
	m_globals[name] = std::move(value);
}

bool SharedRuntime::GetGlobal(const std::string& name, Core::Value& outValue) const
{
	std::lock_guard lock(m_globalsMutex);
	if (const auto it = m_globals.find(name); it != m_globals.end())
	{
		outValue = it->second;
		return true;
	}
	return false;
}

bool SharedRuntime::SetGlobal(const std::string& name, Core::Value value)
{
	std::lock_guard lock(m_globalsMutex);
	if (const auto it = m_globals.find(name); it != m_globals.end())
	{
		it->second = std::move(value);
		return true;
	}
	return false;
}

void* SharedRuntime::Allocate(const size_t size)
{
	auto block = std::make_unique<uint8_t[]>(size);
	void* ptr = block.get();

	std::lock_guard lock(m_allocMutex);
	m_memoryPool.emplace(ptr, AllocationBlock{ size, std::move(block) });
	++m_allocationStats.activeAllocations;
	m_allocationStats.activeBytes += size;
	++m_allocationStats.totalAllocations;
	m_allocationStats.totalBytes += size;
	return ptr;
}

bool SharedRuntime::Release(const void* ptr)
{
	std::lock_guard lock(m_allocMutex);
	if (const auto it = m_memoryPool.find(ptr); it != m_memoryPool.end())
	{
		--m_allocationStats.activeAllocations;
		m_allocationStats.activeBytes -= it->second.size;
		m_memoryPool.erase(it);
		return true;
	}
	return false;
}

SharedRuntime::AllocationStats SharedRuntime::GetAllocationStats() const
{
	std::lock_guard lock(m_allocMutex);
	return m_allocationStats;
}

Core::ThreadPtr SharedRuntime::ThreadHandle(const size_t threadId)
{
	auto thread = std::make_shared<Core::ThreadHandle>();
	thread->id = threadId;
	return thread;
}

size_t SharedRuntime::CreateExecutionThread()
{
	std::lock_guard lock(m_syncMutex);
	const size_t threadId = m_nextThreadId++;
	m_threads.emplace(threadId, SyncThreadState{});
	RecomputeSyncStatsLocked();
	return threadId;
}

Core::ThreadPtr SharedRuntime::CreateLogicalThread()
{
	return ThreadHandle(CreateExecutionThread());
}

Core::MutexPtr SharedRuntime::CreateMutex()
{
	std::lock_guard lock(m_syncMutex);
	auto mutex = std::make_shared<Core::MutexHandle>();
	mutex->id = m_nextMutexId++;
	m_mutexes.emplace(mutex->id, SyncMutexState{});
	RecomputeSyncStatsLocked();
	return mutex;
}

bool SharedRuntime::IsThreadActive(const size_t threadId) const
{
	std::lock_guard lock(m_syncMutex);
	return SyncHandleExistsLocked(threadId);
}

bool SharedRuntime::HasMutex(const size_t mutexId) const
{
	std::lock_guard lock(m_syncMutex);
	return MutexExistsLocked(mutexId);
}

bool SharedRuntime::SyncHandleExistsLocked(const size_t threadId) const
{
	return m_threads.contains(threadId) && m_threads.at(threadId).active;
}

bool SharedRuntime::MutexExistsLocked(const size_t mutexId) const
{
	return m_mutexes.contains(mutexId);
}

void SharedRuntime::ClearWaitingEdgesFromLocked(const size_t threadId)
{
	m_waitGraph.erase(threadId);
	RecomputeSyncStatsLocked();
}

void SharedRuntime::AddWaitEdgeLocked(const size_t fromThreadId, const size_t toThreadId)
{
	m_waitGraph[fromThreadId].insert(toThreadId);
	RecomputeSyncStatsLocked();
}

bool SharedRuntime::WouldIntroduceCycleLocked(const size_t fromThreadId, const size_t toThreadId) const
{
	if (fromThreadId == toThreadId)
	{
		return true;
	}

	std::unordered_set<size_t> visited;
	std::vector stack{ toThreadId };
	while (!stack.empty())
	{
		const size_t current = stack.back();
		stack.pop_back();
		if (!visited.insert(current).second)
		{
			continue;
		}
		if (current == fromThreadId)
		{
			return true;
		}
		if (const auto it = m_waitGraph.find(current); it != m_waitGraph.end())
		{
			for (const auto next : it->second)
			{
				stack.push_back(next);
			}
		}
	}
	return false;
}

bool SharedRuntime::HasCycleLocked() const
{
	for (const auto& [threadId, deps] : m_waitGraph)
	{
		for (const auto dep : deps)
		{
			if (WouldIntroduceCycleLocked(threadId, dep))
			{
				return true;
			}
		}
	}
	return false;
}

void SharedRuntime::RecomputeSyncStatsLocked()
{
	size_t activeThreads = 0;
	for (const auto& [_, state] : m_threads)
	{
		if (state.active)
		{
			++activeThreads;
		}
	}
	m_syncStats.threadCount = activeThreads;
	m_syncStats.mutexCount = m_mutexes.size();
	size_t waitEdges = 0;
	for (const auto& [_, deps] : m_waitGraph)
	{
		waitEdges += deps.size();
	}
	m_syncStats.waitEdgeCount = waitEdges;
}

bool SharedRuntime::TryLockMutex(const size_t threadId, const size_t mutexId)
{
	std::lock_guard lock(m_syncMutex);
	if (!SyncHandleExistsLocked(threadId))
	{
		return false;
	}
	if (!MutexExistsLocked(mutexId))
	{
		return false;
	}

	auto& mutexState = m_mutexes.at(mutexId);
	if (!mutexState.ownerThreadId.has_value())
	{
		mutexState.ownerThreadId = threadId;
		m_threads.at(threadId).ownedMutexes.insert(mutexId);
		ClearWaitingEdgesFromLocked(threadId);
		return true;
	}

	const size_t ownerId = *mutexState.ownerThreadId;
	AddWaitEdgeLocked(threadId, ownerId);
	if (WouldIntroduceCycleLocked(threadId, ownerId))
	{
		ClearWaitingEdgesFromLocked(threadId);
		return false;
	}
	return false;
}

bool SharedRuntime::WouldDeadlockOnMutex(const size_t threadId, const size_t mutexId) const
{
	std::lock_guard lock(m_syncMutex);
	if (!SyncHandleExistsLocked(threadId) || !MutexExistsLocked(mutexId))
	{
		return false;
	}
	const auto& mutexState = m_mutexes.at(mutexId);
	if (!mutexState.ownerThreadId.has_value())
	{
		return false;
	}
	return WouldIntroduceCycleLocked(threadId, *mutexState.ownerThreadId);
}

bool SharedRuntime::UnlockMutex(const size_t threadId, const size_t mutexId)
{
	std::lock_guard lock(m_syncMutex);
	if (!SyncHandleExistsLocked(threadId) || !MutexExistsLocked(mutexId))
	{
		return false;
	}
	auto& mutexState = m_mutexes.at(mutexId);
	if (mutexState.ownerThreadId != threadId)
	{
		return false;
	}
	mutexState.ownerThreadId.reset();
	m_threads.at(threadId).ownedMutexes.erase(mutexId);
	ClearWaitingEdgesFromLocked(threadId);
	return true;
}

bool SharedRuntime::AssertNoDeadlock() const
{
	std::lock_guard lock(m_syncMutex);
	return !HasCycleLocked();
}

bool SharedRuntime::JoinThread(const size_t waitingThreadId, const size_t targetThreadId)
{
	std::lock_guard lock(m_syncMutex);
	if (!SyncHandleExistsLocked(waitingThreadId) || !SyncHandleExistsLocked(targetThreadId))
	{
		return false;
	}
	AddWaitEdgeLocked(waitingThreadId, targetThreadId);
	if (WouldIntroduceCycleLocked(waitingThreadId, targetThreadId))
	{
		ClearWaitingEdgesFromLocked(waitingThreadId);
		return false;
	}
	return true;
}

bool SharedRuntime::FinishThread(const size_t threadId)
{
	std::lock_guard lock(m_syncMutex);
	if (!m_threads.contains(threadId))
	{
		return false;
	}
	auto& threadState = m_threads.at(threadId);
	threadState.active = false;
	for (const auto mutexId : threadState.ownedMutexes)
	{
		if (m_mutexes.contains(mutexId))
		{
			m_mutexes.at(mutexId).ownerThreadId.reset();
		}
	}
	threadState.ownedMutexes.clear();
	ClearWaitingEdgesFromLocked(threadId);
	for (auto& deps : m_waitGraph | std::views::values)
	{
		deps.erase(threadId);
	}
	RecomputeSyncStatsLocked();
	return true;
}

bool SharedRuntime::IsMutexLocked(const size_t mutexId) const
{
	std::lock_guard lock(m_syncMutex);
	return MutexExistsLocked(mutexId) && m_mutexes.at(mutexId).ownerThreadId.has_value();
}

std::optional<size_t> SharedRuntime::MutexOwner(const size_t mutexId) const
{
	std::lock_guard lock(m_syncMutex);
	if (!MutexExistsLocked(mutexId))
	{
		return std::nullopt;
	}
	return m_mutexes.at(mutexId).ownerThreadId;
}

SharedRuntime::SyncStats SharedRuntime::GetSyncStats() const
{
	std::lock_guard lock(m_syncMutex);
	return m_syncStats;
}

size_t SharedRuntime::RegisterActor(const Core::ActorPtr& actor)
{
	std::lock_guard lock(m_actorMutex);
	const size_t actorId = m_nextActorId++;
	m_actors.emplace(actorId, actor);
	return actorId;
}

void SharedRuntime::ShutdownActors()
{
	std::vector<Core::ActorPtr> actors;
	{
		std::lock_guard lock(m_actorMutex);
		for (auto it = m_actors.begin(); it != m_actors.end();)
		{
			if (const auto actor = it->second.lock())
			{
				actors.push_back(actor);
				++it;
			}
			else
			{
				it = m_actors.erase(it);
			}
		}
	}

	for (const auto& actor : actors)
	{
		{
			std::lock_guard lock(actor->mutex);
			actor->stopping = true;
		}
		actor->cv.notify_all();
	}
}

} // namespace VM::Runtime
