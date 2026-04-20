#include "../../core/values/ValueHelper.h"
#include "../../runtime/SharedRuntime.h"

#include <atomic>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using VM::Runtime::SharedRuntime;
using namespace VM::Core;

namespace
{

std::string GlobalKey(const size_t workerId, const size_t iteration)
{
	return "stress.global." + std::to_string(workerId) + "." + std::to_string(iteration);
}

} // namespace

TEST(SharedRuntimeTest, ConcurrentGlobalDefineAndReadStress)
{
	SharedRuntime runtime;
	constexpr size_t workerCount = 8;
	constexpr size_t iterationsPerWorker = 400;
	std::atomic<size_t> ready = 0;
	std::atomic start = false;
	std::vector<std::thread> workers;
	workers.reserve(workerCount);

	for (size_t workerId = 0; workerId < workerCount; ++workerId)
	{
		workers.emplace_back([&, workerId]() {
			ready.fetch_add(1, std::memory_order_release);
			while (!start.load(std::memory_order_acquire))
			{
				std::this_thread::yield();
			}

			for (size_t iteration = 0; iteration < iterationsPerWorker; ++iteration)
			{
				const auto key = GlobalKey(workerId, iteration);
				runtime.DefineGlobal(key, static_cast<int64_t>(workerId * 100000 + iteration));

				Value value;
				ASSERT_TRUE(runtime.GetGlobal(key, value));
				ASSERT_TRUE(std::holds_alternative<int64_t>(value));
				EXPECT_EQ(std::get<int64_t>(value), static_cast<int64_t>(workerId * 100000 + iteration));
			}
		});
	}

	while (ready.load(std::memory_order_acquire) != workerCount)
	{
		std::this_thread::yield();
	}
	start.store(true, std::memory_order_release);

	for (auto& worker : workers)
	{
		worker.join();
	}

	for (size_t workerId = 0; workerId < workerCount; ++workerId)
	{
		const auto key = GlobalKey(workerId, iterationsPerWorker - 1);
		Value value;
		ASSERT_TRUE(runtime.GetGlobal(key, value));
		EXPECT_EQ(
			std::get<int64_t>(value),
			static_cast<int64_t>(workerId * 100000 + iterationsPerWorker - 1));
	}
}

TEST(SharedRuntimeTest, ConcurrentAllocationReleaseStress)
{
	SharedRuntime runtime;
	constexpr size_t workerCount = 6;
	constexpr size_t allocationsPerWorker = 600;
	std::atomic<size_t> ready = 0;
	std::atomic start = false;
	std::vector<std::thread> workers;
	workers.reserve(workerCount);

	for (size_t workerId = 0; workerId < workerCount; ++workerId)
	{
		workers.emplace_back([&, workerId]() {
			ready.fetch_add(1, std::memory_order_release);
			while (!start.load(std::memory_order_acquire))
			{
				std::this_thread::yield();
			}

			std::vector<void*> owned;
			owned.reserve(allocationsPerWorker);
			for (size_t i = 0; i < allocationsPerWorker; ++i)
			{
				const size_t bytes = 16 + ((workerId + i) % 32);
				void* ptr = runtime.Allocate(bytes);
				ASSERT_NE(ptr, nullptr);
				owned.push_back(ptr);

				if (i % 3 == 2)
				{
					ASSERT_TRUE(runtime.Release(owned.back()));
					owned.pop_back();
				}
			}

			for (auto it = owned.rbegin(); it != owned.rend(); ++it)
			{
				ASSERT_TRUE(runtime.Release(*it));
			}
		});
	}

	while (ready.load(std::memory_order_acquire) != workerCount)
	{
		std::this_thread::yield();
	}
	start.store(true, std::memory_order_release);

	for (auto& worker : workers)
	{
		worker.join();
	}

	const auto stats = runtime.GetAllocationStats();
	EXPECT_EQ(stats.activeAllocations, 0);
	EXPECT_EQ(stats.activeBytes, 0);
	EXPECT_EQ(stats.totalAllocations, workerCount * allocationsPerWorker);
	EXPECT_GT(stats.totalBytes, 0);
}

TEST(SharedRuntimeTest, DeadlockDetectionChainStress)
{
	constexpr size_t rounds = 250;
	for (size_t round = 0; round < rounds; ++round)
	{
		SharedRuntime runtime;
		const size_t t1 = runtime.CreateExecutionThread();
		const size_t t2 = runtime.CreateExecutionThread();
		const size_t t3 = runtime.CreateExecutionThread();

		const auto m1 = runtime.CreateMutex();
		const auto m2 = runtime.CreateMutex();
		const auto m3 = runtime.CreateMutex();

		ASSERT_TRUE(runtime.TryLockMutex(t1, m1->id));
		ASSERT_TRUE(runtime.TryLockMutex(t2, m2->id));
		ASSERT_TRUE(runtime.TryLockMutex(t3, m3->id));

		EXPECT_FALSE(runtime.TryLockMutex(t1, m2->id));
		EXPECT_FALSE(runtime.TryLockMutex(t2, m3->id));
		EXPECT_TRUE(runtime.WouldDeadlockOnMutex(t3, m1->id));
		EXPECT_FALSE(runtime.TryLockMutex(t3, m1->id));
		EXPECT_TRUE(runtime.AssertNoDeadlock());

		EXPECT_TRUE(runtime.UnlockMutex(t1, m1->id));
		EXPECT_TRUE(runtime.UnlockMutex(t2, m2->id));
		EXPECT_TRUE(runtime.UnlockMutex(t3, m3->id));
		EXPECT_TRUE(runtime.FinishThread(t1));
		EXPECT_TRUE(runtime.FinishThread(t2));
		EXPECT_TRUE(runtime.FinishThread(t3));
	}
}

TEST(SharedRuntimeTest, ConcurrentThreadAndMutexCreationStress)
{
	SharedRuntime runtime;
	constexpr size_t workerCount = 8;
	constexpr size_t creationsPerWorker = 250;
	std::atomic<size_t> ready = 0;
	std::atomic start = false;
	std::vector<std::thread> workers;
	workers.reserve(workerCount);

	for (size_t workerId = 0; workerId < workerCount; ++workerId)
	{
		workers.emplace_back([&]() {
			ready.fetch_add(1, std::memory_order_release);
			while (!start.load(std::memory_order_acquire))
			{
				std::this_thread::yield();
			}

			for (size_t i = 0; i < creationsPerWorker; ++i)
			{
				const size_t threadId = runtime.CreateExecutionThread();
				const auto mutex = runtime.CreateMutex();
				ASSERT_NE(mutex, nullptr);
				ASSERT_TRUE(runtime.TryLockMutex(threadId, mutex->id));
				ASSERT_TRUE(runtime.UnlockMutex(threadId, mutex->id));
				ASSERT_TRUE(runtime.FinishThread(threadId));
			}
		});
	}

	while (ready.load(std::memory_order_acquire) != workerCount)
	{
		std::this_thread::yield();
	}
	start.store(true, std::memory_order_release);

	for (auto& worker : workers)
	{
		worker.join();
	}

	const auto [threadCount, mutexCount, waitEdgeCount] = runtime.GetSyncStats();
	EXPECT_EQ(threadCount, 1);
	EXPECT_EQ(mutexCount, workerCount * creationsPerWorker);
	EXPECT_EQ(waitEdgeCount, 0);
}
