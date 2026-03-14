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

	OP_EQUAL = 0x07,
	OP_GREATER = 0x08,
	OP_LESS = 0x09,
	OP_NOT = 0x0A,
	OP_GREATER_EQUAL = 0x0B,
	OP_LESS_EQUAL = 0x0C,
	OP_NOT_EQUAL = 0x0D,

	OP_JUMP = 0x10,
	OP_JUMP_IF_FALSE = 0x11,
	OP_JUMP_IF_TRUE = 0x12,
	OP_LOOP = 0x13,
	OP_POP = 0x14,

	OP_DEFINE_GLOBAL = 0x20,
	OP_GET_GLOBAL = 0x21,
	OP_SET_GLOBAL = 0x22,
	OP_GET_LOCAL = 0x23,
	OP_SET_LOCAL = 0x24,

	OP_CALL = 0x30,

	OP_MOD = 0x40,
	OP_DIV = 0x41,
	OP_AND = 0x42,
	OP_OR = 0x43,

	OP_RETURN = 0xFF,
};

constexpr std::string_view GetOpCodeName(const OpCode opcode) noexcept
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
	case OpCode::OP_DEFINE_GLOBAL:
		return "OP_DEFINE_GLOBAL";
	case OpCode::OP_GET_GLOBAL:
		return "OP_GET_GLOBAL";
	case OpCode::OP_SET_GLOBAL:
		return "OP_SET_GLOBAL";
	case OpCode::OP_GET_LOCAL:
		return "OP_GET_LOCAL";
	case OpCode::OP_SET_LOCAL:
		return "OP_SET_LOCAL";
	case OpCode::OP_JUMP:
		return "OP_JUMP";
	case OpCode::OP_JUMP_IF_FALSE:
		return "OP_JUMP_IF_FALSE";
	case OpCode::OP_EQUAL:
		return "OP_EQUAL";
	case OpCode::OP_GREATER:
		return "OP_GREATER";
	case OpCode::OP_LESS:
		return "OP_LESS";
	case OpCode::OP_NOT:
		return "OP_NOT";
	default:
		return "OP_UNKNOWN";
	}
}

constexpr bool OpCodeHasOperand(const OpCode opcode)
{
	using enum OpCode;
	switch (opcode)
	{
	case OP_CONSTANT:
	case OP_JUMP:
	case OP_JUMP_IF_FALSE:
	case OP_JUMP_IF_TRUE:
	case OP_DEFINE_GLOBAL:
	case OP_GET_GLOBAL:
	case OP_SET_GLOBAL:
	case OP_GET_LOCAL:
	case OP_SET_LOCAL:
	case OP_CALL:
	case OP_LOOP:
		return true;
	default:
		return false;
	}
}

} // namespace VM::Core