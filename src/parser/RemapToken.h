#pragma once

#include "Token.h"

namespace remapToken
{
inline std::string RemapTokenTypeToString(const Token& t)
{
	switch (t.type)
	{
	case TokenType::EOF_TOKEN:
		return "EOF";
	case TokenType::ID:
		return "identifier";
	case TokenType::INTEGER_LITERAL:
		return "integer_literal";
	case TokenType::FLOAT_LITERAL:
		return "float_literal";
	case TokenType::STRING_LITERAL:
		return "string_literal";

	case TokenType::KW_MODULE:
		return "module";
	case TokenType::KW_IMPORT:
		return "import";
	case TokenType::KW_AS:
		return "as";
	case TokenType::KW_EXPORT:
		return "export";
	case TokenType::KW_VAR:
		return "var";
	case TokenType::KW_CONST:
		return "const";
	case TokenType::KW_FN:
		return "fn";
	case TokenType::KW_TYPE:
		return "type";
	case TokenType::KW_STRUCT:
		return "struct";
	case TokenType::KW_ENUM:
		return "enum";
	case TokenType::KW_INTERFACE:
		return "interface";
	case TokenType::KW_SHARED:
		return "shared";
	case TokenType::KW_THREAD_LOCAL:
		return "thread_local";
	case TokenType::KW_ACTOR:
		return "actor";
	case TokenType::KW_STATE:
		return "state";
	case TokenType::KW_MSG:
		return "msg";
	case TokenType::KW_QUERY:
		return "query";
	case TokenType::KW_EFFECT:
		return "effect";
	case TokenType::KW_COMPTIME:
		return "comptime";
	case TokenType::KW_PRINT:
		return "print";

	case TokenType::KW_IF:
		return "if";
	case TokenType::KW_ELSE:
		return "else";
	case TokenType::KW_WHILE:
		return "while";
	case TokenType::KW_ITER:
		return "iter";
	case TokenType::KW_OF:
		return "of";
	case TokenType::KW_RETURN:
		return "return";
	case TokenType::KW_HANDLE:
		return "handle";
	case TokenType::KW_WITH:
		return "with";
	case TokenType::KW_TRANSACTION:
		return "transaction";
	case TokenType::KW_UNSAFE:
		return "unsafe";

	case TokenType::KW_INT:
		return "int";
	case TokenType::KW_FLOAT:
		return "float";
	case TokenType::KW_BOOL:
		return "bool";
	case TokenType::KW_STRING:
		return "string";
	case TokenType::KW_VOID:
		return "void";
	case TokenType::KW_NEVER:
		return "never";
	case TokenType::KW_PTR:
		return "ptr";
	case TokenType::KW_REF:
		return "ref";
	case TokenType::KW_TRUE:
		return "true";
	case TokenType::KW_FALSE:
		return "false";
	case TokenType::KW_NULL:
		return "null";

	case TokenType::KW_REQUIRES:
		return "requires";
	case TokenType::KW_ENSURES:
		return "ensures";
	case TokenType::KW_INVARIANT:
		return "invariant";
	case TokenType::KW_RAISES:
		return "raises";

	case TokenType::KW_DROP:
		return "drop";
	case TokenType::KW_TAKE:
		return "take";
	case TokenType::KW_REVERSE:
		return "reverse";
	case TokenType::KW_FILTER:
		return "filter";
	case TokenType::KW_TRANSFORM:
		return "transform";

	case TokenType::SEMICOLON:
		return ";";
	case TokenType::COLON:
		return ":";
	case TokenType::COMMA:
		return ",";
	case TokenType::DOT:
		return ".";
	case TokenType::PIPE:
		return "|";
	case TokenType::PARAN_OPEN:
		return "(";
	case TokenType::PARAN_CLOSE:
		return ")";
	case TokenType::CURLY_OPEN:
		return "{";
	case TokenType::CURLY_CLOSE:
		return "}";
	case TokenType::BRACKET_OPEN:
		return "[";
	case TokenType::BRACKET_CLOSE:
		return "]";
	case TokenType::ARROW:
		return "->";

	case TokenType::OP_ASSIGNMENT:
		return "=";
	case TokenType::OP_PLUS:
		return "+";
	case TokenType::OP_MINUS:
		return "-";
	case TokenType::OP_MUL:
		return "*";
	case TokenType::OP_DIVISION:
		return "/";
	case TokenType::KW_MOD:
		return "mod";
	case TokenType::KW_DIV:
		return "div";

	case TokenType::OP_EQUAL:
		return "==";
	case TokenType::OP_NOT_EQUAL:
		return "!=";
	case TokenType::OP_LESS:
		return "<";
	case TokenType::OP_GREATER:
		return ">";
	case TokenType::OP_LESS_OR_EQUAL:
		return "<=";
	case TokenType::OP_GREATER_OR_EQUAL:
		return ">=";

	case TokenType::OP_DOUBLE_AMPERSAND:
		return "&&";
	case TokenType::OP_DOUBLE_PIPE:
		return "||";
	case TokenType::KW_NOT:
		return "not";
	case TokenType::KW_AND:
		return "and";
	case TokenType::KW_OR:
		return "or";
	default:
		return t.value;
	}
}
} // namespace remapToken
