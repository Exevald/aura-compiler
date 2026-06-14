#include "Base64Module.h"

#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

#include <array>

namespace VM::Runtime
{

using Core::Array;
using Core::Value;
using Execution::ExecutionContext;

namespace
{

Core::ArrayPtr MakeDecodeResult(const bool ok, const std::string& value, const std::string& error)
{
	auto result = std::make_shared<Array>();
	result->elements.push_back(ok);
	result->elements.push_back(std::make_shared<const std::string>(value));
	result->elements.push_back(std::make_shared<const std::string>(error));
	return result;
}

bool DecodeBase64Url(const std::string_view input, std::string& output, std::string& error)
{
	static const auto alphabet = [] {
		std::array<int, 256> table{};
		table.fill(-1);
		for (int i = 0; i < 26; ++i)
		{
			table[static_cast<unsigned char>('A' + i)] = i;
			table[static_cast<unsigned char>('a' + i)] = 26 + i;
		}
		for (int i = 0; i < 10; ++i)
		{
			table[static_cast<unsigned char>('0' + i)] = 52 + i;
		}
		table[static_cast<unsigned char>('-')] = 62;
		table[static_cast<unsigned char>('_')] = 63;
		table[static_cast<unsigned char>('=')] = 0;
		return table;
	}();

	if (input.empty())
	{
		output.clear();
		return true;
	}

	if (input.size() % 4 == 1)
	{
		error = "invalid base64url length";
		return false;
	}

	std::string normalized(input);
	while (normalized.size() % 4 != 0)
	{
		normalized.push_back('=');
	}

	output.clear();
	output.reserve((normalized.size() / 4) * 3);

	for (size_t i = 0; i < normalized.size(); i += 4)
	{
		const unsigned char c0 = static_cast<unsigned char>(normalized[i]);
		const unsigned char c1 = static_cast<unsigned char>(normalized[i + 1]);
		const unsigned char c2 = static_cast<unsigned char>(normalized[i + 2]);
		const unsigned char c3 = static_cast<unsigned char>(normalized[i + 3]);

		const int v0 = alphabet[c0];
		const int v1 = alphabet[c1];
		const int v2 = alphabet[c2];
		const int v3 = alphabet[c3];
		if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0)
		{
			error = "invalid base64url character";
			return false;
		}

		const uint32_t block = (static_cast<uint32_t>(v0) << 18)
			| (static_cast<uint32_t>(v1) << 12)
			| (static_cast<uint32_t>(v2) << 6)
			| static_cast<uint32_t>(v3);

		output.push_back(static_cast<char>((block >> 16) & 0xFF));
		if (normalized[i + 2] != '=')
		{
			output.push_back(static_cast<char>((block >> 8) & 0xFF));
		}
		if (normalized[i + 3] != '=')
		{
			output.push_back(static_cast<char>(block & 0xFF));
		}
	}

	return true;
}

} // namespace

void Base64Module::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(std::string(ModuleName()), MakeModule(std::string(ModuleName())));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".try_url_decode",
		MakeNative(
			"try_url_decode",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto value = RequireString(ctx, args[0], "std.base64.try_url_decode expects a string");
				if (!value)
				{
					return std::monostate{};
				}

				std::string decoded;
				std::string error;
				if (!DecodeBase64Url(*value, decoded, error))
				{
					return MakeDecodeResult(false, "", error);
				}
				return MakeDecodeResult(true, decoded, "");
			}));
}

} // namespace VM::Runtime
