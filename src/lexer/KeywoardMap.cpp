#include "KeywordMap.h"

KeywordMap::KeywordMap()
{
	m_map["module"] = TokenType::KW_MODULE;
	m_map["import"] = TokenType::KW_IMPORT;
	m_map["as"] = TokenType::KW_AS;
	m_map["export"] = TokenType::KW_EXPORT;
	m_map["type"] = TokenType::KW_TYPE;
	m_map["struct"] = TokenType::KW_STRUCT;
	m_map["enum"] = TokenType::KW_ENUM;
	m_map["interface"] = TokenType::KW_INTERFACE;
	m_map["fn"] = TokenType::KW_FN;
	m_map["var"] = TokenType::KW_VAR;
	m_map["const"] = TokenType::KW_CONST;
	m_map["shared"] = TokenType::KW_SHARED;
	m_map["thread_local"] = TokenType::KW_THREAD_LOCAL;
	m_map["actor"] = TokenType::KW_ACTOR;
	m_map["state"] = TokenType::KW_STATE;
	m_map["msg"] = TokenType::KW_MSG;
	m_map["query"] = TokenType::KW_QUERY;
	m_map["effect"] = TokenType::KW_EFFECT;
	m_map["if"] = TokenType::KW_IF;
	m_map["else"] = TokenType::KW_ELSE;
	m_map["while"] = TokenType::KW_WHILE;
	m_map["iter"] = TokenType::KW_ITER;
	m_map["of"] = TokenType::KW_OF;
	m_map["return"] = TokenType::KW_RETURN;
	m_map["unsafe"] = TokenType::KW_UNSAFE;
	m_map["true"] = TokenType::KW_TRUE;
	m_map["false"] = TokenType::KW_FALSE;
	m_map["null"] = TokenType::KW_NULL;
	m_map["void"] = TokenType::KW_VOID;
	m_map["never"] = TokenType::KW_NEVER;
	m_map["int"] = TokenType::KW_INT;
	m_map["float"] = TokenType::KW_FLOAT;
	m_map["bool"] = TokenType::KW_BOOL;
	m_map["string"] = TokenType::KW_STRING;
	m_map["ptr"] = TokenType::KW_PTR;
	m_map["ref"] = TokenType::KW_REF;
	m_map["requires"] = TokenType::KW_REQUIRES;
	m_map["ensures"] = TokenType::KW_ENSURES;
	m_map["invariant"] = TokenType::KW_INVARIANT;
	m_map["raises"] = TokenType::KW_RAISES;
	m_map["with"] = TokenType::KW_WITH;
	m_map["drop"] = TokenType::KW_DROP;
	m_map["take"] = TokenType::KW_TAKE;
	m_map["reverse"] = TokenType::KW_REVERSE;
	m_map["filter"] = TokenType::KW_FILTER;
	m_map["transform"] = TokenType::KW_TRANSFORM;
	m_map["handle"] = TokenType::KW_HANDLE;
	m_map["transaction"] = TokenType::KW_TRANSACTION;
	m_map["context"] = TokenType::KW_CONTEXT;
	m_map["comptime"] = TokenType::KW_COMPTIME;
	m_map["and"] = TokenType::KW_AND;
	m_map["or"] = TokenType::KW_OR;
	m_map["not"] = TokenType::KW_NOT;
	m_map["mod"] = TokenType::KW_MOD;
	m_map["div"] = TokenType::KW_DIV;
}

TokenType KeywordMap::Lookup(const std::string& id) const
{
	const auto it = m_map.find(id);
	return (it != m_map.end()) ? it->second : TokenType::ID;
}