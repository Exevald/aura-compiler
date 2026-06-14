#pragma once

#include "../core/values/Value.h"
#include "../core/values/ValueHelper.h"
#include "ExecutionContext.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace VM::Runtime
{

template <typename Fn>
Core::NativeFunctionPtr MakeNative(
	const std::string& name,
	const int arity,
	Fn&& fn,
	const bool variadic = false)
{
	auto native = std::make_shared<Core::NativeFunction>();
	native->name = name;
	native->arity = arity;
	native->variadic = variadic;
	native->invoke = std::forward<Fn>(fn);
	return native;
}

template <typename Fn>
Core::NativeFunctionPtr MakeNative0(const std::string& name, Fn&& fn)
{
	auto native = std::make_shared<Core::NativeFunction>();
	native->name = name;
	native->arity = 0;
	native->invoke0 = std::forward<Fn>(fn);
	return native;
}

template <typename Fn>
Core::NativeFunctionPtr MakeNative1(const std::string& name, Fn&& fn)
{
	auto native = std::make_shared<Core::NativeFunction>();
	native->name = name;
	native->arity = 1;
	native->invoke1 = std::forward<Fn>(fn);
	return native;
}

template <typename Fn>
Core::NativeFunctionPtr MakeNative2(const std::string& name, Fn&& fn)
{
	auto native = std::make_shared<Core::NativeFunction>();
	native->name = name;
	native->arity = 2;
	native->invoke2 = std::forward<Fn>(fn);
	return native;
}

template <typename Fn>
Core::NativeFunctionPtr MakeNative3(const std::string& name, Fn&& fn)
{
	auto native = std::make_shared<Core::NativeFunction>();
	native->name = name;
	native->arity = 3;
	native->invoke3 = std::forward<Fn>(fn);
	return native;
}

template <typename Fn>
Core::NativeFunctionPtr MakeNative4(const std::string& name, Fn&& fn)
{
	auto native = std::make_shared<Core::NativeFunction>();
	native->name = name;
	native->arity = 4;
	native->invoke4 = std::forward<Fn>(fn);
	return native;
}

inline Core::ModulePtr MakeModule(const std::string& name, const bool cacheableMembers = true)
{
	auto module = std::make_shared<Core::Module>();
	module->name = name;
	module->cacheableMembers = cacheableMembers;
	return module;
}

inline int CompareValues(const Core::Value& lhs, const Core::Value& rhs, const std::string& domain)
{
	using Core::StringPtr;
	using Core::ValueHelper;

	if (std::holds_alternative<int64_t>(lhs) || std::holds_alternative<double>(lhs))
	{
		if (!(std::holds_alternative<int64_t>(rhs) || std::holds_alternative<double>(rhs)))
		{
			throw std::runtime_error(domain + " comparison requires compatible value types");
		}

		const auto left = ValueHelper::As<double>(lhs);
		const auto right = ValueHelper::As<double>(rhs);
		if (left < right)
		{
			return -1;
		}
		if (left > right)
		{
			return 1;
		}
		return 0;
	}

	if (std::holds_alternative<StringPtr>(lhs) && std::holds_alternative<StringPtr>(rhs))
	{
		const auto& left = std::get<StringPtr>(lhs);
		const auto& right = std::get<StringPtr>(rhs);
		const std::string leftValue = left ? *left : "";
		const std::string rightValue = right ? *right : "";
		if (leftValue < rightValue)
		{
			return -1;
		}
		if (leftValue > rightValue)
		{
			return 1;
		}
		return 0;
	}

	if (std::holds_alternative<bool>(lhs) && std::holds_alternative<bool>(rhs))
	{
		const bool left = std::get<bool>(lhs);
		if (const bool right = std::get<bool>(rhs); left == right)
		{
			return 0;
		}
		return left ? 1 : -1;
	}

	throw std::runtime_error(domain + " comparison requires int, float, bool, or string values");
}

inline Core::ArrayPtr RequireArray(
	Execution::ExecutionContext& ctx,
	const Core::Value& value,
	const std::string& error = "Expected an array")
{
	if (!std::holds_alternative<Core::ArrayPtr>(value) || !std::get<Core::ArrayPtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<Core::ArrayPtr>(value);
}

inline Core::StringPtr RequireString(
	Execution::ExecutionContext& ctx,
	const Core::Value& value,
	const std::string& error = "Expected a string")
{
	if (!std::holds_alternative<Core::StringPtr>(value) || !std::get<Core::StringPtr>(value))
	{
		ctx.RaiseError(error);
		return {};
	}
	return std::get<Core::StringPtr>(value);
}

inline Core::Value ConvertToInt(
	Execution::ExecutionContext& ctx,
	const Core::Value& value,
	const std::string& domain)
{
	if (std::holds_alternative<int64_t>(value))
	{
		return std::get<int64_t>(value);
	}
	if (std::holds_alternative<double>(value))
	{
		return static_cast<int64_t>(std::get<double>(value));
	}
	if (std::holds_alternative<bool>(value))
	{
		return std::get<bool>(value) ? int64_t{ 1 } : int64_t{ 0 };
	}
	if (std::holds_alternative<Core::StringPtr>(value) && std::get<Core::StringPtr>(value))
	{
		try
		{
			size_t parsed = 0;
			const auto result = std::stoll(*std::get<Core::StringPtr>(value), &parsed);
			if (parsed == std::get<Core::StringPtr>(value)->size())
			{
				return result;
			}
		}
		catch (const std::exception&)
		{
		}
	}
	ctx.RaiseError(domain + ".to_int cannot convert value to int");
	return std::monostate{};
}

inline Core::Value ConvertToFloat(
	Execution::ExecutionContext& ctx,
	const Core::Value& value,
	const std::string& domain)
{
	if (std::holds_alternative<double>(value))
	{
		return std::get<double>(value);
	}
	if (std::holds_alternative<int64_t>(value))
	{
		return static_cast<double>(std::get<int64_t>(value));
	}
	if (std::holds_alternative<bool>(value))
	{
		return std::get<bool>(value) ? 1.0 : 0.0;
	}
	if (std::holds_alternative<Core::StringPtr>(value) && std::get<Core::StringPtr>(value))
	{
		try
		{
			size_t parsed = 0;
			const auto result = std::stod(*std::get<Core::StringPtr>(value), &parsed);
			if (parsed == std::get<Core::StringPtr>(value)->size())
			{
				return result;
			}
		}
		catch (const std::exception&)
		{
		}
	}
	ctx.RaiseError(domain + ".to_float cannot convert value to float");
	return std::monostate{};
}

inline Core::Value ConvertToBool(
	Execution::ExecutionContext& ctx,
	const Core::Value& value,
	const std::string& domain)
{
	if (std::holds_alternative<bool>(value))
	{
		return std::get<bool>(value);
	}
	if (std::holds_alternative<int64_t>(value))
	{
		return std::get<int64_t>(value) != 0;
	}
	if (std::holds_alternative<double>(value))
	{
		return std::get<double>(value) != 0.0;
	}
	if (std::holds_alternative<Core::StringPtr>(value) && std::get<Core::StringPtr>(value))
	{
		std::string lowered = *std::get<Core::StringPtr>(value);
		std::ranges::transform(lowered, lowered.begin(), [](const unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		if (lowered == "true" || lowered == "1")
		{
			return true;
		}
		if (lowered == "false" || lowered == "0")
		{
			return false;
		}
	}
	ctx.RaiseError(domain + ".to_bool cannot convert value to bool");
	return std::monostate{};
}

} // namespace VM::Runtime
