#pragma once

#include "../../errors/Error.h"
#include "../reader/Reader.h"

namespace utils
{
void SkipWhitespacesAndComments(Reader& reader, size_t& line);
}