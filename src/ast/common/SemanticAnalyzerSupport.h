#pragma once

#include "../sematic/SemanticAnalyzer.h"

#include <cctype>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SemanticAnalyzerDetail
{
class ScopeExit
{
public:
	explicit ScopeExit(std::function<void()> fn)
		: m_fn(std::move(fn))
	{
	}

	ScopeExit(const ScopeExit&) = delete;
	ScopeExit& operator=(const ScopeExit&) = delete;

	~ScopeExit()
	{
		if (m_fn)
		{
			m_fn();
		}
	}

private:
	std::function<void()> m_fn;
};

inline std::string FormatUndefined(const std::string& name)
{
	return "Undefined variable: " + name;
}

inline std::string Trim(std::string value)
{
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
	{
		value.erase(value.begin());
	}
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
	{
		value.pop_back();
	}
	return value;
}

inline std::vector<std::string> SplitTopLevel(const std::string& text, const char delimiter)
{
	std::vector<std::string> parts;
	int depthAngles = 0;
	int depthBrackets = 0;
	int depthParens = 0;
	size_t start = 0;

	for (size_t i = 0; i < text.size(); ++i)
	{
		switch (text[i])
		{
		case '<':
			++depthAngles;
			break;
		case '>':
			--depthAngles;
			break;
		case '[':
			++depthBrackets;
			break;
		case ']':
			--depthBrackets;
			break;
		case '(':
			++depthParens;
			break;
		case ')':
			--depthParens;
			break;
		default:
			break;
		}

		if (text[i] == delimiter && depthAngles == 0 && depthBrackets == 0 && depthParens == 0)
		{
			parts.push_back(Trim(text.substr(start, i - start)));
			start = i + 1;
		}
	}

	parts.push_back(Trim(text.substr(start)));
	return parts;
}

inline bool SplitGenericName(const std::string& text, std::string& baseName, std::vector<std::string>& args)
{
	const size_t anglePos = text.find('<');
	if (anglePos == std::string::npos || text.back() != '>')
	{
		return false;
	}

	int depth = 0;
	for (size_t i = anglePos; i < text.size(); ++i)
	{
		if (text[i] == '<')
		{
			++depth;
		}
		else if (text[i] == '>')
		{
			--depth;
			if (depth == 0 && i != text.size() - 1)
			{
				return false;
			}
		}
	}

	baseName = Trim(text.substr(0, anglePos));
	args = SplitTopLevel(text.substr(anglePos + 1, text.size() - anglePos - 2), ',');
	return true;
}

inline std::string SubstituteTypeString(
	const std::string& text,
	const std::unordered_map<std::string, std::string>& replacements)
{
	for (const auto& [name, replacement] : replacements)
	{
		if (text == name)
		{
			return replacement;
		}
	}

	if ((text.rfind("ptr<", 0) == 0 || text.rfind("ref<", 0) == 0) && text.back() == '>')
	{
		return text.substr(0, 4) + SubstituteTypeString(text.substr(4, text.size() - 5), replacements) + ">";
	}

	if (text.size() > 2 && text.front() == '[' && text.back() == ']')
	{
		return "[" + SubstituteTypeString(text.substr(1, text.size() - 2), replacements) + "]";
	}

	if (const size_t arrowPos = text.find("->"); arrowPos != std::string::npos)
	{
		return SubstituteTypeString(text.substr(0, arrowPos), replacements)
			+ "->"
			+ SubstituteTypeString(text.substr(arrowPos + 2), replacements);
	}

	std::vector<std::string> args;
	if (std::string baseName; SplitGenericName(text, baseName, args))
	{
		std::string result = baseName + "<";
		for (size_t i = 0; i < args.size(); ++i)
		{
			if (i > 0)
			{
				result += ",";
			}
			result += SubstituteTypeString(args[i], replacements);
		}
		result += ">";
		return result;
	}

	return text;
}

} // namespace SemanticAnalyzerDetail