#include "ExecutionContext.h"

#include <stdexcept>
#include <utility>

namespace VM::Execution
{

void ExecutionContext::PushFrame(Core::FunctionPtr func, Core::ClosurePtr closure, const size_t base)
{
	if (m_frames.size() >= FRAMES_MAX)
	{
		throw std::runtime_error("Stack overflow (too many frames)");
	}
	m_frames.emplace_back(
		std::move(func),
		std::move(closure),
		base,
		m_activeTransactions.size(),
		m_handlerStack.size());
}

void ExecutionContext::PopFrame()
{
	if (m_frames.empty())
	{
		throw std::runtime_error("Stack underflow (no frames to pop)");
	}
	m_frames.pop_back();
}

CallFrame& ExecutionContext::CurrentFrame()
{
	if (m_frames.empty())
	{
		throw std::runtime_error("No active call frame");
	}
	return m_frames.back();
}

const CallFrame& ExecutionContext::CurrentFrame() const
{
	if (m_frames.empty())
	{
		throw std::runtime_error("No active call frame");
	}
	return m_frames.back();
}

void ExecutionContext::SetLocal(const size_t index, const Core::Value& val)
{
	const size_t absoluteIndex = m_frames.back().stackBase + index;
	if (absoluteIndex >= m_valueStack.size())
	{
		throw std::out_of_range("Local index out of bounds");
	}
	m_valueStack[absoluteIndex] = val;
}

const Core::Value& ExecutionContext::GetLocal(const size_t index) const
{
	const size_t absoluteIndex = m_frames.back().stackBase + index;
	if (absoluteIndex >= m_valueStack.size())
	{
		throw std::out_of_range("Local index out of bounds");
	}
	return m_valueStack[absoluteIndex];
}

void ExecutionContext::SetUpvalue(const size_t index, const Core::Value& val)
{
	const auto& frame = CurrentFrame();
	if (!frame.closure || index >= frame.closure->captures.size())
	{
		throw std::out_of_range("Upvalue index out of bounds");
	}
	frame.closure->captures[index] = val;
}

const Core::Value& ExecutionContext::GetUpvalue(const size_t index) const
{
	const auto& frame = CurrentFrame();
	if (!frame.closure || index >= frame.closure->captures.size())
	{
		throw std::out_of_range("Upvalue index out of bounds");
	}
	return frame.closure->captures[index];
}

} // namespace VM::Execution
