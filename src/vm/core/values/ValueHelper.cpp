#include "ValueHelper.h"
#include "../../execution/chunk/Chunk.h"
#include "Value.h"

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
		else if constexpr (std::is_same_v<T, ClosurePtr>)
		{
			return "closure";
		}
		else if constexpr (std::is_same_v<T, NativeFunctionPtr>)
		{
			return "native_function";
		}
		else if constexpr (std::is_same_v<T, ModulePtr>)
		{
			return "module";
		}
		else if constexpr (std::is_same_v<T, ThreadPtr>)
		{
			return "thread";
		}
		else if constexpr (std::is_same_v<T, MutexPtr>)
		{
			return "mutex";
		}
		else if constexpr (std::is_same_v<T, TaskPtr>)
		{
			return "task";
		}
		else if constexpr (std::is_same_v<T, ChannelPtr>)
		{
			return "channel";
		}
		else if constexpr (std::is_same_v<T, ContextPtr>)
		{
			return "context";
		}
		else if constexpr (std::is_same_v<T, ListenerPtr>)
		{
			return "listener";
		}
		else if constexpr (std::is_same_v<T, ConnectionPtr>)
		{
			return "connection";
		}
		else if constexpr (std::is_same_v<T, HttpRequestPtr>)
		{
			return "http_request";
		}
		else if constexpr (std::is_same_v<T, StringMapPtr>)
		{
			return "string_store";
		}
		else if constexpr (std::is_same_v<T, MapPtr>)
		{
			return "map";
		}
		else if constexpr (std::is_same_v<T, MySqlConnectionPtr>)
		{
			return "mysql_connection";
		}
		else if constexpr (std::is_same_v<T, MySqlPoolPtr>)
		{
			return "mysql_pool";
		}
		else if constexpr (std::is_same_v<T, MySqlStatementPtr>)
		{
			return "mysql_statement";
		}
		else if constexpr (std::is_same_v<T, MySqlResultPtr>)
		{
			return "mysql_result";
		}
		else if constexpr (std::is_same_v<T, MySqlRowPtr>)
		{
			return "mysql_row";
		}
		else if constexpr (std::is_same_v<T, RabbitMqConnectionPtr>)
		{
			return "rabbitmq_connection";
		}
		else if constexpr (std::is_same_v<T, RabbitMqMessagePtr>)
		{
			return "rabbitmq_message";
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
		else if constexpr (std::is_same_v<T, ClosurePtr>)
		{
			out << "<closure " << ((v && v->function) ? v->function->name : "anonymous") << ">";
		}
		else if constexpr (std::is_same_v<T, NativeFunctionPtr>)
		{
			out << "<native " << (v ? v->name : "anonymous") << ">";
		}
		else if constexpr (std::is_same_v<T, std::monostate>)
		{
			out << "null";
		}
		else if constexpr (std::is_same_v<T, bool>)
		{
			out << (v ? "true" : "false");
		}
		else if constexpr (std::is_same_v<T, ArrayPtr>)
		{
			out << "[";
			for (size_t i = 0; i < v->elements.size(); ++i)
			{
				PrintValue(v->elements[i], out);
				if (i < v->elements.size() - 1)
				{
					out << ", ";
				}
			}
			out << "]";
		}
		else if constexpr (std::is_same_v<T, InstancePtr>)
		{
			out << "{struct}";
		}
		else if constexpr (std::is_same_v<T, EnumPtr>)
		{
			out << "variant(" << v->tag << ")";
		}
		else if constexpr (std::is_same_v<T, PointerPtr>)
		{
			out << (v ? v->targetName : "nullptr");
		}
		else if constexpr (std::is_same_v<T, ModulePtr>)
		{
			out << "<module " << (v ? v->name : "anonymous") << ">";
		}
		else if constexpr (std::is_same_v<T, ThreadPtr>)
		{
			out << "<thread " << (v ? std::to_string(v->id) : "null") << ">";
		}
		else if constexpr (std::is_same_v<T, MutexPtr>)
		{
			out << "<mutex " << (v ? std::to_string(v->id) : "null") << ">";
		}
		else if constexpr (std::is_same_v<T, TaskPtr>)
		{
			out << "<task " << (v ? std::to_string(v->id) : "null") << ">";
		}
		else if constexpr (std::is_same_v<T, ChannelPtr>)
		{
			out << "<channel " << (v ? std::to_string(v->id) : "null") << ">";
		}
		else if constexpr (std::is_same_v<T, ContextPtr>)
		{
			out << "<context " << (v ? std::to_string(v->id) : "null") << ">";
		}
		else if constexpr (std::is_same_v<T, ListenerPtr>)
		{
			out << "<listener " << (v ? std::to_string(v->port) : "null") << ">";
		}
		else if constexpr (std::is_same_v<T, ConnectionPtr>)
		{
			out << "<connection " << (v ? std::to_string(v->port) : "null") << ">";
		}
		else if constexpr (std::is_same_v<T, HttpRequestPtr>)
		{
			out << "<http_request " << (v ? v->method + " " + v->path : "null") << ">";
		}
		else if constexpr (std::is_same_v<T, StringMapPtr>)
		{
			out << "<string_store " << (v ? std::to_string(v->values.size()) : "null") << ">";
		}
		else if constexpr (std::is_same_v<T, MapPtr>)
		{
			out << "<map " << (v ? std::to_string(v->entries.size()) : "null") << ">";
		}
		else if constexpr (std::is_same_v<T, MySqlConnectionPtr>)
		{
			out << "<mysql_connection " << (v && v->connection ? "open" : "closed") << ">";
		}
		else if constexpr (std::is_same_v<T, MySqlPoolPtr>)
		{
			out << "<mysql_pool " << (v ? std::to_string(v->allConnections.size()) : "null") << ">";
		}
		else if constexpr (std::is_same_v<T, MySqlStatementPtr>)
		{
			out << "<mysql_statement " << (v && v->statement ? "ready" : "closed") << ">";
		}
		else if constexpr (std::is_same_v<T, MySqlResultPtr>)
		{
			out << "<mysql_result " << (v ? std::to_string(v->rows.size()) : "null") << ">";
		}
		else if constexpr (std::is_same_v<T, MySqlRowPtr>)
		{
			out << "<mysql_row " << (v ? std::to_string(v->values.size()) : "null") << ">";
		}
		else if constexpr (std::is_same_v<T, RabbitMqConnectionPtr>)
		{
			out << "<rabbitmq_connection " << (v && v->open ? "open" : "closed") << ">";
		}
		else if constexpr (std::is_same_v<T, RabbitMqMessagePtr>)
		{
			out << "<rabbitmq_message " << (v ? std::to_string(v->deliveryTag) : "null") << ">";
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

			if constexpr (std::is_same_v<Type1, ClosurePtr>)
			{
				return a == b;
			}

			if constexpr (std::is_same_v<Type1, NativeFunctionPtr>)
			{
				return a == b;
			}

			if constexpr (std::is_same_v<Type1, ThreadPtr>)
			{
				return a == b || (a && b && a->id == b->id);
			}

			if constexpr (std::is_same_v<Type1, MutexPtr>)
			{
				return a == b || (a && b && a->id == b->id);
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
