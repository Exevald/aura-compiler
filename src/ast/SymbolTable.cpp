#include "SymbolTable.h"

#include <ranges>
#include <stdexcept>

void SymbolTable::PushScope()
{
	m_scopes.emplace_back();
	m_indexStack.push(m_nextIndex);
}

void SymbolTable::PopScope()
{
	if (m_scopes.empty() || m_indexStack.empty())
	{
		throw std::runtime_error("SymbolTable::PopScope called with no active scope");
	}

	m_scopes.pop_back();
	m_nextIndex = m_indexStack.top();
	m_indexStack.pop();
}

uint8_t SymbolTable::Define(const std::string& name)
{
	if (m_scopes.empty())
	{
		throw std::runtime_error("SymbolTable::Define called with no active scope");
	}

	const uint8_t index = m_nextIndex++;
	m_scopes.back()[name] = { name, index };
	return index;
}

std::optional<uint8_t> SymbolTable::Resolve(const std::string& name) const
{
	for (auto& m_scope : std::ranges::reverse_view(m_scopes))
	{
		if (const auto it = m_scope.find(name);
			it != m_scope.end())
		{
			return it->second.index;
		}
	}
	return std::nullopt;
}

void SymbolTable::Reset()
{
	m_scopes.clear();
	while (!m_indexStack.empty())
	{
		m_indexStack.pop();
	}
	m_nextIndex = 0;
	PushScope();
}