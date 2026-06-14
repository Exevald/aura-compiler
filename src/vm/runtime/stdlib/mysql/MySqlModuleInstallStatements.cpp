#include "MySqlModuleInternal.h"

#include "../../NativeModuleSupport.h"

namespace VM::Runtime::MySqlInternal
{

void InstallStatementBuiltins(SharedRuntime& runtime)
{
	using Core::MySqlConnectionHandle;
	using Core::MySqlPoolPtr;
	using Core::MySqlStatementHandle;
	using Core::Value;

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".prepare", MakeNative("prepare", 2, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto sql = RequireString(ctx, args[1], "std.mysql.prepare expects a SQL string");
		if (!sql) return std::monostate{};

		Core::MySqlConnectionPtr lease;
		MYSQL* nativeConnection = nullptr;
		if (std::holds_alternative<Core::MySqlConnectionPtr>(args[0]))
		{
			lease = RequireMySqlConnection(ctx, args[0], "std.mysql.prepare expects a connection or pool handle");
			if (!lease) return std::monostate{};
			if (lease->connection == nullptr) { ctx.RaiseError("std.mysql.prepare cannot use a closed connection"); return std::monostate{}; }
			nativeConnection = static_cast<MYSQL*>(lease->connection);
		}
		else if (std::holds_alternative<MySqlPoolPtr>(args[0]))
		{
			const auto pool = RequireMySqlPool(ctx, args[0], "std.mysql.prepare expects a connection or pool handle");
			if (!pool) return std::monostate{};
			std::lock_guard lock(pool->mutex);
			if (pool->availableConnections.empty()) { pool->lastError = "std.mysql.prepare could not borrow a connection from the pool"; ctx.RaiseError(pool->lastError); return std::monostate{}; }
			lease = std::make_shared<MySqlConnectionHandle>();
			lease->connection = pool->availableConnections.back();
			lease->owningPool = pool;
			lease->pooledLease = true;
			pool->availableConnections.pop_back();
			nativeConnection = static_cast<MYSQL*>(lease->connection);
		}
		else
		{
			ctx.RaiseError("std.mysql.prepare expects a connection or pool handle");
			return std::monostate{};
		}

		MYSQL_STMT* nativeStatement = mysql_stmt_init(nativeConnection);
		if (nativeStatement == nullptr) { const std::string error = mysql_error(nativeConnection); SetLastError(lease, error); ctx.RaiseError("std.mysql.prepare failed: " + error); return std::monostate{}; }
		const bool updateMaxLength = true;
		mysql_stmt_attr_set(nativeStatement, STMT_ATTR_UPDATE_MAX_LENGTH, &updateMaxLength);
		if (mysql_stmt_prepare(nativeStatement, sql->data(), sql->size()) != 0)
		{
			const std::string error = mysql_stmt_error(nativeStatement);
			SetLastError(lease, error);
			mysql_stmt_close(nativeStatement);
			ctx.RaiseError("std.mysql.prepare failed: " + error);
			return std::monostate{};
		}

		auto statement = std::make_shared<MySqlStatementHandle>();
		statement->statement = nativeStatement;
		statement->connection = lease;
		statement->ownsConnectionLease = lease->pooledLease;
		return statement;
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".bind", MakeNative("bind", 2, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto statement = RequireMySqlStatement(ctx, args[0], "std.mysql.bind expects a prepared statement");
		if (!statement) return std::monostate{};
		const auto params = RequireArray(ctx, args[1], "std.mysql.bind expects an array of parameters");
		if (!params) return std::monostate{};
		statement->boundValues = params->elements;
		return statement;
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".execute", MakeNative("execute", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto statement = RequireMySqlStatement(ctx, args[0], "std.mysql.execute expects a prepared statement");
		if (!statement) return std::monostate{};
		if (statement->statement == nullptr || !statement->connection || statement->connection->connection == nullptr)
		{
			ctx.RaiseError("std.mysql.execute cannot use a closed prepared statement");
			return std::monostate{};
		}

		const auto params = BuildStatementParameters(ctx, statement->boundValues);
		if (!params) return std::monostate{};

		MYSQL_STMT* nativeStatement = static_cast<MYSQL_STMT*>(statement->statement);
		if (mysql_stmt_reset(nativeStatement) != 0) { const std::string error = mysql_stmt_error(nativeStatement); SetLastError(statement, error); ctx.RaiseError("std.mysql.execute failed: " + error); return std::monostate{}; }
		const unsigned long expected = mysql_stmt_param_count(nativeStatement);
		if (expected != params->size()) { ctx.RaiseError("std.mysql.execute expected " + std::to_string(expected) + " bound parameters but received " + std::to_string(params->size())); return std::monostate{}; }

		std::vector<MYSQL_BIND> binds(params->size());
		for (size_t index = 0; index < params->size(); ++index) binds[index] = (*params)[index].bind;
		if (!binds.empty() && mysql_stmt_bind_param(nativeStatement, binds.data()) != 0) { const std::string error = mysql_stmt_error(nativeStatement); SetLastError(statement, error); ctx.RaiseError("std.mysql.execute failed: " + error); return std::monostate{}; }
		if (mysql_stmt_execute(nativeStatement) != 0) { const std::string error = mysql_stmt_error(nativeStatement); SetLastError(statement, error); ctx.RaiseError("std.mysql.execute failed: " + error); return std::monostate{}; }
		return MaterializeStatementResult(nativeStatement);
	}));
}

} // namespace VM::Runtime::MySqlInternal
