#include "Chunk.h"

#include <limits>

namespace VM::Execution
{

namespace
{

bool ConstantEquals(const Core::Value& lhs, const Core::Value& rhs)
{
	if (lhs.index() != rhs.index())
	{
		return false;
	}

	if (std::holds_alternative<std::monostate>(lhs))
	{
		return true;
	}
	if (std::holds_alternative<bool>(lhs))
	{
		return std::get<bool>(lhs) == std::get<bool>(rhs);
	}
	if (std::holds_alternative<int64_t>(lhs))
	{
		return std::get<int64_t>(lhs) == std::get<int64_t>(rhs);
	}
	if (std::holds_alternative<double>(lhs))
	{
		return std::get<double>(lhs) == std::get<double>(rhs);
	}
	if (std::holds_alternative<Core::StringPtr>(lhs))
	{
		const auto& left = std::get<Core::StringPtr>(lhs);
		const auto& right = std::get<Core::StringPtr>(rhs);
		if (!left || !right)
		{
			return left == right;
		}
		return *left == *right;
	}

	return false;
}

} // namespace

void Chunk::Write(Core::OpCode opcode)
{
	code.push_back(static_cast<uint8_t>(opcode));
}

void Chunk::WriteOperand(const Core::OpCode opcode, const uint16_t operand)
{
	switch (Core::GetOperandSize(opcode))
	{
	case Core::OperandSize::Uint8:
		code.push_back(static_cast<uint8_t>(operand & 0xff));
		break;
	case Core::OperandSize::Uint16:
		code.push_back(static_cast<uint8_t>((operand >> 8) & 0xff));
		code.push_back(static_cast<uint8_t>(operand & 0xff));
		break;
	case Core::OperandSize::None:
		break;
	}
}

void Chunk::WriteConstant(const Core::Value& value)
{
	const uint16_t constantIndex = AddConstant(value);
	Write(Core::OpCode::OP_CONSTANT);
	WriteOperand(Core::OpCode::OP_CONSTANT, constantIndex);
}

void Chunk::WriteJump(const Core::OpCode opcode, const uint16_t offset)
{
	Write(opcode);
	code.push_back(static_cast<uint8_t>((offset >> 8) & 0xff));
	code.push_back(static_cast<uint8_t>(offset & 0xff));
}

uint16_t Chunk::AddConstant(const Core::Value& value)
{
	for (size_t i = 0; i < constants.size(); ++i)
	{
		if (ConstantEquals(constants[i], value))
		{
			return static_cast<uint16_t>(i);
		}
	}

	constants.push_back(value);
	if (constants.size() > std::numeric_limits<uint16_t>::max() + 1ull)
	{
		throw std::overflow_error("Too many constants in chunk");
	}
	return static_cast<uint16_t>(constants.size() - 1);
}

void Chunk::Clear()
{
	code.clear();
	constants.clear();
	debugName.clear();
}

} // namespace VM::Execution
