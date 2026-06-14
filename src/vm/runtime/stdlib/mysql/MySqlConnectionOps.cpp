#include "MySqlModuleInternal.h"

#include "../../ExecutionContext.h"

namespace VM::Runtime::MySqlInternal
{

MySqlConnectionPtr RequireMySqlConnection(ExecutionContext& ctx, const Value& value, const std::string& error)
{
	if (!std::holds_alternative<MySqlConnectionPtr>(value) || !std::get<MySqlConnectionPtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<MySqlConnectionPtr>(value);
}

MySqlPoolPtr RequireMySqlPool(ExecutionContext& ctx, const Value& value, const std::string& error)
{
	if (!std::holds_alternative<MySqlPoolPtr>(value) || !std::get<MySqlPoolPtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<MySqlPoolPtr>(value);
}

MySqlStatementPtr RequireMySqlStatement(ExecutionContext& ctx, const Value& value, const std::string& error)
{
	if (!std::holds_alternative<MySqlStatementPtr>(value) || !std::get<MySqlStatementPtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<MySqlStatementPtr>(value);
}

MySqlResultPtr RequireMySqlResult(ExecutionContext& ctx, const Value& value, const std::string& error)
{
	if (!std::holds_alternative<MySqlResultPtr>(value) || !std::get<MySqlResultPtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<MySqlResultPtr>(value);
}

MySqlRowPtr RequireMySqlRow(ExecutionContext& ctx, const Value& value, const std::string& error)
{
	if (!std::holds_alternative<MySqlRowPtr>(value) || !std::get<MySqlRowPtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<MySqlRowPtr>(value);
}

bool ReturnConnectionToPool(const MySqlConnectionPtr& connection)
{
	if (!connection || !connection->pooledLease || !connection->owningPool || connection->connection == nullptr)
	{
		return false;
	}

	auto pool = connection->owningPool;
	{
		std::lock_guard<std::mutex> poolLock(pool->mutex);
		pool->availableConnections.push_back(connection->connection);
	}
	connection->connection = nullptr;
	connection->owningPool.reset();
	connection->pooledLease = false;
	connection->lastError.clear();
	return true;
}

bool CloseMySqlConnection(const MySqlConnectionPtr& connection)
{
	if (!connection)
	{
		return false;
	}

	std::lock_guard<std::mutex> lock(connection->mutex);
	if (connection->connection == nullptr)
	{
		return false;
	}

	if (connection->pooledLease)
	{
		return ReturnConnectionToPool(connection);
	}

	mysql_close(static_cast<MYSQL*>(connection->connection));
	connection->connection = nullptr;
	connection->lastError.clear();
	return true;
}

bool CloseMySqlStatement(const MySqlStatementPtr& statement)
{
	if (!statement)
	{
		return false;
	}

	if (statement->statement != nullptr)
	{
		mysql_stmt_close(static_cast<MYSQL_STMT*>(statement->statement));
		statement->statement = nullptr;
	}

	statement->boundValues.clear();
	if (statement->ownsConnectionLease && statement->connection)
	{
		CloseMySqlConnection(statement->connection);
	}
	statement->connection.reset();
	statement->ownsConnectionLease = false;
	return true;
}

bool CloseMySqlPool(const MySqlPoolPtr& pool)
{
	if (!pool)
	{
		return false;
	}

	std::lock_guard<std::mutex> lock(pool->mutex);
	if (pool->availableConnections.size() != pool->allConnections.size())
	{
		pool->lastError = "std.mysql.close cannot close a pool while prepared statements or leases are active";
		return false;
	}

	for (void* raw : pool->allConnections)
	{
		if (raw != nullptr)
		{
			mysql_close(static_cast<MYSQL*>(raw));
		}
	}
	pool->availableConnections.clear();
	pool->allConnections.clear();
	pool->lastError.clear();
	return true;
}

BorrowedConnection BorrowConnection(ExecutionContext& ctx, const Value& handle, const std::string& operation)
{
	if (std::holds_alternative<MySqlConnectionPtr>(handle))
	{
		const auto connectionHandle = RequireMySqlConnection(
			ctx, handle, "std.mysql." + operation + " expects a connection, pool, or prepared statement handle");
		if (!connectionHandle)
		{
			return {};
		}

		auto lock = std::make_shared<std::unique_lock<std::mutex>>(connectionHandle->mutex);
		if (connectionHandle->connection == nullptr)
		{
			ctx.RaiseError("std.mysql." + operation + " cannot use a closed connection");
			return {};
		}

		BorrowedConnection borrowed;
		borrowed.connection = static_cast<MYSQL*>(connectionHandle->connection);
		borrowed.errorTarget = "connection";
		borrowed.release = [lock]() mutable { lock->unlock(); };
		return borrowed;
	}

	if (std::holds_alternative<MySqlPoolPtr>(handle))
	{
		const auto pool = RequireMySqlPool(
			ctx, handle, "std.mysql." + operation + " expects a connection, pool, or prepared statement handle");
		if (!pool)
		{
			return {};
		}

		std::lock_guard<std::mutex> lock(pool->mutex);
		if (pool->availableConnections.empty())
		{
			pool->lastError = "std.mysql." + operation + " could not borrow a connection from the pool";
			ctx.RaiseError(pool->lastError);
			return {};
		}

		void* raw = pool->availableConnections.back();
		pool->availableConnections.pop_back();

		BorrowedConnection borrowed;
		borrowed.connection = static_cast<MYSQL*>(raw);
		borrowed.errorTarget = "pool";
		borrowed.release = [pool, raw]() {
			std::lock_guard<std::mutex> lock(pool->mutex);
			pool->availableConnections.push_back(raw);
		};
		return borrowed;
	}

	ctx.RaiseError("std.mysql." + operation + " expects a connection or pool handle");
	return {};
}

void StoreOperationError(const Value& handle, const std::string& error)
{
	if (std::holds_alternative<MySqlConnectionPtr>(handle))
	{
		SetLastError(std::get<MySqlConnectionPtr>(handle), error);
	}
	else if (std::holds_alternative<MySqlPoolPtr>(handle))
	{
		SetLastError(std::get<MySqlPoolPtr>(handle), error);
	}
}

Value ExecuteTextQuery(
	ExecutionContext& ctx,
	const Value& handle,
	const std::string& sqlQuery,
	const std::string& operation)
{
	auto borrowed = BorrowConnection(ctx, handle, operation);
	if (borrowed.connection == nullptr)
	{
		return std::monostate{};
	}

	if (mysql_real_query(borrowed.connection, sqlQuery.data(), sqlQuery.size()) != 0)
	{
		const std::string error = mysql_error(borrowed.connection);
		StoreOperationError(handle, error);
		borrowed.release();
		ctx.RaiseError("std.mysql." + operation + " failed: " + error);
		return std::monostate{};
	}

	auto result = MaterializeResultSet(borrowed.connection);
	borrowed.release();
	return result;
}

} // namespace VM::Runtime::MySqlInternal
