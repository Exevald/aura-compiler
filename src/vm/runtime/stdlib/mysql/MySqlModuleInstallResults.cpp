#include "MySqlModuleInternal.h"

#include "../../../core/values/ValueHelper.h"
#include "../../NativeModuleSupport.h"

namespace VM::Runtime::MySqlInternal
{

void InstallResultBuiltins(SharedRuntime& runtime)
{
	using Core::Array;
	using Core::MySqlConnectionPtr;
	using Core::MySqlPoolPtr;
	using Core::MySqlResultPtr;
	using Core::MySqlStatementPtr;
	using Core::Value;

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".fetch_one", MakeNative("fetch_one", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto result = RequireMySqlResult(ctx, args[0], "std.mysql.fetch_one expects a result handle");
		if (!result || result->cursor >= result->rows.size()) return std::monostate{};
		return result->rows[result->cursor++];
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".fetch_all", MakeNative("fetch_all", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto result = RequireMySqlResult(ctx, args[0], "std.mysql.fetch_all expects a result handle");
		if (!result) return std::monostate{};
		auto array = std::make_shared<Array>();
		array->elements.reserve(result->rows.size());
		for (const auto& row : result->rows) array->elements.push_back(row);
		result->cursor = result->rows.size();
		return array;
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".next", MakeNative("next", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto result = RequireMySqlResult(ctx, args[0], "std.mysql.next expects a result handle");
		if (!result || result->cursor >= result->rows.size()) return std::monostate{};
		return result->rows[result->cursor++];
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".affected_rows", MakeNative("affected_rows", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto result = RequireMySqlResult(ctx, args[0], "std.mysql.affected_rows expects a result handle");
		return result ? Value(result->affectedRows) : Value(std::monostate{});
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".last_insert_id", MakeNative("last_insert_id", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto result = RequireMySqlResult(ctx, args[0], "std.mysql.last_insert_id expects a result handle");
		return result ? Value(result->lastInsertId) : Value(std::monostate{});
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".column_count", MakeNative("column_count", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto result = RequireMySqlResult(ctx, args[0], "std.mysql.column_count expects a result handle");
		return result ? Value(static_cast<int64_t>(result->columnNames.size())) : Value(std::monostate{});
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".column_name", MakeNative("column_name", 2, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto result = RequireMySqlResult(ctx, args[0], "std.mysql.column_name expects a result handle");
		if (!result) return std::monostate{};
		const int64_t index = Core::ValueHelper::As<int64_t>(args[1]);
		if (index < 0 || static_cast<size_t>(index) >= result->columnNames.size()) { ctx.RaiseError("std.mysql.column_name index is out of range"); return std::monostate{}; }
		return std::make_shared<const std::string>(result->columnNames[static_cast<size_t>(index)]);
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".get", MakeNative("get", 2, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto row = RequireMySqlRow(ctx, args[0], "std.mysql.get expects a row handle");
		if (!row) return std::monostate{};
		const auto column = RequireString(ctx, args[1], "std.mysql.get expects a string column name");
		if (!column) return std::monostate{};
		const auto it = row->nameToIndex->find(*column);
		if (it == row->nameToIndex->end()) { ctx.RaiseError("std.mysql.get could not find the requested column"); return std::monostate{}; }
		return row->values[it->second];
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".get_at", MakeNative("get_at", 2, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		const auto row = RequireMySqlRow(ctx, args[0], "std.mysql.get_at expects a row handle");
		if (!row) return std::monostate{};
		const int64_t index = Core::ValueHelper::As<int64_t>(args[1]);
		if (index < 0 || static_cast<size_t>(index) >= row->values.size()) { ctx.RaiseError("std.mysql.get_at index is out of range"); return std::monostate{}; }
		return row->values[static_cast<size_t>(index)];
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".is_null", MakeNative("is_null", 1, [](ExecutionContext&, const std::vector<Value>& args) -> Value {
		return std::holds_alternative<std::monostate>(args[0]);
	}));

	runtime.DefineGlobal(std::string(MySqlModule::ModuleName()) + ".error", MakeNative("error", 1, [](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
		if (std::holds_alternative<MySqlConnectionPtr>(args[0]))
		{
			const auto handle = std::get<MySqlConnectionPtr>(args[0]);
			if (!handle) return std::make_shared<const std::string>("");
			if (handle->connection != nullptr)
			{
				const char* native = mysql_error(static_cast<MYSQL*>(handle->connection));
				if (native != nullptr && *native != '\0') return std::make_shared<const std::string>(native);
			}
			return std::make_shared<const std::string>(handle->lastError);
		}
		if (std::holds_alternative<MySqlPoolPtr>(args[0]))
		{
			const auto handle = std::get<MySqlPoolPtr>(args[0]);
			return std::make_shared<const std::string>(handle ? handle->lastError : "");
		}
		if (std::holds_alternative<MySqlStatementPtr>(args[0]))
		{
			const auto handle = std::get<MySqlStatementPtr>(args[0]);
			if (!handle) return std::make_shared<const std::string>("");
			if (handle->statement != nullptr)
			{
				const char* native = mysql_stmt_error(static_cast<MYSQL_STMT*>(handle->statement));
				if (native != nullptr && *native != '\0') return std::make_shared<const std::string>(native);
			}
			return std::make_shared<const std::string>(handle->lastError);
		}
		if (std::holds_alternative<MySqlResultPtr>(args[0]))
		{
			const auto handle = std::get<MySqlResultPtr>(args[0]);
			return std::make_shared<const std::string>(handle ? handle->lastError : "");
		}
		ctx.RaiseError("std.mysql.error expects a MySQL handle");
		return std::monostate{};
	}));
}

} // namespace VM::Runtime::MySqlInternal
