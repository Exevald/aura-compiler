#pragma once

#include "../core/values/Value.h"
#include "../core/values/ValueHelper.h"
#include "ExecutionContext.h"

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

inline Core::ModulePtr MakeModule(const std::string& name)
{
	auto module = std::make_shared<Core::Module>();
	module->name = name;
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
		const bool right = std::get<bool>(rhs);
		if (left == right)
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

} // namespace VM::Runtime
