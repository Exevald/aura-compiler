#include "Value.h"
#include "../execution/Chunk.h"

#include <sstream>
#include <stdexcept>

namespace VM::Core
{

Function::Function()
	: chunk(std::make_unique<Execution::Chunk>())
{
}

Function::~Function() = default;

Value ValueHelper::PerformUnaryLogic(const Value& val)
{
	return !As<bool>(val);
}

std::string_view ValueHelper::GetTypeName(const Value& val) noexcept
{
	return std::visit([]<typename T0>(T0&&) -> std::string_view {
		using T = std::decay_t<T0>;
		if constexpr (std::is_same_v<T, std::monostate>)
		{
			return "void";
		}
		else if constexpr (std::is_same_v<T, bool>)
		{
			return "bool";
		}
		else if constexpr (std::is_same_v<T, int64_t>)
		{
			return "int64";
		}
		else if constexpr (std::is_same_v<T, double>)
		{
			return "float64";
		}
		else if constexpr (std::is_same_v<T, StringPtr>)
		{
			return "string";
		}
		else if constexpr (std::is_same_v<T, FunctionPtr>)
		{
			return "function";
		}
		else
		{
			return "unknown";
		}
	},
		val);
}

void ValueHelper::PrintValue(const Value& val, std::ostream& out)
{
	std::visit([&]<typename T0>(T0&& v) {
		using T = std::decay_t<T0>;
		if constexpr (std::is_same_v<T, StringPtr>)
		{
			out << (v ? *v : "null");
		}
		else if constexpr (std::is_same_v<T, FunctionPtr>)
		{
			out << "<fn " << (v ? v->name : "anonymous") << ">";
		}
		else if constexpr (std::is_same_v<T, std::monostate>)
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
	if (IsString(lhs) || IsString(rhs))
	{
		return std::make_shared<const std::string>(ToString(lhs) + ToString(rhs));
	}
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
	return PerformBinaryArithmetic(lhs, rhs, [](const double a, const double b) {
		if (b == 0.0)
		{
			throw std::runtime_error("Division by zero");
		}
		return a / b;
	});
}

Value ValueHelper::DivideInt(const Value& lhs, const Value& rhs)
{
	return std::visit([]<typename T1, typename T2>(T1 a, T2 b) -> Value {
		if constexpr (std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>)
		{
			if (static_cast<int64_t>(b) == 0)
			{
				throw std::runtime_error("Division by zero");
			}
			return static_cast<int64_t>(a) / static_cast<int64_t>(b);
		}
		throw std::runtime_error("Integer division requires numeric types");
	},
		lhs, rhs);
}

Value ValueHelper::Modulo(const Value& lhs, const Value& rhs)
{
	return std::visit([]<typename T1, typename T2>(T1 a, T2 b) -> Value {
		if constexpr (std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>)
		{
			if (static_cast<int64_t>(b) == 0)
			{
				throw std::runtime_error("Modulo by zero");
			}
			return static_cast<int64_t>(a) % static_cast<int64_t>(b);
		}
		throw std::runtime_error("Modulo requires numeric types");
	},
		lhs, rhs);
}

Value ValueHelper::Negate(const Value& val)
{
	return PerformUnaryArithmetic(val, [](double x) { return -x; });
}

bool ValueHelper::Equal(const Value& lhs, const Value& rhs)
{
	return std::visit([&]<typename T1, typename T2>(const T1& a, const T2& b) -> bool {
		using Type1 = std::decay_t<T1>;
		using Type2 = std::decay_t<T2>;

		if constexpr (std::is_same_v<Type1, Type2>)
		{
			if constexpr (std::is_same_v<Type1, std::monostate>)
			{
				return true;
			}

			if constexpr (std::is_same_v<Type1, StringPtr>)
			{
				if (!a || !b)
				{
					return a == b;
				}
				return *a == *b;
			}

			if constexpr (std::is_same_v<Type1, FunctionPtr>)
			{
				return a == b;
			}

			return a == b;
		}
		else if constexpr (std::is_arithmetic_v<Type1> && std::is_arithmetic_v<Type2>)
		{
			return static_cast<double>(a) == static_cast<double>(b);
		}
		else
		{
			return false;
		}
	},
		lhs, rhs);
}

Value ValueHelper::Greater(const Value& lhs, const Value& rhs)
{
	return PerformBinaryComparison(lhs, rhs, std::greater<double>{});
}

Value ValueHelper::Less(const Value& lhs, const Value& rhs)
{
	return PerformBinaryComparison(lhs, rhs, std::less<double>{});
}

} // namespace VM::Core