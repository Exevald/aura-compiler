#pragma once

#include "../Token.h"
#include "../reader/Reader.h"
#include <unordered_map>
#include "../KeywordMap.h"

namespace identifierRule
{
Token ParseIdentifier(Reader& reader, size_t pos, size_t line, const KeywordMap& keywords);
}