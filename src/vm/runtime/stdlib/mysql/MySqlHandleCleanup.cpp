#include "MySqlModuleInternal.h"

namespace VM::Core
{

MySqlPoolHandle::~MySqlPoolHandle()
{
	for (void* raw : allConnections)
	{
		if (raw != nullptr)
		{
			mysql_close(static_cast<MYSQL*>(raw));
		}
	}
}

MySqlConnectionHandle::~MySqlConnectionHandle()
{
	if (pooledLease)
	{
		if (owningPool && connection != nullptr)
		{
			std::lock_guard poolLock(owningPool->mutex);
			owningPool->availableConnections.push_back(connection);
		}
		connection = nullptr;
		owningPool.reset();
		pooledLease = false;
		return;
	}

	if (connection != nullptr)
	{
		mysql_close(static_cast<MYSQL*>(connection));
		connection = nullptr;
	}
}

MySqlStatementHandle::~MySqlStatementHandle()
{
	if (statement != nullptr)
	{
		mysql_stmt_close(static_cast<MYSQL_STMT*>(statement));
		statement = nullptr;
	}
	if (ownsConnectionLease && connection)
	{
		if (connection->pooledLease)
		{
			if (connection->owningPool && connection->connection != nullptr)
			{
				std::lock_guard poolLock(connection->owningPool->mutex);
				connection->owningPool->availableConnections.push_back(connection->connection);
			}
			connection->connection = nullptr;
			connection->owningPool.reset();
			connection->pooledLease = false;
		}
		else if (connection->connection != nullptr)
		{
			mysql_close(static_cast<MYSQL*>(connection->connection));
			connection->connection = nullptr;
		}
	}
	connection.reset();
	ownsConnectionLease = false;
}

} // namespace VM::Core
