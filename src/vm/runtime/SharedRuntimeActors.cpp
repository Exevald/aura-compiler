#include "SharedRuntime.h"

#include <vector>

namespace VM::Runtime
{

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
		actor->worker.request_stop();
		actor->cv.notify_all();
	}
}

} // namespace VM::Runtime
