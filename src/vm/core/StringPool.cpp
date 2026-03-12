#include "StringPool.h"

VM::Core::StringPtr VM::Core::StringPool::Intern(std::string_view str)
{
	if (const auto it = m_pool.find(str); it != m_pool.end())
	{
		return *it;
	}
	auto interned = std::make_shared<const std::string>(str);
	m_pool.insert(interned);
	return interned;
}