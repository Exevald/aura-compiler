#pragma once

#include "../Token.h"
#include "../reader/Reader.h"

namespace stringRule
{
Token ParseString(Reader& reader, size_t pos, size_t line);
}