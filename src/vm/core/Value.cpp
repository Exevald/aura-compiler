#include "Value.h"

#include <sstream>
#include <stdexcept>

namespace VM::Core
{

Value ValueHelper::PerformUnaryLogic(const Value& val)
{
	return !As<bool>(val);
}

std::string_view ValueHelper::GetTypeName(const Value& val) noexcept
{
	return std::visit([](auto&& v) -> std::string_view {
		using T = std::decay_t<decltype(v)>;
		if constexpr (std::is_same_v<T, std::monostate>)
			return "void";
		else if constexpr (std::is_same_v<T, bool>)
			return "bool";
		else if constexpr (std::is_same_v<T, int64_t>)
			return "int64";
		else if constexpr (std::is_same_v<T, double>)
			return "float64";
		else
			return "unknown";
	},
		val);
}

void ValueHelper::PrintValue(const Value& val, std::ostream& out)
{
	std::visit([&](auto&& v) {
		using T = std::decay_t<decltype(v)>;
		if constexpr (std::is_same_v<T, std::monostate>)
		{
			out << "null";
		}
		else if constexpr (std::is_same_v<T, bool>)
		{
			out << (v ? "true" : "false");
		}
		else
		{
			out << v;
		}
	},
		val);
}

std::string ValueHelper::ToString(const Value& val)
{
	std::ostringstream oss;
	PrintValue(val, oss);
	return oss.str();
}

Value ValueHelper::Add(const Value& lhs, const Value& rhs)
{
	return PerformBinaryArithmetic(lhs, rhs, std::plus<double>{});
}

Value ValueHelper::Subtract(const Value& lhs, const Value& rhs)
{
	return PerformBinaryArithmetic(lhs, rhs, std::minus<double>{});
}

Value ValueHelper::Multiply(const Value& lhs, const Value& rhs)
{
	return PerformBinaryArithmetic(lhs, rhs, std::multiplies<double>{});
}

Value ValueHelper::Divide(const Value& lhs, const Value& rhs)
{
	return PerformBinaryArithmetic(lhs, rhs, [](double a, double b) {
		if (b == 0.0)
		{
			throw std::runtime_error("Division by zero");
		}
		return a / b;
	});
}

Value ValueHelper::Negate(const Value& val)
{
	return PerformUnaryArithmetic(val, [](double x) { return -x; });
}

} // namespace VM::Core