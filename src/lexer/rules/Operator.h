#pragma once

#include "../Token.h"
#include "../reader/Reader.h"

namespace operatorRule
{
Token ParseOperator(Reader& reader, size_t pos, size_t line);
}