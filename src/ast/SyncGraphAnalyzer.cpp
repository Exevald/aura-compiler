#include "SyncGraphAnalyzer.h"

#include <vector>

bool SyncGraphAnalyzer::WouldCycle(
	const Graph& graph,
	const std::string& from,
	const std::string& to)
{
	if (from == to)
	{
		return true;
	}

	std::unordered_set<std::string> visited;
	std::vector<std::string> stack{ to };
	while (!stack.empty())
	{
		std::string current = std::move(stack.back());
		stack.pop_back();
		if (!visited.insert(current).second)
		{
			continue;
		}
		if (current == from)
		{
			return true;
		}
		if (const auto it = graph.find(current); it != graph.end())
		{
			for (const auto& next : it->second)
			{
				stack.push_back(next);
			}
		}
	}
	return false;
}

void SyncGraphAnalyzer::AddEdge(Graph& graph, const std::string& from, const std::string& to)
{
	graph[from].insert(to);
}

void SyncGraphAnalyzer::SetError(std::string message)
{
	m_lastError = std::move(message);
}

void SyncGraphAnalyzer::ClearError()
{
	m_lastError.clear();
}

bool SyncGraphAnalyzer::AddLockEdge(
	const std::string& fromMutex,
	const std::string& toMutex,
	const std::string& context)
{
	if (WouldCycle(m_lockGraph, fromMutex, toMutex))
	{
		SetError(
			"Potential deadlock detected at compile time: lock-order cycle involving '"
			+ fromMutex + "' -> '" + toMutex + "' in " + context);
		return false;
	}

	AddEdge(m_lockGraph, fromMutex, toMutex);
	ClearError();
	return true;
}

bool SyncGraphAnalyzer::AddJoinEdge(
	const std::string& fromThread,
	const std::string& toThread,
	const std::string& context)
{
	if (WouldCycle(m_joinGraph, fromThread, toThread))
	{
		SetError(
			"Potential deadlock detected at compile time: join cycle involving '"
			+ fromThread + "' -> '" + toThread + "' in " + context);
		return false;
	}

	AddEdge(m_joinGraph, fromThread, toThread);
	ClearError();
	return true;
}
