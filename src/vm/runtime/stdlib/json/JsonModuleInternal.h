#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace VM::Runtime::JsonInternal
{

std::shared_ptr<const std::string> MakeSharedString(const std::string& value);
std::string EscapeJsonString(std::string_view input);
bool LookupTopLevelField(std::string_view objectJson, std::string_view fieldName, std::string& rawValue);
std::optional<std::string> DecodeJsonString(std::string_view rawJsonString);
bool ParseJsonInt(std::string_view rawValue, int64_t& value);
bool ParseJsonBool(std::string_view rawValue, bool& value);

} // namespace VM::Runtime::JsonInternal
