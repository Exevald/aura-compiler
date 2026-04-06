#pragma once

#include <optional>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

struct Symbol
{
	std::string name;
	uint8_t index{};
};

class SymbolTable
{
public:
	void PushScope();
	void PopScope();

	uint8_t Define(const std::string& name);
	std::optional<uint8_t> Resolve(const std::string& name) const;
	void Reset();

private:
	std::vector<std::unordered_map<std::string, Symbol>> m_scopes;
	std::stack<uint8_t> m_indexStack;
	uint8_t m_nextIndex = 0;
};
