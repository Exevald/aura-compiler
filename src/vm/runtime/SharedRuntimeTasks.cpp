#include "SharedRuntime.h"

#include <exception>

namespace VM::Runtime
{

Core::TaskPtr SharedRuntime::TaskHandle(const size_t taskId)
{
	auto task = std::make_shared<Core::TaskHandle>();
	task->id = taskId;
	return task;
}

Core::TaskPtr SharedRuntime::SpawnTask(TaskRunner runner)
{
	auto state = std::make_shared<TaskState>();
	size_t taskId = 0;
	{
		std::lock_guard lock(m_taskMutex);
		taskId = m_nextTaskId++;
		m_tasks.emplace(taskId, state);
	}

	state->worker = std::jthread(
		[state, runner = std::move(runner)](const std::stop_token& stopToken) mutable {
			TaskOutcome outcome;
			try
			{
				outcome = runner(stopToken);
			}
			catch (const std::exception& ex)
			{
				outcome.ok = false;
				outcome.error = ex.what();
			}
			catch (...)
			{
				outcome.ok = false;
				outcome.error = "Unhandled task exception";
			}

			{
				std::lock_guard lock(state->mutex);
				state->completed = true;
				state->result = std::move(outcome.result);
				state->error = outcome.ok ? std::string{} : std::move(outcome.error);
				if (stopToken.stop_requested())
				{
					state->cancelled = true;
				}
			}
			state->cv.notify_all();
		});

	return TaskHandle(taskId);
}

bool SharedRuntime::AwaitTask(const size_t taskId, Core::Value& outResult, std::string& outError)
{
	std::shared_ptr<TaskState> state;
	{
		std::lock_guard lock(m_taskMutex);
		if (const auto it = m_tasks.find(taskId); it != m_tasks.end())
		{
			state = it->second;
		}
	}
	if (!state)
	{
		return false;
	}

	bool shouldJoin = false;
	{
		std::unique_lock lock(state->mutex);
		state->cv.wait(lock, [&state] {
			return state->completed;
		});
		outResult = state->result;
		outError = state->error;
		if (!state->joined
			&& state->worker.joinable()
			&& state->worker.get_id() != std::this_thread::get_id())
		{
			state->joined = true;
			shouldJoin = true;
		}
	}
	if (shouldJoin)
	{
		state->worker.join();
	}
	return true;
}

bool SharedRuntime::CancelTask(const size_t taskId)
{
	std::shared_ptr<TaskState> state;
	{
		std::lock_guard lock(m_taskMutex);
		if (const auto it = m_tasks.find(taskId); it != m_tasks.end())
		{
			state = it->second;
		}
	}
	if (!state)
	{
		return false;
	}

	{
		std::lock_guard lock(state->mutex);
		state->cancelled = true;
	}
	state->worker.request_stop();
	state->cv.notify_all();
	return true;
}

bool SharedRuntime::IsTaskDone(const size_t taskId) const
{
	std::shared_ptr<TaskState> state;
	{
		std::lock_guard lock(m_taskMutex);
		if (const auto it = m_tasks.find(taskId); it != m_tasks.end())
		{
			state = it->second;
		}
	}
	if (!state)
	{
		return false;
	}

	std::lock_guard lock(state->mutex);
	return state->completed;
}

std::string SharedRuntime::TaskError(const size_t taskId) const
{
	std::shared_ptr<TaskState> state;
	{
		std::lock_guard lock(m_taskMutex);
		if (const auto it = m_tasks.find(taskId); it != m_tasks.end())
		{
			state = it->second;
		}
	}
	if (!state)
	{
		return {};
	}

	std::lock_guard lock(state->mutex);
	return state->error;
}

} // namespace VM::Runtime
