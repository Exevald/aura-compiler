#include "SharedRuntime.h"

#include <csignal>
#include <mutex>

namespace VM::Runtime
{

namespace
{

volatile std::sig_atomic_t g_shutdownSignalSeen = 0;
std::once_flag g_shutdownSignalInstallOnce;

void HandleShutdownSignal(int)
{
	g_shutdownSignalSeen = 1;
}

void InstallShutdownSignalHandlers()
{
	std::signal(SIGINT, HandleShutdownSignal);
	std::signal(SIGTERM, HandleShutdownSignal);
}

[[nodiscard]] bool ShutdownSignalSeen()
{
	return g_shutdownSignalSeen != 0;
}

} // namespace

Core::ChannelPtr SharedRuntime::ChannelHandle(const size_t channelId)
{
	auto channel = std::make_shared<Core::ChannelHandle>();
	channel->id = channelId;
	return channel;
}

Core::ContextPtr SharedRuntime::ContextHandle(const size_t contextId)
{
	auto context = std::make_shared<Core::ContextHandle>();
	context->id = contextId;
	return context;
}

Core::ChannelPtr SharedRuntime::CreateChannel(const size_t capacity)
{
	std::lock_guard lock(m_channelMutex);
	const size_t channelId = m_nextChannelId++;
	auto state = std::make_shared<ChannelState>();
	state->capacity = capacity;
	m_channels.emplace(channelId, std::move(state));
	return ChannelHandle(channelId);
}

bool SharedRuntime::SendChannel(const size_t channelId, Core::Value value)
{
	std::shared_ptr<ChannelState> state;
	{
		std::lock_guard lock(m_channelMutex);
		const auto it = m_channels.find(channelId);
		if (it == m_channels.end())
		{
			return false;
		}
		state = it->second;
	}

	std::unique_lock lock(state->mutex);
	if (state->capacity == 0)
	{
		state->cvNotFull.wait(lock, [&] {
			return state->closed || !state->rendezvousReady;
		});
		if (state->closed)
		{
			return false;
		}
		state->rendezvousValue = std::move(value);
		state->rendezvousReady = true;
		state->cvNotEmpty.notify_one();
		state->cvNotFull.wait(lock, [&] {
			return state->closed || !state->rendezvousReady;
		});
		return !state->closed;
	}

	state->cvNotFull.wait(lock, [&] {
		return state->closed || state->queue.size() < state->capacity;
	});
	if (state->closed)
	{
		return false;
	}
	state->queue.push_back(std::move(value));
	state->cvNotEmpty.notify_one();
	return true;
}

bool SharedRuntime::RecvChannel(const size_t channelId, Core::Value& outValue, bool& outOk)
{
	std::shared_ptr<ChannelState> state;
	{
		std::lock_guard lock(m_channelMutex);
		const auto it = m_channels.find(channelId);
		if (it == m_channels.end())
		{
			return false;
		}
		state = it->second;
	}

	std::unique_lock lock(state->mutex);
	if (state->capacity == 0)
	{
		state->cvNotEmpty.wait(lock, [&] {
			return state->closed || state->rendezvousReady;
		});
		if (!state->rendezvousReady)
		{
			outValue = std::monostate{};
			outOk = false;
			return true;
		}
		outValue = state->rendezvousValue;
		outOk = true;
		state->rendezvousValue = std::monostate{};
		state->rendezvousReady = false;
		state->cvNotFull.notify_one();
		return true;
	}

	state->cvNotEmpty.wait(lock, [&] {
		return state->closed || !state->queue.empty();
	});
	if (state->queue.empty())
	{
		outValue = std::monostate{};
		outOk = false;
		return true;
	}
	outValue = state->queue.front();
	state->queue.pop_front();
	outOk = true;
	state->cvNotFull.notify_one();
	return true;
}

bool SharedRuntime::CloseChannel(const size_t channelId)
{
	std::shared_ptr<ChannelState> state;
	{
		std::lock_guard lock(m_channelMutex);
		const auto it = m_channels.find(channelId);
		if (it == m_channels.end())
		{
			return false;
		}
		state = it->second;
	}

	std::lock_guard lock(state->mutex);
	if (state->closed)
	{
		return false;
	}
	state->closed = true;
	state->cvNotEmpty.notify_all();
	state->cvNotFull.notify_all();
	return true;
}

Core::ContextPtr SharedRuntime::BackgroundContext()
{
	std::lock_guard lock(m_contextMutex);
	if (m_backgroundContextId == 0)
	{
		m_backgroundContextId = m_nextContextId++;
		m_contexts.emplace(m_backgroundContextId, std::make_shared<ContextState>());
	}
	return ContextHandle(m_backgroundContextId);
}

Core::ContextPtr SharedRuntime::ShutdownContext()
{
	std::call_once(g_shutdownSignalInstallOnce, InstallShutdownSignalHandlers);
	const auto parent = BackgroundContext();
	if (!parent)
	{
		return {};
	}

	std::lock_guard lock(m_contextMutex);
	const size_t contextId = m_nextContextId++;
	auto child = std::make_shared<ContextState>();
	child->parentId = parent->id;
	child->cancelled = ShutdownSignalSeen();
	child->signalAware = true;
	if (const auto parentIt = m_contexts.find(parent->id); parentIt != m_contexts.end())
	{
		std::lock_guard parentLock(parentIt->second->mutex);
		parentIt->second->children.insert(contextId);
	}
	m_contexts.emplace(contextId, std::move(child));
	return ContextHandle(contextId);
}

Core::ContextPtr SharedRuntime::CreateChildContext(const size_t parentContextId)
{
	std::lock_guard lock(m_contextMutex);
	const auto parentIt = m_contexts.find(parentContextId);
	if (parentIt == m_contexts.end())
	{
		return {};
	}

	const size_t contextId = m_nextContextId++;
	auto child = std::make_shared<ContextState>();
	child->parentId = parentContextId;
	{
		std::lock_guard childLock(child->mutex);
		std::lock_guard parentLock(parentIt->second->mutex);
		child->cancelled = parentIt->second->cancelled;
		child->signalAware = parentIt->second->signalAware;
		parentIt->second->children.insert(contextId);
	}
	m_contexts.emplace(contextId, std::move(child));
	return ContextHandle(contextId);
}

bool SharedRuntime::CancelContext(const size_t contextId)
{
	std::lock_guard lock(m_contextMutex);
	if (!m_contexts.contains(contextId))
	{
		return false;
	}
	std::vector<size_t> pending{ contextId };
	while (!pending.empty())
	{
		const size_t currentId = pending.back();
		pending.pop_back();

		const auto it = m_contexts.find(currentId);
		if (it == m_contexts.end())
		{
			continue;
		}

		std::vector<size_t> children;
		{
			std::lock_guard stateLock(it->second->mutex);
			if (it->second->cancelled)
			{
				continue;
			}
			it->second->cancelled = true;
			children.assign(it->second->children.begin(), it->second->children.end());
		}
		pending.insert(pending.end(), children.begin(), children.end());
	}
	return true;
}

bool SharedRuntime::IsContextCancelled(const size_t contextId) const
{
	std::shared_ptr<ContextState> state;
	{
		std::lock_guard lock(m_contextMutex);
		const auto it = m_contexts.find(contextId);
		if (it == m_contexts.end())
		{
			return false;
		}
		state = it->second;
	}

	std::lock_guard lock(state->mutex);
	return state->cancelled || (state->signalAware && ShutdownSignalSeen());
}

} // namespace VM::Runtime
