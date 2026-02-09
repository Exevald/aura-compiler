#include "Identifier.h"

KeywordMap::KeywordMap()
{
	m_map = {
		{ "module", TokenType::KW_MODULE }, { "import", TokenType::KW_IMPORT }, { "as", TokenType::KW_AS },
		{ "export", TokenType::KW_EXPORT }, { "type", TokenType::KW_TYPE }, { "struct", TokenType::KW_STRUCT },
		{ "enum", TokenType::KW_ENUM }, { "interface", TokenType::KW_INTERFACE }, { "fn", TokenType::KW_FN },
		{ "var", TokenType::KW_VAR }, { "const", TokenType::KW_CONST }, { "shared", TokenType::KW_SHARED },
		{ "thread_local", TokenType::KW_THREAD_LOCAL }, { "actor", TokenType::KW_ACTOR }, { "state", TokenType::KW_STATE },
		{ "msg", TokenType::KW_MSG }, { "query", TokenType::KW_QUERY }, { "effect", TokenType::KW_EFFECT },
		{ "if", TokenType::KW_IF }, { "else", TokenType::KW_ELSE }, { "while", TokenType::KW_WHILE },
		{ "iter", TokenType::KW_ITER }, { "of", TokenType::KW_OF }, { "return", TokenType::KW_RETURN },
		{ "unsafe", TokenType::KW_UNSAFE }, { "true", TokenType::KW_TRUE }, { "false", TokenType::KW_FALSE },
		{ "null", TokenType::KW_NULL }, { "void", TokenType::KW_VOID }, { "never", TokenType::KW_NEVER },
		{ "int", TokenType::KW_INT }, { "float", TokenType::KW_FLOAT }, { "bool", TokenType::KW_BOOL },
		{ "string", TokenType::KW_STRING }, { "ptr", TokenType::KW_PTR }, { "ref", TokenType::KW_REF },
		{ "requires", TokenType::KW_REQUIRES }, { "ensures", TokenType::KW_ENSURES }, { "invariant", TokenType::KW_INVARIANT },
		{ "raises", TokenType::KW_RAISES }, { "with", TokenType::KW_WITH }, { "drop", TokenType::KW_DROP },
		{ "take", TokenType::KW_TAKE }, { "reverse", TokenType::KW_REVERSE }, { "filter", TokenType::KW_FILTER },
		{ "transform", TokenType::KW_TRANSFORM }, { "handle", TokenType::KW_HANDLE }, { "transaction", TokenType::KW_TRANSACTION },
		{ "context", TokenType::KW_CONTEXT }, { "comptime", TokenType::KW_COMPTIME }, { "and", TokenType::KW_AND },
		{ "or", TokenType::KW_OR }, { "not", TokenType::KW_NOT }, { "mod", TokenType::KW_MOD }, { "div", TokenType::KW_DIV }
	};
}

TokenType KeywordMap::Lookup(const std::string& id) const
{
	auto kw = m_map.find(id);
	return (kw != m_map.end()) ? kw->second : TokenType::ID;
}

Token identifierRule::ParseIdentifier(Reader& reader, size_t pos, size_t line, const KeywordMap& keywords)
{
	std::string value;

	if (!std::isalpha(static_cast<unsigned char>(reader.Peek())) && reader.Peek() != '_')
	{
		return {
			TokenType::ERROR,
			/*value=*/"",
			pos,
			line,
			Error::UNEXPECTED_CHARACTER
		};
	}

	value += reader.Get();
	while (!reader.EndOfFile()
		&& (std::isalnum(static_cast<unsigned char>(reader.Peek())) || reader.Peek() == '_'))
	{
		value += reader.Get();
	}

	while (!reader.EndOfFile() && reader.Peek() == '.')
	{
		value += reader.Get();

		if (!std::isalpha(static_cast<unsigned char>(reader.Peek())) && reader.Peek() != '_')
		{
			reader.Unget();
			value.pop_back();
			break;
		}

		value += reader.Get();
		while (!reader.EndOfFile()
			&& (std::isalnum(static_cast<unsigned char>(reader.Peek())) || reader.Peek() == '_'))
		{
			value += reader.Get();
		}
	}

	if (value.find('.') == std::string::npos)
	{
		TokenType kwType = keywords.Lookup(value);
		if (kwType != TokenType::ID)
		{
			return { kwType, value, pos, line, Error::NONE };
		}
	}

	return { TokenType::ID, value, pos, line, Error::NONE };
}
