#include "SharedRuntime.h"

#include <ranges>
#include <vector>

namespace VM::Runtime
{

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
	for (const auto& [active, ownedMutexes] : m_threads | std::views::values)
	{
		(void)ownedMutexes;
		if (active)
		{
			++activeThreads;
		}
	}
	m_syncStats.threadCount = activeThreads;
	m_syncStats.mutexCount = m_mutexes.size();

	size_t waitEdges = 0;
	for (const auto& deps : m_waitGraph | std::views::values)
	{
		waitEdges += deps.size();
	}
	m_syncStats.waitEdgeCount = waitEdges;
}

bool SharedRuntime::TryLockMutex(const size_t threadId, const size_t mutexId)
{
	std::lock_guard lock(m_syncMutex);
	if (!SyncHandleExistsLocked(threadId) || !MutexExistsLocked(mutexId))
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

} // namespace VM::Runtime
