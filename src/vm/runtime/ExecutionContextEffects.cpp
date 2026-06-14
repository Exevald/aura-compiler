#include "ExecutionContext.h"

namespace VM::Execution
{

void ExecutionContext::PushHandlerMap(const Core::HandlerMapPtr& handlerMap)
{
	m_handlerStack.push_back(handlerMap);
}

bool ExecutionContext::PopHandlerMap()
{
	if (m_handlerStack.empty())
	{
		RaiseError("Handler stack underflow");
		return false;
	}
	m_handlerStack.pop_back();
	return true;
}

void ExecutionContext::UnwindHandlers(const size_t base)
{
	while (m_handlerStack.size() > base)
	{
		m_handlerStack.pop_back();
	}
}

Core::Value ExecutionContext::ResolveHandledEffect(const std::string& effectName) const
{
	for (auto it = m_handlerStack.rbegin(); it != m_handlerStack.rend(); ++it)
	{
		if (*it && (*it)->handlers.contains(effectName))
		{
			return (*it)->handlers.at(effectName);
		}
	}
	return std::monostate{};
}

void ExecutionContext::ClearHandlers()
{
	m_handlerStack.clear();
}

} // namespace VM::Execution
