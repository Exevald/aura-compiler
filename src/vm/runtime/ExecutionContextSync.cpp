#include "ExecutionContext.h"

namespace VM::Execution
{

Core::ThreadPtr ExecutionContext::CurrentThreadHandle() const
{
	return m_runtime->ThreadHandle(m_currentThreadId);
}

Core::ThreadPtr ExecutionContext::CreateLogicalThread() const
{
	return m_runtime->CreateLogicalThread();
}

Core::MutexPtr ExecutionContext::CreateMutex() const
{
	return m_runtime->CreateMutex();
}

bool ExecutionContext::TryLockMutex(const size_t threadId, const size_t mutexId)
{
	if (!m_runtime->IsThreadActive(threadId))
	{
		RaiseError("Unknown thread handle");
		return false;
	}
	if (!m_runtime->HasMutex(mutexId))
	{
		RaiseError("Unknown mutex handle");
		return false;
	}

	if (const auto owner = m_runtime->MutexOwner(mutexId); owner.has_value())
	{
		if (*owner == threadId)
		{
			RaiseError("Recursive mutex lock is not allowed (self-deadlock)");
			return false;
		}
		if (m_runtime->WouldDeadlockOnMutex(threadId, mutexId))
		{
			RaiseError("Deadlock detected while locking mutex");
			return false;
		}
	}

	if (!m_runtime->TryLockMutex(threadId, mutexId))
	{
		if (!m_runtime->MutexOwner(mutexId).has_value())
		{
			RaiseError("Mutex lock failed");
		}
		return false;
	}
	return true;
}

bool ExecutionContext::WouldDeadlockOnMutex(const size_t threadId, const size_t mutexId) const
{
	return m_runtime->WouldDeadlockOnMutex(threadId, mutexId);
}

bool ExecutionContext::UnlockMutex(const size_t threadId, const size_t mutexId)
{
	if (!m_runtime->IsThreadActive(threadId))
	{
		RaiseError("Unknown thread handle");
		return false;
	}
	if (!m_runtime->HasMutex(mutexId))
	{
		RaiseError("Unknown mutex handle");
		return false;
	}
	if (m_runtime->MutexOwner(mutexId) != threadId)
	{
		RaiseError("Mutex unlock by non-owner is not allowed");
		return false;
	}
	if (!m_runtime->UnlockMutex(threadId, mutexId))
	{
		RaiseError("Mutex unlock failed");
		return false;
	}
	return true;
}

bool ExecutionContext::AssertNoDeadlock()
{
	if (!m_runtime->AssertNoDeadlock())
	{
		RaiseError("Deadlock detected");
		return false;
	}
	return true;
}

bool ExecutionContext::JoinThread(const size_t waitingThreadId, const size_t targetThreadId)
{
	if (!m_runtime->JoinThread(waitingThreadId, targetThreadId))
	{
		RaiseError("Thread join failed");
		return false;
	}
	return true;
}

bool ExecutionContext::FinishThread(const size_t threadId)
{
	if (!m_runtime->FinishThread(threadId))
	{
		RaiseError("Unknown thread handle");
		return false;
	}
	return true;
}

bool ExecutionContext::IsMutexLocked(const size_t mutexId) const
{
	return m_runtime->IsMutexLocked(mutexId);
}

std::optional<size_t> ExecutionContext::MutexOwner(const size_t mutexId) const
{
	return m_runtime->MutexOwner(mutexId);
}

} // namespace VM::Execution
