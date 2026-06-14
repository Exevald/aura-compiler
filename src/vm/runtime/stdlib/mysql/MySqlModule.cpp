#include "MySqlModuleInternal.h"

#include "../../ExecutionContext.h"

#include <algorithm>
#include <charconv>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace VM::Runtime
{

using Core::MySqlConnectionPtr;
using Core::MySqlPoolPtr;
using Core::MySqlResultHandle;
using Core::MySqlResultPtr;
using Core::MySqlRowHandle;
using Core::MySqlRowPtr;
using Core::MySqlStatementPtr;
using Core::Value;
using Execution::ExecutionContext;

namespace
{

std::string UrlDecode(const std::string_view text)
{
	std::string result;
	result.reserve(text.size());
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] == '%' && i + 2 < text.size())
		{
			unsigned int value = 0;
			const auto hex = text.substr(i + 1, 2);
			if (std::from_chars(hex.data(), hex.data() + hex.size(), value, 16).ec == std::errc{})
			{
				result.push_back(static_cast<char>(value));
				i += 2;
				continue;
			}
		}
		result.push_back(text[i] == '+' ? ' ' : text[i]);
	}
	return result;
}

bool ParseUnsigned(const std::string_view text, uint16_t& out)
{
	unsigned int value = 0;
	const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (ec != std::errc{} || ptr != text.data() + text.size() || value > 65535)
	{
		return false;
	}
	out = static_cast<uint16_t>(value);
	return true;
}

bool ParseUnsigned(const std::string_view text, unsigned int& out)
{
	unsigned int value = 0;
	const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (ec != std::errc{} || ptr != text.data() + text.size())
	{
		return false;
	}
	out = value;
	return true;
}

std::optional<MySqlInternal::ConnectionOptions> ParseKeyValueDsn(const std::string_view dsn, std::string& error)
{
	MySqlInternal::ConnectionOptions options;
	size_t start = 0;
	while (start < dsn.size())
	{
		const size_t end = dsn.find(';', start);
		const auto token = dsn.substr(start, end == std::string_view::npos ? dsn.size() - start : end - start);
		start = end == std::string_view::npos ? dsn.size() : end + 1;
		if (token.empty()) continue;
		const size_t equals = token.find('=');
		if (equals == std::string_view::npos) { error = "std.mysql expected DSN entries in key=value form"; return std::nullopt; }
		const auto key = token.substr(0, equals);
		const auto value = token.substr(equals + 1);
		if (key == "host") options.host = std::string(value);
		else if (key == "port")
		{
			if (!ParseUnsigned(value, options.port)) { error = "std.mysql DSN port must be in range 0..65535"; return std::nullopt; }
		}
		else if (key == "user") options.user = std::string(value);
		else if (key == "password" || key == "pass") options.password = std::string(value);
		else if (key == "database" || key == "db") options.database = std::string(value);
		else if (key == "charset") options.charset = std::string(value);
		else if (key == "connect_timeout")
		{
			if (!ParseUnsigned(value, options.connectTimeoutSeconds)) { error = "std.mysql DSN connect_timeout must be a non-negative integer"; return std::nullopt; }
		}
		else { error = "std.mysql DSN contains unsupported key: " + std::string(key); return std::nullopt; }
	}
	if (options.user.empty()) { error = "std.mysql DSN requires a user"; return std::nullopt; }
	if (options.database.empty()) { error = "std.mysql DSN requires a database"; return std::nullopt; }
	return options;
}

std::optional<MySqlInternal::ConnectionOptions> ParseUrlDsn(const std::string_view dsn, std::string& error)
{
	MySqlInternal::ConnectionOptions options;
	std::string_view rest = dsn.substr(std::string_view("mysql://").size());
	const size_t at = rest.find('@');
	if (at == std::string_view::npos) { error = "std.mysql URL DSN requires mysql://user[:password]@host[:port]/database"; return std::nullopt; }
	const auto credentials = rest.substr(0, at);
	rest = rest.substr(at + 1);
	const size_t colon = credentials.find(':');
	options.user = UrlDecode(credentials.substr(0, colon));
	if (colon != std::string_view::npos) options.password = UrlDecode(credentials.substr(colon + 1));
	const size_t slash = rest.find('/');
	if (slash == std::string_view::npos) { error = "std.mysql URL DSN requires a database name"; return std::nullopt; }
	const auto hostPort = rest.substr(0, slash);
	rest = rest.substr(slash + 1);
	const size_t hostColon = hostPort.rfind(':');
	if (hostColon != std::string_view::npos)
	{
		options.host = std::string(hostPort.substr(0, hostColon));
		if (!ParseUnsigned(hostPort.substr(hostColon + 1), options.port)) { error = "std.mysql URL DSN port must be in range 0..65535"; return std::nullopt; }
	}
	else
	{
		options.host = std::string(hostPort);
	}
	const size_t query = rest.find('?');
	options.database = UrlDecode(rest.substr(0, query));
	if (options.database.empty()) { error = "std.mysql URL DSN requires a database name"; return std::nullopt; }
	if (query != std::string_view::npos)
	{
		std::string_view params = rest.substr(query + 1);
		while (!params.empty())
		{
			const size_t amp = params.find('&');
			const auto token = params.substr(0, amp);
			params = amp == std::string_view::npos ? std::string_view{} : params.substr(amp + 1);
			const size_t equals = token.find('=');
			if (equals == std::string_view::npos) continue;
			const auto key = token.substr(0, equals);
			const auto value = UrlDecode(token.substr(equals + 1));
			if (key == "charset") options.charset = value;
			else if (key == "connect_timeout")
			{
				unsigned int timeout = 0;
				if (!ParseUnsigned(value, timeout)) { error = "std.mysql URL DSN connect_timeout must be a non-negative integer"; return std::nullopt; }
				options.connectTimeoutSeconds = timeout;
			}
		}
	}
	if (options.user.empty()) { error = "std.mysql URL DSN requires a user"; return std::nullopt; }
	return options;
}

MySqlRowPtr BuildRow(
	const std::shared_ptr<std::vector<std::string>>& columnNames,
	const std::shared_ptr<std::unordered_map<std::string, size_t>>& nameToIndex,
	std::vector<Value> values)
{
	auto row = std::make_shared<MySqlRowHandle>();
	row->columnNames = columnNames;
	row->nameToIndex = nameToIndex;
	row->values = std::move(values);
	return row;
}

} // namespace

void MySqlInternal::SetLastError(const MySqlConnectionPtr& handle, const std::string& error) { if (handle) handle->lastError = error; }
void MySqlInternal::SetLastError(const MySqlPoolPtr& handle, const std::string& error) { if (handle) handle->lastError = error; }
void MySqlInternal::SetLastError(const MySqlStatementPtr& handle, const std::string& error) { if (handle) handle->lastError = error; }
void MySqlInternal::SetLastError(const MySqlResultPtr& handle, const std::string& error) { if (handle) handle->lastError = error; }

std::optional<MySqlInternal::ConnectionOptions> MySqlInternal::ParseDsn(const std::string& dsn, std::string& error)
{
	return dsn.starts_with("mysql://") ? ParseUrlDsn(dsn, error) : ParseKeyValueDsn(dsn, error);
}

MYSQL* MySqlInternal::OpenNativeConnection(const ConnectionOptions& options, std::string& error)
{
	MYSQL* connection = mysql_init(nullptr);
	if (connection == nullptr) { error = "std.mysql failed to initialize the MySQL client"; return nullptr; }
	mysql_options(connection, MYSQL_SET_CHARSET_NAME, options.charset.c_str());
	mysql_options(connection, MYSQL_OPT_CONNECT_TIMEOUT, &options.connectTimeoutSeconds);
	if (mysql_real_connect(connection, options.host.c_str(), options.user.c_str(), options.password.empty() ? nullptr : options.password.c_str(), options.database.c_str(), options.port, nullptr, 0) == nullptr)
	{
		error = mysql_error(connection);
		mysql_close(connection);
		return nullptr;
	}
	return connection;
}

Value MySqlInternal::ConvertNativeCell(const enum_field_types type, const char* data, const unsigned long length)
{
	if (data == nullptr) return std::monostate{};
	const std::string text(data, length);
	switch (type)
	{
	case MYSQL_TYPE_TINY:
	case MYSQL_TYPE_SHORT:
	case MYSQL_TYPE_LONG:
	case MYSQL_TYPE_INT24:
	case MYSQL_TYPE_LONGLONG:
	case MYSQL_TYPE_YEAR:
		try { return static_cast<int64_t>(std::stoll(text)); } catch (const std::exception&) { return std::make_shared<const std::string>(text); }
	case MYSQL_TYPE_FLOAT:
	case MYSQL_TYPE_DOUBLE:
		try { return std::stod(text); } catch (const std::exception&) { return std::make_shared<const std::string>(text); }
	case MYSQL_TYPE_NULL:
		return std::monostate{};
	default:
		return std::make_shared<const std::string>(text);
	}
}

MySqlResultPtr MySqlInternal::MaterializeResultSet(MYSQL* connection)
{
	auto result = std::make_shared<MySqlResultHandle>();
	result->affectedRows = static_cast<int64_t>(mysql_affected_rows(connection));
	result->lastInsertId = static_cast<int64_t>(mysql_insert_id(connection));
	MYSQL_RES* nativeResult = mysql_store_result(connection);
	if (nativeResult == nullptr) return result;
	const unsigned int count = mysql_num_fields(nativeResult);
	MYSQL_FIELD* fields = mysql_fetch_fields(nativeResult);
	result->columnNames.reserve(count);
	auto columnNames = std::make_shared<std::vector<std::string>>();
	auto nameToIndex = std::make_shared<std::unordered_map<std::string, size_t>>();
	columnNames->reserve(count);
	nameToIndex->reserve(count);
	for (unsigned int index = 0; index < count; ++index)
	{
		const std::string name = fields[index].name ? fields[index].name : "";
		result->columnNames.push_back(name);
		columnNames->push_back(name);
		(*nameToIndex)[name] = index;
	}
	while (MYSQL_ROW row = mysql_fetch_row(nativeResult))
	{
		const unsigned long* lengths = mysql_fetch_lengths(nativeResult);
		std::vector<Value> values;
		values.reserve(count);
		for (unsigned int index = 0; index < count; ++index) values.push_back(ConvertNativeCell(fields[index].type, row[index], lengths[index]));
		result->rows.push_back(BuildRow(columnNames, nameToIndex, std::move(values)));
	}
	mysql_free_result(nativeResult);
	return result;
}

std::optional<std::vector<MySqlInternal::StatementParameter>> MySqlInternal::BuildStatementParameters(ExecutionContext& ctx, const std::vector<Value>& values)
{
	std::vector<StatementParameter> parameters(values.size());
	for (size_t index = 0; index < values.size(); ++index)
	{
		auto& storage = parameters[index];
		storage.bind.is_null = &storage.isNull;
		storage.bind.length = &storage.length;
		if (std::holds_alternative<std::monostate>(values[index])) { storage.isNull = true; storage.bind.buffer_type = MYSQL_TYPE_NULL; continue; }
		if (std::holds_alternative<bool>(values[index])) { storage.intValue = std::get<bool>(values[index]) ? 1 : 0; storage.bind.buffer_type = MYSQL_TYPE_LONGLONG; storage.bind.buffer = &storage.intValue; storage.bind.is_unsigned = false; continue; }
		if (std::holds_alternative<int64_t>(values[index])) { storage.intValue = std::get<int64_t>(values[index]); storage.bind.buffer_type = MYSQL_TYPE_LONGLONG; storage.bind.buffer = &storage.intValue; storage.bind.is_unsigned = false; continue; }
		if (std::holds_alternative<double>(values[index])) { storage.doubleValue = std::get<double>(values[index]); storage.bind.buffer_type = MYSQL_TYPE_DOUBLE; storage.bind.buffer = &storage.doubleValue; continue; }
		if (std::holds_alternative<Core::StringPtr>(values[index]) && std::get<Core::StringPtr>(values[index]))
		{
			storage.stringValue = *std::get<Core::StringPtr>(values[index]);
			storage.length = static_cast<unsigned long>(storage.stringValue.size());
			storage.bind.buffer_type = MYSQL_TYPE_STRING;
			storage.bind.buffer = storage.stringValue.data();
			storage.bind.buffer_length = storage.length;
			continue;
		}
		ctx.RaiseError("std.mysql.bind supports only null, bool, int, float, and string parameters");
		return std::nullopt;
	}
	return parameters;
}

MySqlResultPtr MySqlInternal::MaterializeStatementResult(MYSQL_STMT* statement)
{
	auto result = std::make_shared<MySqlResultHandle>();
	result->affectedRows = static_cast<int64_t>(mysql_stmt_affected_rows(statement));
	result->lastInsertId = static_cast<int64_t>(mysql_stmt_insert_id(statement));
	MYSQL_RES* meta = mysql_stmt_result_metadata(statement);
	if (meta == nullptr) return result;
	mysql_stmt_store_result(statement);
	const unsigned int count = mysql_num_fields(meta);
	MYSQL_FIELD* fields = mysql_fetch_fields(meta);
	struct ResultColumn
	{
		MYSQL_BIND bind{};
		std::vector<char> bytes;
		unsigned long length = 0;
		bool isNull = false;
		bool error = false;
		enum_field_types fieldType = MYSQL_TYPE_STRING;
	};
	std::vector<ResultColumn> columns(count);
	result->columnNames.reserve(count);
	auto columnNames = std::make_shared<std::vector<std::string>>();
	auto nameToIndex = std::make_shared<std::unordered_map<std::string, size_t>>();
	columnNames->reserve(count);
	nameToIndex->reserve(count);
	for (unsigned int index = 0; index < count; ++index)
	{
		const std::string name = fields[index].name ? fields[index].name : "";
		result->columnNames.push_back(name);
		columnNames->push_back(name);
		(*nameToIndex)[name] = index;
		auto& column = columns[index];
		column.fieldType = fields[index].type;
		const unsigned long suggested = fields[index].max_length > 0 ? fields[index].max_length : 255;
		column.bytes.resize(static_cast<size_t>(suggested) + 1);
		column.bind.buffer_type = MYSQL_TYPE_STRING;
		column.bind.buffer = column.bytes.data();
		column.bind.buffer_length = static_cast<unsigned long>(column.bytes.size());
		column.bind.length = &column.length;
		column.bind.is_null = &column.isNull;
		column.bind.error = &column.error;
	}
	std::vector<MYSQL_BIND> binds(count);
	for (unsigned int index = 0; index < count; ++index) binds[index] = columns[index].bind;
	mysql_stmt_bind_result(statement, binds.data());
	while (true)
	{
		const int rc = mysql_stmt_fetch(statement);
		if (rc == MYSQL_NO_DATA) break;
		if (rc != 0 && rc != MYSQL_DATA_TRUNCATED) { result->lastError = mysql_stmt_error(statement); break; }
		std::vector<Value> values;
		values.reserve(count);
		for (unsigned int index = 0; index < count; ++index)
		{
			const auto& column = columns[index];
			values.push_back(column.isNull ? Value(std::monostate{}) : ConvertNativeCell(column.fieldType, column.bytes.data(), std::min(column.length, static_cast<unsigned long>(column.bytes.size()))));
		}
		result->rows.push_back(BuildRow(columnNames, nameToIndex, std::move(values)));
	}
	mysql_free_result(meta);
	mysql_stmt_free_result(statement);
	return result;
}

} // namespace VM::Runtime
