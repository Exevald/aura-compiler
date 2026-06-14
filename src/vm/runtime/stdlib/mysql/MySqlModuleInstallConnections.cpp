#include "MySqlModuleInternal.h"

#include "../../../core/values/ValueHelper.h"
#include "../../NativeModuleSupport.h"

namespace VM::Runtime::MySqlInternal
{

void InstallConnectionBuiltins(SharedRuntime& runtime)
{
	using Core::MySqlConnectionHandle;
	using Core::MySqlConnectionPtr;
	using Core::MySqlPoolHandle;
	using Core::MySqlPoolPtr;
	using Core::MySqlResultPtr;
	using Core::MySqlStatementPtr;
	using Core::Value;

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".open", MakeNative("open", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto dsn = RequireString(ctx, args[0], "std.mysql.open expects a DSN string");
		if (!dsn) return std::monostate{};
		std::string parseError;
		auto options = ParseDsn(*dsn, parseError);
		if (!options) { ctx.RaiseError(parseError); return std::monostate{}; }
		std::string connectError;
		MYSQL* native = OpenNativeConnection(*options, connectError);
		if (native == nullptr) { ctx.RaiseError("std.mysql.open failed: " + connectError); return std::monostate{}; }
		auto connection = std::make_shared<MySqlConnectionHandle>();
		connection->connection = native;
		return connection;
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".open_pool", MakeNative("open_pool", 2, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto dsn = RequireString(ctx, args[0], "std.mysql.open_pool expects a DSN string");
		if (!dsn) return std::monostate{};
		const int64_t poolSize = Core::ValueHelper::As<int64_t>(args[1]);
		if (poolSize <= 0 || poolSize > 128) { ctx.RaiseError("std.mysql.open_pool expects size in range 1..128"); return std::monostate{}; }
		std::string parseError;
		auto options = ParseDsn(*dsn, parseError);
		if (!options) { ctx.RaiseError(parseError); return std::monostate{}; }
		auto pool = std::make_shared<MySqlPoolHandle>();
		pool->availableConnections.reserve(static_cast<size_t>(poolSize));
		pool->allConnections.reserve(static_cast<size_t>(poolSize));
		for (int64_t index = 0; index < poolSize; ++index)
		{
			std::string connectError;
			MYSQL* native = OpenNativeConnection(*options, connectError);
			if (native == nullptr) { pool->lastError = connectError; ctx.RaiseError("std.mysql.open_pool failed: " + connectError); return std::monostate{}; }
			pool->availableConnections.push_back(native);
			pool->allConnections.push_back(native);
		}
		return pool;
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".close", MakeNative("close", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		if (std::holds_alternative<MySqlConnectionPtr>(args[0])) return CloseMySqlConnection(std::get<MySqlConnectionPtr>(args[0]));
		if (std::holds_alternative<MySqlPoolPtr>(args[0]))
		{
			const auto pool = std::get<MySqlPoolPtr>(args[0]);
			const bool closed = CloseMySqlPool(pool);
			if (!closed && pool && !pool->lastError.empty()) { ctx.RaiseError(pool->lastError); return std::monostate{}; }
			return closed;
		}
		if (std::holds_alternative<MySqlStatementPtr>(args[0])) return CloseMySqlStatement(std::get<MySqlStatementPtr>(args[0]));
		if (std::holds_alternative<MySqlResultPtr>(args[0]))
		{
			const auto result = std::get<MySqlResultPtr>(args[0]);
			if (!result) return false;
			result->rows.clear();
			result->columnNames.clear();
			result->cursor = 0;
			return true;
		}
		ctx.RaiseError("std.mysql.close expects a MySQL connection, pool, statement, or result handle");
		return std::monostate{};
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".ping", MakeNative("ping", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		auto borrowed = BorrowConnection(ctx, args[0], "ping");
		if (borrowed.connection == nullptr) return std::monostate{};
		const int rc = mysql_ping(borrowed.connection);
		if (rc != 0)
		{
			const std::string error = mysql_error(borrowed.connection);
			StoreOperationError(args[0], error);
			borrowed.release();
			return false;
		}
		borrowed.release();
		return true;
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".begin", MakeNative("begin", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto connection = RequireMySqlConnection(ctx, args[0], "std.mysql.begin expects a single connection handle");
		if (!connection) return std::monostate{};
		std::lock_guard<std::mutex> lock(connection->mutex);
		if (connection->connection == nullptr) { ctx.RaiseError("std.mysql.begin cannot use a closed connection"); return std::monostate{}; }
		if (mysql_autocommit(static_cast<MYSQL*>(connection->connection), 0) != 0)
		{
			const std::string error = mysql_error(static_cast<MYSQL*>(connection->connection));
			SetLastError(connection, error);
			ctx.RaiseError("std.mysql.begin failed: " + error);
			return std::monostate{};
		}
		return true;
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".commit", MakeNative("commit", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto connection = RequireMySqlConnection(ctx, args[0], "std.mysql.commit expects a single connection handle");
		if (!connection) return std::monostate{};
		std::lock_guard<std::mutex> lock(connection->mutex);
		if (connection->connection == nullptr) { ctx.RaiseError("std.mysql.commit cannot use a closed connection"); return std::monostate{}; }
		if (mysql_commit(static_cast<MYSQL*>(connection->connection)) != 0 || mysql_autocommit(static_cast<MYSQL*>(connection->connection), 1) != 0)
		{
			const std::string error = mysql_error(static_cast<MYSQL*>(connection->connection));
			SetLastError(connection, error);
			ctx.RaiseError("std.mysql.commit failed: " + error);
			return std::monostate{};
		}
		return true;
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".rollback", MakeNative("rollback", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto connection = RequireMySqlConnection(ctx, args[0], "std.mysql.rollback expects a single connection handle");
		if (!connection) return std::monostate{};
		std::lock_guard<std::mutex> lock(connection->mutex);
		if (connection->connection == nullptr) { ctx.RaiseError("std.mysql.rollback cannot use a closed connection"); return std::monostate{}; }
		if (mysql_rollback(static_cast<MYSQL*>(connection->connection)) != 0 || mysql_autocommit(static_cast<MYSQL*>(connection->connection), 1) != 0)
		{
			const std::string error = mysql_error(static_cast<MYSQL*>(connection->connection));
			SetLastError(connection, error);
			ctx.RaiseError("std.mysql.rollback failed: " + error);
			return std::monostate{};
		}
		return true;
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".with_tx", MakeNative("with_tx", 2, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto connection = RequireMySqlConnection(ctx, args[0], "std.mysql.with_tx expects a single connection handle");
		if (!connection) return std::monostate{};

		{
			std::lock_guard<std::mutex> lock(connection->mutex);
			if (connection->connection == nullptr)
			{
				ctx.RaiseError("std.mysql.with_tx cannot use a closed connection");
				return std::monostate{};
			}
			if (mysql_autocommit(static_cast<MYSQL*>(connection->connection), 0) != 0)
			{
				const std::string error = mysql_error(static_cast<MYSQL*>(connection->connection));
				SetLastError(connection, error);
				ctx.RaiseError("std.mysql.with_tx failed to begin: " + error);
				return std::monostate{};
			}
		}

		Value callbackResult;
		if (const auto invokeError = ctx.InvokeCallable(args[1], { args[0] }, callbackResult))
		{
			std::lock_guard<std::mutex> lock(connection->mutex);
			if (connection->connection != nullptr)
			{
				(void)mysql_rollback(static_cast<MYSQL*>(connection->connection));
				(void)mysql_autocommit(static_cast<MYSQL*>(connection->connection), 1);
			}
			SetLastError(connection, *invokeError);
			ctx.RaiseError(*invokeError);
			return std::monostate{};
		}

		{
			std::lock_guard<std::mutex> lock(connection->mutex);
			if (connection->connection == nullptr)
			{
				ctx.RaiseError("std.mysql.with_tx cannot commit a closed connection");
				return std::monostate{};
			}
			if (mysql_commit(static_cast<MYSQL*>(connection->connection)) != 0
				|| mysql_autocommit(static_cast<MYSQL*>(connection->connection), 1) != 0)
			{
				const std::string error = mysql_error(static_cast<MYSQL*>(connection->connection));
				(void)mysql_rollback(static_cast<MYSQL*>(connection->connection));
				(void)mysql_autocommit(static_cast<MYSQL*>(connection->connection), 1);
				SetLastError(connection, error);
				ctx.RaiseError("std.mysql.with_tx failed to commit: " + error);
				return std::monostate{};
			}
		}

		return callbackResult;
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".exec", MakeNative("exec", 2, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto sql = RequireString(ctx, args[1], "std.mysql.exec expects a SQL string");
		if (!sql) return std::monostate{};
		return ExecuteTextQuery(ctx, args[0], *sql, "exec");
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".query", MakeNative("query", 2, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto sql = RequireString(ctx, args[1], "std.mysql.query expects a SQL string");
		if (!sql) return std::monostate{};
		return ExecuteTextQuery(ctx, args[0], *sql, "query");
	}));
}

} // namespace VM::Runtime::MySqlInternal
