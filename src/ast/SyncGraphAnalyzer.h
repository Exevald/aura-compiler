#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

class SyncGraphAnalyzer
{
public:
	[[nodiscard]] bool AddLockEdge(
		const std::string& fromMutex,
		const std::string& toMutex,
		const std::string& context);
	[[nodiscard]] bool AddJoinEdge(
		const std::string& fromThread,
		const std::string& toThread,
		const std::string& context);
	[[nodiscard]] const std::string& LastError() const { return m_lastError; }

private:
	using Graph = std::unordered_map<std::string, std::unordered_set<std::string>>;

	[[nodiscard]] static bool WouldCycle(
		const Graph& graph,
		const std::string& from,
		const std::string& to);
	static void AddEdge(Graph& graph, const std::string& from, const std::string& to);
	void SetError(std::string message);
	void ClearError();

	Graph m_lockGraph;
	Graph m_joinGraph;
	std::string m_lastError;
};
