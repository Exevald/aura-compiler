#pragma once

#include "../Token.h"
#include "../reader/Reader.h"

namespace numberRule
{
Token ParseNumber(Reader& reader, size_t pos, size_t line);
}