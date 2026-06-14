#pragma once

#include "MySqlModule.h"

#include "../../../core/values/Value.h"

#include <mysql/mysql.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace VM::Execution
{
class ExecutionContext;
}

namespace VM::Runtime::MySqlInternal
{

using Core::MySqlConnectionPtr;
using Core::MySqlPoolPtr;
using Core::MySqlResultPtr;
using Core::MySqlRowPtr;
using Core::MySqlStatementPtr;
using Core::Value;
using Execution::ExecutionContext;

struct ConnectionOptions
{
	std::string host = "127.0.0.1";
	uint16_t port = 3306;
	std::string user;
	std::string password;
	std::string database;
	std::string charset = "utf8mb4";
	unsigned int connectTimeoutSeconds = 5;
};

struct BorrowedConnection
{
	MYSQL* connection = nullptr;
	std::string errorTarget;
	std::function<void()> release;
};

struct StatementParameter
{
	MYSQL_BIND bind{};
	std::string stringValue;
	long long intValue = 0;
	double doubleValue = 0.0;
	unsigned long length = 0;
	bool isNull = false;
};

void SetLastError(const MySqlConnectionPtr& handle, const std::string& error);
void SetLastError(const MySqlPoolPtr& handle, const std::string& error);
void SetLastError(const MySqlStatementPtr& handle, const std::string& error);
void SetLastError(const MySqlResultPtr& handle, const std::string& error);

std::optional<ConnectionOptions> ParseDsn(const std::string& dsn, std::string& error);
MYSQL* OpenNativeConnection(const ConnectionOptions& options, std::string& error);
Value ConvertNativeCell(enum_field_types type, const char* data, unsigned long length);
MySqlResultPtr MaterializeResultSet(MYSQL* connection);
std::optional<std::vector<StatementParameter>> BuildStatementParameters(
	ExecutionContext& ctx,
	const std::vector<Value>& values);
MySqlResultPtr MaterializeStatementResult(MYSQL_STMT* statement);

MySqlConnectionPtr RequireMySqlConnection(ExecutionContext& ctx, const Value& value, const std::string& error);
MySqlPoolPtr RequireMySqlPool(ExecutionContext& ctx, const Value& value, const std::string& error);
MySqlStatementPtr RequireMySqlStatement(ExecutionContext& ctx, const Value& value, const std::string& error);
MySqlResultPtr RequireMySqlResult(ExecutionContext& ctx, const Value& value, const std::string& error);
MySqlRowPtr RequireMySqlRow(ExecutionContext& ctx, const Value& value, const std::string& error);

bool ReturnConnectionToPool(const MySqlConnectionPtr& connection);
bool CloseMySqlConnection(const MySqlConnectionPtr& connection);
bool CloseMySqlStatement(const MySqlStatementPtr& statement);
bool CloseMySqlPool(const MySqlPoolPtr& pool);

BorrowedConnection BorrowConnection(ExecutionContext& ctx, const Value& handle, const std::string& operation);
void StoreOperationError(const Value& handle, const std::string& error);
Value ExecuteTextQuery(
	ExecutionContext& ctx,
	const Value& handle,
	const std::string& sqlQuery,
	const std::string& operation);

void InstallConnectionBuiltins(SharedRuntime& runtime);
void InstallStatementBuiltins(SharedRuntime& runtime);
void InstallResultBuiltins(SharedRuntime& runtime);

} // namespace VM::Runtime::MySqlInternal
