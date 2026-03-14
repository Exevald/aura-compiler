#pragma once

#include "Value.h"

namespace VM::Core
{
class ValueHelper
{
public:
	template <typename Op>
	static Value PerformBinaryArithmetic(const Value& lhs, const Value& rhs, Op operation);

	template <typename Op>
	static Value PerformBinaryLogic(const Value& lhs, const Value& rhs, Op operation);

	template <typename Op>
	static Value PerformBinaryComparison(const Value& lhs, const Value& rhs, Op operation);

	template <typename Op>
	static Value PerformUnaryArithmetic(const Value& val, Op operation);

	static Value PerformUnaryLogic(const Value& val);

	template <typename T>
	static constexpr bool IsType(const Value& val) noexcept;

	static std::string_view GetTypeName(const Value& val) noexcept;

	static bool IsString(const Value& v) { return std::holds_alternative<StringPtr>(v); }

	template <typename T>
	static T As(const Value& val);

	static void PrintValue(const Value& val, std::ostream& out);
	static std::string ToString(const Value& val);

	static Value Add(const Value& lhs, const Value& rhs);
	static Value Subtract(const Value& lhs, const Value& rhs);
	static Value Multiply(const Value& lhs, const Value& rhs);
	static Value Divide(const Value& lhs, const Value& rhs);
	static Value DivideInt(const Value& lhs, const Value& rhs);
	static Value Modulo(const Value& lhs, const Value& rhs);
	static Value Negate(const Value& val);

	static bool Equal(const Value& lhs, const Value& rhs);
	static Value Greater(const Value& lhs, const Value& rhs);
	static Value Less(const Value& lhs, const Value& rhs);
};

} // namespace VM::Core

namespace VM::Core
{

template <typename T>
constexpr bool ValueHelper::IsType(const Value& val) noexcept
{
	return std::holds_alternative<T>(val);
}

template <typename Op>
Value ValueHelper::PerformBinaryArithmetic(const Value& lhs, const Value& rhs, Op operation)
{
	return std::visit([&]<typename T0, typename T1>(T0&& l, T1&& r) -> Value {
		using L = std::decay_t<T0>;
		using R = std::decay_t<T1>;

		if constexpr (PrimitiveValue<L> && PrimitiveValue<R>)
		{
			auto lv = static_cast<double>(l);
			auto rv = static_cast<double>(r);
			return operation(lv, rv);
		}
		else
		{
			throw std::runtime_error(
				"Arithmetic type mismatch: "
				+ std::string(GetTypeName(lhs))
				+ " and " + std::string(GetTypeName(rhs)));
		}
	},
		lhs, rhs);
}

template <typename Op>
Value ValueHelper::PerformBinaryLogic(const Value& lhs, const Value& rhs, Op operation)
{
	bool l = As<bool>(lhs);
	bool r = As<bool>(rhs);
	return operation(l, r);
}

template <typename Op>
Value ValueHelper::PerformBinaryComparison(const Value& lhs, const Value& rhs, Op operation)
{
	return std::visit([&]<typename T0, typename T1>(T0&& l, T1&& r) -> Value {
		using L = std::decay_t<T0>;
		using R = std::decay_t<T1>;

		if constexpr (std::is_same_v<L, R>)
		{
			if constexpr (std::is_invocable_v<Op, L, R>)
			{
				return operation(l, r);
			}
		}
		else if constexpr (PrimitiveValue<L> && PrimitiveValue<R>)
		{
			if constexpr (std::is_invocable_v<Op, double, double>)
			{
				return operation(static_cast<double>(l), static_cast<double>(r));
			}
		}
		return false;
	},
		lhs, rhs);
}

template <typename Op>
Value ValueHelper::PerformUnaryArithmetic(const Value& val, Op operation)
{
	return std::visit([&]<typename T0>(T0&& v) -> Value {
		using T = std::decay_t<T0>;
		if constexpr (PrimitiveValue<T>)
		{
			return operation(static_cast<double>(v));
		}
		else
		{
			throw std::runtime_error(
				"Unary arithmetic not supported for type: " + std::string(GetTypeName(val)));
		}
	},
		val);
}

template <typename T>
T ValueHelper::As(const Value& val)
{
	if (std::holds_alternative<T>(val))
	{
		return std::get<T>(val);
	}

	if constexpr (std::is_arithmetic_v<T>)
	{
		return std::visit([&]<typename T0>(T0&& v) -> T {
			using V = std::decay_t<T0>;
			if constexpr (std::is_arithmetic_v<V>)
			{
				return static_cast<T>(v);
			}
			else if constexpr (std::is_same_v<V, bool>)
			{
				return static_cast<T>(v ? 1 : 0);
			}
			else
			{
				throw std::bad_variant_access();
			}
		},
			val);
	}

	if constexpr (std::is_same_v<T, bool>)
	{
		return std::visit([&]<typename T0>(T0&& v) -> bool {
			using V = std::decay_t<T0>;
			if constexpr (std::is_arithmetic_v<V>)
			{
				return v != 0;
			}
			return true;
		},
			val);
	}

	throw std::bad_variant_access();
}

} // namespace VM::Core