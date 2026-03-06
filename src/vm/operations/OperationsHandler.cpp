#include "OperationsHandler.h"

namespace VM::Operations
{

OperationRegistry& OperationRegistry::Instance()
{
	static OperationRegistry instance;
	return instance;
}

bool OperationRegistry::Register(Core::OpCode opcode, HandlerPtr handler)
{
	if (Has(opcode))
	{
		return false;
	}
	m_handlers[opcode] = std::move(handler);
	return true;
}

IOperationHandler* OperationRegistry::Get(Core::OpCode opcode) const
{
	auto it = m_handlers.find(opcode);
	return (it != m_handlers.end()) ? it->second.get() : nullptr;
}

bool OperationRegistry::Has(Core::OpCode opcode) const
{
	return m_handlers.count(opcode) > 0;
}

void RegisterBuiltInOperations()
{
	using namespace Core;

	auto& reg = OperationRegistry::Instance();

	reg.Register(OpCode::OP_ADD, std::make_unique<BinaryArithmeticHandler>(OpCode::OP_ADD, "ADD", [](const Value& l, const Value& r) {
		return ValueHelper::PerformBinaryArithmetic(l, r, std::plus<double>{});
	}));

	reg.Register(OpCode::OP_SUBTRACT, std::make_unique<BinaryArithmeticHandler>(OpCode::OP_SUBTRACT, "SUB", [](const Value& l, const Value& r) {
		return ValueHelper::PerformBinaryArithmetic(l, r, std::minus<double>{});
	}));

	reg.Register(OpCode::OP_MULTIPLY, std::make_unique<BinaryArithmeticHandler>(OpCode::OP_MULTIPLY, "MUL", [](const Value& l, const Value& r) {
		return ValueHelper::PerformBinaryArithmetic(l, r, std::multiplies<double>{});
	}));

	reg.Register(OpCode::OP_DIVIDE, std::make_unique<BinaryArithmeticHandler>(OpCode::OP_DIVIDE, "DIV", [](const Value& l, const Value& r) {
		double rv = ValueHelper::As<double>(r);
		if (rv == 0.0)
		{
			throw std::runtime_error("Division by zero");
		}
		return ValueHelper::PerformBinaryArithmetic(l, r, std::divides<double>{});
	}));

	reg.Register(OpCode::OP_NEGATE, std::make_unique<UnaryHandler>(OpCode::OP_NEGATE, "NEG", [](const Value& v) {
		return ValueHelper::PerformUnaryArithmetic(v, [](double x) { return -x; });
	}));
}

} // namespace VM::Operations