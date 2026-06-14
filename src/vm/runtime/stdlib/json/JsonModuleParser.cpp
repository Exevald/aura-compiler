#include "JsonModule.h"

#include "../json/JsonModuleInternal.h"

#include <cctype>
#include <charconv>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace VM::Runtime
{

namespace
{

struct CachedJsonEntry
{
	bool valid = false;
	std::string compact;
};

std::mutex g_jsonCacheMutex;
std::unordered_map<std::string, CachedJsonEntry> g_jsonCache;

struct JsonParser
{
	explicit JsonParser(std::string_view input)
		: source(input)
	{
	}

	std::string_view source;
	size_t index = 0;

	void SkipWhitespace()
	{
		while (index < source.size() && std::isspace(static_cast<unsigned char>(source[index])))
		{
			++index;
		}
	}

	bool End() const { return index >= source.size(); }
	char Peek() const { return End() ? '\0' : source[index]; }

	bool Consume(const char expected)
	{
		if (Peek() != expected)
		{
			return false;
		}
		++index;
		return true;
	}

	bool ParseHexQuad()
	{
		for (int i = 0; i < 4; ++i)
		{
			if (index >= source.size() || !std::isxdigit(static_cast<unsigned char>(source[index])))
			{
				return false;
			}
			++index;
		}
		return true;
	}

	bool ParseStringToken(std::string& rawToken)
	{
		if (!Consume('"'))
		{
			return false;
		}

		const size_t start = index - 1;
		while (index < source.size())
		{
			const char ch = source[index++];
			if (ch == '"')
			{
				rawToken.assign(source.substr(start, index - start));
				return true;
			}
			if (ch == '\\')
			{
				if (index >= source.size())
				{
					return false;
				}
				const char escaped = source[index++];
				switch (escaped)
				{
				case '"':
				case '\\':
				case '/':
				case 'b':
				case 'f':
				case 'n':
				case 'r':
				case 't':
					break;
				case 'u':
					if (!ParseHexQuad())
					{
						return false;
					}
					break;
				default:
					return false;
				}
			}
			else if (static_cast<unsigned char>(ch) < 0x20)
			{
				return false;
			}
		}
		return false;
	}

	bool ParseStringValue(std::string& decoded)
	{
		if (!Consume('"'))
		{
			return false;
		}

		decoded.clear();
		while (index < source.size())
		{
			const char ch = source[index++];
			if (ch == '"')
			{
				return true;
			}
			if (ch == '\\')
			{
				if (index >= source.size())
				{
					return false;
				}
				const char escaped = source[index++];
				switch (escaped)
				{
				case '"':
				case '\\':
				case '/':
					decoded.push_back(escaped);
					break;
				case 'b':
					decoded.push_back('\b');
					break;
				case 'f':
					decoded.push_back('\f');
					break;
				case 'n':
					decoded.push_back('\n');
					break;
				case 'r':
					decoded.push_back('\r');
					break;
				case 't':
					decoded.push_back('\t');
					break;
				case 'u':
					if (!ParseHexQuad())
					{
						return false;
					}
					decoded.push_back('?');
					break;
				default:
					return false;
				}
			}
			else if (static_cast<unsigned char>(ch) < 0x20)
			{
				return false;
			}
			else
			{
				decoded.push_back(ch);
			}
		}
		return false;
	}

	bool ParseLiteral(const std::string_view literal)
	{
		if (source.substr(index, literal.size()) != literal)
		{
			return false;
		}
		index += literal.size();
		return true;
	}

	bool ParseNumber(std::string& out)
	{
		const size_t start = index;

		if (Consume('-') && End())
		{
			return false;
		}

		if (Consume('0'))
		{
			if (!End() && std::isdigit(static_cast<unsigned char>(Peek())))
			{
				return false;
			}
		}
		else
		{
			if (End() || !std::isdigit(static_cast<unsigned char>(Peek())))
			{
				return false;
			}
			while (!End() && std::isdigit(static_cast<unsigned char>(Peek())))
			{
				++index;
			}
		}

		if (Consume('.'))
		{
			if (End() || !std::isdigit(static_cast<unsigned char>(Peek())))
			{
				return false;
			}
			while (!End() && std::isdigit(static_cast<unsigned char>(Peek())))
			{
				++index;
			}
		}

		if (Peek() == 'e' || Peek() == 'E')
		{
			++index;
			if (Peek() == '+' || Peek() == '-')
			{
				++index;
			}
			if (End() || !std::isdigit(static_cast<unsigned char>(Peek())))
			{
				return false;
			}
			while (!End() && std::isdigit(static_cast<unsigned char>(Peek())))
			{
				++index;
			}
		}

		out.assign(source.substr(start, index - start));
		return true;
	}

	bool ParseValue(std::string& compact)
	{
		SkipWhitespace();
		if (End())
		{
			return false;
		}

		switch (Peek())
		{
		case '{':
			return ParseObject(compact);
		case '[':
			return ParseArray(compact);
		case '"':
			return ParseStringToken(compact);
		case 't':
			if (ParseLiteral("true"))
			{
				compact = "true";
				return true;
			}
			return false;
		case 'f':
			if (ParseLiteral("false"))
			{
				compact = "false";
				return true;
			}
			return false;
		case 'n':
			if (ParseLiteral("null"))
			{
				compact = "null";
				return true;
			}
			return false;
		default:
			if (Peek() == '-' || std::isdigit(static_cast<unsigned char>(Peek())))
			{
				return ParseNumber(compact);
			}
			return false;
		}
	}

	bool ParseArray(std::string& compact)
	{
		if (!Consume('['))
		{
			return false;
		}

		SkipWhitespace();
		compact = "[";
		if (Consume(']'))
		{
			compact += "]";
			return true;
		}

		bool first = true;
		while (true)
		{
			std::string value;
			if (!ParseValue(value))
			{
				return false;
			}
			if (!first)
			{
				compact += ",";
			}
			compact += value;
			first = false;

			SkipWhitespace();
			if (Consume(']'))
			{
				compact += "]";
				return true;
			}
			if (!Consume(','))
			{
				return false;
			}
			SkipWhitespace();
		}
	}

	bool ParseObject(std::string& compact)
	{
		if (!Consume('{'))
		{
			return false;
		}

		SkipWhitespace();
		compact = "{";
		if (Consume('}'))
		{
			compact += "}";
			return true;
		}

		bool first = true;
		while (true)
		{
			std::string key;
			if (!ParseStringToken(key))
			{
				return false;
			}

			SkipWhitespace();
			if (!Consume(':'))
			{
				return false;
			}
			SkipWhitespace();

			std::string value;
			if (!ParseValue(value))
			{
				return false;
			}

			if (!first)
			{
				compact += ",";
			}
			compact += key;
			compact += ":";
			compact += value;
			first = false;

			SkipWhitespace();
			if (Consume('}'))
			{
				compact += "}";
				return true;
			}
			if (!Consume(','))
			{
				return false;
			}
			SkipWhitespace();
		}
	}
};

bool ParseCompactJson(const std::string_view input, std::string& compact)
{
	JsonParser parser(input);
	if (!parser.ParseValue(compact))
	{
		return false;
	}
	parser.SkipWhitespace();
	return parser.index == input.size();
}

const CachedJsonEntry& LookupCompactJson(const std::string_view input)
{
	const std::string key(input);
	{
		std::lock_guard lock(g_jsonCacheMutex);
		if (const auto it = g_jsonCache.find(key); it != g_jsonCache.end())
		{
			return it->second;
		}
	}

	CachedJsonEntry entry;
	entry.valid = ParseCompactJson(input, entry.compact);

	std::lock_guard lock(g_jsonCacheMutex);
	return g_jsonCache.emplace(key, std::move(entry)).first->second;
}

bool ParseJsonString(const std::string_view rawJsonString, std::string& decoded)
{
	JsonParser parser(rawJsonString);
	if (!parser.ParseStringValue(decoded))
	{
		return false;
	}
	parser.SkipWhitespace();
	return parser.End();
}

bool LookupObjectField(
	const std::string_view objectJson,
	const std::string_view fieldName,
	std::string& rawValue)
{
	JsonParser parser(objectJson);
	parser.SkipWhitespace();
	if (!parser.Consume('{'))
	{
		return false;
	}
	parser.SkipWhitespace();
	if (parser.Consume('}'))
	{
		return false;
	}

	while (true)
	{
		std::string decodedKey;
		if (!parser.ParseStringValue(decodedKey))
		{
			return false;
		}
		parser.SkipWhitespace();
		if (!parser.Consume(':'))
		{
			return false;
		}
		parser.SkipWhitespace();

		std::string currentValue;
		if (!parser.ParseValue(currentValue))
		{
			return false;
		}
		if (decodedKey == fieldName)
		{
			rawValue = std::move(currentValue);
			return true;
		}

		parser.SkipWhitespace();
		if (parser.Consume('}'))
		{
			return false;
		}
		if (!parser.Consume(','))
		{
			return false;
		}
		parser.SkipWhitespace();
	}
}

} // namespace

bool JsonModule::IsValidJsonText(const std::string_view input)
{
	return LookupCompactJson(input).valid;
}

std::optional<std::string> JsonModule::CompactJsonText(const std::string_view input)
{
	const auto& cached = LookupCompactJson(input);
	if (!cached.valid)
	{
		return std::nullopt;
	}
	return cached.compact;
}

bool JsonInternal::LookupTopLevelField(
	const std::string_view objectJson,
	const std::string_view fieldName,
	std::string& rawValue)
{
	return LookupObjectField(objectJson, fieldName, rawValue);
}

std::optional<std::string> JsonInternal::DecodeJsonString(const std::string_view rawJsonString)
{
	std::string decoded;
	if (!ParseJsonString(rawJsonString, decoded))
	{
		return std::nullopt;
	}
	return decoded;
}

bool JsonInternal::ParseJsonInt(const std::string_view rawValue, int64_t& value)
{
	const auto [ptr, ec] = std::from_chars(rawValue.data(), rawValue.data() + rawValue.size(), value);
	return ec == std::errc{} && ptr == rawValue.data() + rawValue.size();
}

bool JsonInternal::ParseJsonBool(const std::string_view rawValue, bool& value)
{
	if (rawValue == "true")
	{
		value = true;
		return true;
	}
	if (rawValue == "false")
	{
		value = false;
		return true;
	}
	return false;
}

} // namespace VM::Runtime
