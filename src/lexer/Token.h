#pragma once

#include "../errors/Error.h"

#include <string>

enum class TokenType
{
	ERROR,
	EOF_TOKEN,

	PARAN_OPEN,
	PARAN_CLOSE,
	CURLY_OPEN,
	CURLY_CLOSE,
	BRACKET_OPEN,
	BRACKET_CLOSE,
	SEMICOLON,
	COMMA,
	COLON,
	DOT,
	PIPE,
	ARROW,

	OP_PLUS,
	OP_MINUS,
	OP_MUL,
	OP_DIVISION,
	OP_ASSIGNMENT,
	OP_EQUAL,
	OP_NOT_EQUAL,
	OP_LESS,
	OP_GREATER,
	OP_LESS_OR_EQUAL,
	OP_GREATER_OR_EQUAL,
	OP_DOUBLE_AMPERSAND,
	OP_DOUBLE_PIPE,

	KW_AND,
	KW_OR,
	KW_NOT,
	KW_MOD,
	KW_DIV,

	KW_MODULE,
	KW_IMPORT,
	KW_AS,
	KW_EXPORT,
	KW_TYPE,
	KW_STRUCT,
	KW_ENUM,
	KW_INTERFACE,
	KW_FN,
	KW_VAR,
	KW_CONST,
	KW_SHARED,
	KW_THREAD_LOCAL,
	KW_ACTOR,
	KW_STATE,
	KW_MSG,
	KW_QUERY,
	KW_EFFECT,

	KW_IF,
	KW_ELSE,
	KW_WHILE,
	KW_ITER,
	KW_OF,
	KW_RETURN,
	KW_UNSAFE,

	KW_TRUE,
	KW_FALSE,
	KW_NULL,
	KW_VOID,
	KW_NEVER,

	KW_INT,
	KW_FLOAT,
	KW_BOOL,
	KW_STRING,
	KW_PTR,
	KW_REF,

	KW_REQUIRES,
	KW_ENSURES,
	KW_INVARIANT,
	KW_RAISES,
	KW_WITH,

	KW_DROP,
	KW_TAKE,
	KW_REVERSE,
	KW_FILTER,
	KW_TRANSFORM,

	KW_HANDLE,
	KW_TRANSACTION,
	KW_CONTEXT,
	KW_COMPTIME,

	INTEGER_LITERAL,
	FLOAT_LITERAL,
	STRING_LITERAL,
	ID
};

struct Token
{
	TokenType type;
	std::string value;
	size_t pos;
	size_t line;
	Error error = Error::NONE;
};

inline bool operator==(Token const& left, Token const& right)
{
	return left.type == right.type
		&& left.value == right.value
		&& left.pos == right.pos
		&& left.line == right.line
		&& left.error == right.error;
}