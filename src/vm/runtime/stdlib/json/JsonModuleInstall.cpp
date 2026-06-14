#include "JsonModule.h"
#include "JsonModuleInternal.h"

#include "../../../core/values/ValueHelper.h"
#include "../../NativeModuleSupport.h"

#include <cmath>

namespace VM::Runtime
{

using Core::Value;
using Execution::ExecutionContext;

void JsonModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(std::string(ModuleName()), MakeModule(std::string(ModuleName())));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".is_valid",
		MakeNative(
			"is_valid",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto text = RequireString(ctx, args[0], "std.json.is_valid expects a string");
				if (!text)
				{
					return std::monostate{};
				}
				return IsValidJsonText(*text);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".compact",
		MakeNative(
			"compact",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto text = RequireString(ctx, args[0], "std.json.compact expects a string");
				if (!text)
				{
					return std::monostate{};
				}
				const auto compact = CompactJsonText(*text);
				if (!compact)
				{
					ctx.RaiseError("std.json.compact expects valid JSON");
					return std::monostate{};
				}
				return JsonInternal::MakeSharedString(*compact);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".quote",
		MakeNative(
			"quote",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto text = RequireString(ctx, args[0], "std.json.quote expects a string");
				if (!text)
				{
					return std::monostate{};
				}
				return JsonInternal::MakeSharedString(JsonInternal::EscapeJsonString(*text));
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".encode_number",
		MakeNative(
			"encode_number",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				if (std::holds_alternative<int64_t>(args[0]))
				{
					return JsonInternal::MakeSharedString(std::to_string(std::get<int64_t>(args[0])));
				}
				if (std::holds_alternative<double>(args[0]))
				{
					const double value = std::get<double>(args[0]);
					if (!std::isfinite(value))
					{
						ctx.RaiseError("std.json.encode_number expects a finite number");
						return std::monostate{};
					}
					return JsonInternal::MakeSharedString(Core::ValueHelper::ToString(args[0]));
				}
				ctx.RaiseError("std.json.encode_number expects an int or float");
				return std::monostate{};
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".has_key",
		MakeNative(
			"has_key",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto json = RequireString(ctx, args[0], "std.json.has_key expects an object JSON string");
				if (!json)
				{
					return std::monostate{};
				}
				const auto key = RequireString(ctx, args[1], "std.json.has_key expects a string key");
				if (!key)
				{
					return std::monostate{};
				}

				std::string raw;
				return JsonInternal::LookupTopLevelField(*json, *key, raw);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".get_string",
		MakeNative(
			"get_string",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto json = RequireString(ctx, args[0], "std.json.get_string expects an object JSON string");
				if (!json)
				{
					return std::monostate{};
				}
				const auto key = RequireString(ctx, args[1], "std.json.get_string expects a string key");
				if (!key)
				{
					return std::monostate{};
				}

				std::string raw;
				if (!JsonInternal::LookupTopLevelField(*json, *key, raw))
				{
					ctx.RaiseError("std.json.get_string could not find the requested field");
					return std::monostate{};
				}

				const auto decoded = JsonInternal::DecodeJsonString(raw);
				if (!decoded)
				{
					ctx.RaiseError("std.json.get_string expects the field value to be a JSON string");
					return std::monostate{};
				}
				return JsonInternal::MakeSharedString(*decoded);
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".get_int",
		MakeNative(
			"get_int",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto json = RequireString(ctx, args[0], "std.json.get_int expects an object JSON string");
				if (!json)
				{
					return std::monostate{};
				}
				const auto key = RequireString(ctx, args[1], "std.json.get_int expects a string key");
				if (!key)
				{
					return std::monostate{};
				}

				std::string raw;
				if (!JsonInternal::LookupTopLevelField(*json, *key, raw))
				{
					ctx.RaiseError("std.json.get_int could not find the requested field");
					return std::monostate{};
				}

				int64_t value = 0;
				if (!JsonInternal::ParseJsonInt(raw, value))
				{
					ctx.RaiseError("std.json.get_int expects the field value to be an integer");
					return std::monostate{};
				}
				return value;
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".get_bool",
		MakeNative(
			"get_bool",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto json = RequireString(ctx, args[0], "std.json.get_bool expects an object JSON string");
				if (!json)
				{
					return std::monostate{};
				}
				const auto key = RequireString(ctx, args[1], "std.json.get_bool expects a string key");
				if (!key)
				{
					return std::monostate{};
				}

				std::string raw;
				if (!JsonInternal::LookupTopLevelField(*json, *key, raw))
				{
					ctx.RaiseError("std.json.get_bool could not find the requested field");
					return std::monostate{};
				}

				bool value = false;
				if (!JsonInternal::ParseJsonBool(raw, value))
				{
					ctx.RaiseError("std.json.get_bool expects the field value to be a boolean");
					return std::monostate{};
				}
				return value;
			}));
}

} // namespace VM::Runtime
