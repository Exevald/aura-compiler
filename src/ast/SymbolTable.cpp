#include "SymbolTable.h"

#include <ranges>

void SymbolTable::PushScope()
{
	m_scopes.emplace_back();
	m_indexStack.push(m_nextIndex);
	m_nextIndex = 0;
}

void SymbolTable::PopScope()
{
	m_scopes.pop_back();
	m_nextIndex = m_indexStack.top();
	m_indexStack.pop();
}

uint8_t SymbolTable::Define(const std::string& name)
{
	const uint8_t index = m_nextIndex++;
	m_scopes.back()[name] = { name, index };
	return index;
}

std::optional<uint8_t> SymbolTable::Resolve(const std::string& name)
{
	for (auto& m_scope : std::ranges::reverse_view(m_scopes))
	{
		if (m_scope.contains(name))
		{
			return m_scope[name].index;
		}
	}
	return std::nullopt;
}

void SymbolTable::Reset()
{
	m_scopes.clear();
	m_nextIndex = 0;
	PushScope();
}