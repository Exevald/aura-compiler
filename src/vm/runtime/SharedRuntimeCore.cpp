#include "SharedRuntime.h"

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

} // namespace VM::Runtime
