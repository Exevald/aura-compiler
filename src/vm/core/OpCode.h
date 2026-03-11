#pragma once

#include <cstdint>
#include <string_view>

namespace VM::Core
{

enum class OpCode : uint8_t
{
	OP_CONSTANT = 0x01,
	OP_NEGATE = 0x02,
	OP_ADD = 0x03,
	OP_SUBTRACT = 0x04,
	OP_MULTIPLY = 0x05,
	OP_DIVIDE = 0x06,
	OP_RETURN = 0xFF,

	OP_JUMP = 0x10,
	OP_JUMP_IF_FALSE = 0x11,
};

constexpr std::string_view GetOpCodeName(OpCode opcode) noexcept
{
	switch (opcode)
	{
	case OpCode::OP_CONSTANT:
		return "OP_CONSTANT";
	case OpCode::OP_NEGATE:
		return "OP_NEGATE";
	case OpCode::OP_ADD:
		return "OP_ADD";
	case OpCode::OP_SUBTRACT:
		return "OP_SUBTRACT";
	case OpCode::OP_MULTIPLY:
		return "OP_MULTIPLY";
	case OpCode::OP_DIVIDE:
		return "OP_DIVIDE";
	case OpCode::OP_RETURN:
		return "OP_RETURN";
	case OpCode::OP_JUMP:
		return "OP_JUMP";
	case OpCode::OP_JUMP_IF_FALSE:
		return "OP_JUMP_IF_FALSE";
	default:
		return "OP_UNKNOWN";
	}
}

constexpr bool OpCodeHasOperand(OpCode opcode)
{
	using enum OpCode;
	switch (opcode)
	{
	case OP_CONSTANT:
	case OP_JUMP:
	case OP_JUMP_IF_FALSE:
		return true;
	default:
		return false;
	}
}

} // namespace VM::Core