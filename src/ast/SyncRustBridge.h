#pragma once

#include <memory>
#include <string>

class SyncRustBridge
{
public:
	SyncRustBridge();
	~SyncRustBridge();

	SyncRustBridge(const SyncRustBridge&) = delete;
	SyncRustBridge& operator=(const SyncRustBridge&) = delete;

	[[nodiscard]] bool AddLockEdge(const std::string& fromMutex, const std::string& toMutex, const std::string& context) const;
	[[nodiscard]] bool AddJoinEdge(const std::string& fromThread, const std::string& toThread, const std::string& context) const;
	[[nodiscard]] std::string LastError() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
