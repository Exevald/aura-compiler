#include "SyncRustBridge.h"

#include <stdexcept>

extern "C"
{
struct AuraSyncGraph;

AuraSyncGraph* aura_sync_graph_new();
void aura_sync_graph_free(AuraSyncGraph* graph);
bool aura_sync_graph_add_lock_edge(
	AuraSyncGraph* graph,
	const char* from_mutex,
	const char* to_mutex,
	const char* context);
bool aura_sync_graph_add_join_edge(
	AuraSyncGraph* graph,
	const char* from_thread,
	const char* to_thread,
	const char* context);
const char* aura_sync_graph_last_error(const AuraSyncGraph* graph);
}

struct SyncRustBridge::Impl
{
	AuraSyncGraph* graph = nullptr;
};

SyncRustBridge::SyncRustBridge()
	: m_impl(std::make_unique<Impl>())
{
	m_impl->graph = aura_sync_graph_new();
	if (!m_impl->graph)
	{
		throw std::runtime_error("Failed to initialize Rust sync graph analyzer");
	}
}

SyncRustBridge::~SyncRustBridge()
{
	if (m_impl && m_impl->graph)
	{
		aura_sync_graph_free(m_impl->graph);
	}
}

bool SyncRustBridge::AddLockEdge(
	const std::string& fromMutex,
	const std::string& toMutex,
	const std::string& context) const
{
	return aura_sync_graph_add_lock_edge(
		m_impl->graph,
		fromMutex.c_str(),
		toMutex.c_str(),
		context.c_str());
}

bool SyncRustBridge::AddJoinEdge(
	const std::string& fromThread,
	const std::string& toThread,
	const std::string& context) const
{
	return aura_sync_graph_add_join_edge(
		m_impl->graph,
		fromThread.c_str(),
		toThread.c_str(),
		context.c_str());
}

std::string SyncRustBridge::LastError() const
{
	if (const char* error = aura_sync_graph_last_error(m_impl->graph))
	{
		return error;
	}
	return {};
}
