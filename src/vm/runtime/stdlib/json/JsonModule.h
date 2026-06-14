#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "../../SharedRuntime.h"

namespace VM::Runtime
{

class JsonModule
{
public:
	static constexpr std::string_view ModuleName() { return "std.json_native"; }

	static bool IsValidJsonText(std::string_view input);
	static std::optional<std::string> CompactJsonText(std::string_view input);
	static void Install(SharedRuntime& runtime);
};

} // namespace VM::Runtime
