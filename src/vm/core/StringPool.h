#pragma once

#include "Value.h"

#include <unordered_set>

namespace VM::Core
{

class StringPool
{
public:
	StringPtr Intern(std::string_view str);

private:
	struct Hash
	{
		using is_transparent = void;
		size_t operator()(const std::string_view v) const { return std::hash<std::string_view>{}(v); }
		size_t operator()(const StringPtr& v) const { return std::hash<std::string>{}(*v); }
	};

	struct Equal
	{
		using is_transparent = void;
		bool operator()(const StringPtr& a, const StringPtr& b) const { return *a == *b; }
		bool operator()(const StringPtr& a, const std::string_view b) const { return *a == b; }
		bool operator()(const std::string_view a, const StringPtr& b) const { return a == *b; }
	};

	std::unordered_set<StringPtr, Hash, Equal> m_pool;
};

} // namespace VM::Core