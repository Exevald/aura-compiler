#pragma once

#include <cstdint>
#include <string_view>

namespace VM::Core
{

enum class OperandSize
{
	None,
	Uint8,
	Uint16
};

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
	OP_GET_UPVALUE = 0x25,
	OP_SET_UPVALUE = 0x26,

	OP_CALL = 0x30,
	OP_CLOSURE = 0x31,
	OP_GET_MODULE_MEMBER = 0x32,

	OP_MOD = 0x40,
	OP_DIV = 0x41,
	OP_AND = 0x42,
	OP_OR = 0x43,

	OP_BUILD_ARRAY = 0x50,
	OP_INDEX_GET = 0x51,
	OP_INDEX_SET = 0x52,
	OP_BUILD_MAP = 0x5C,

	OP_BUILD_STRUCT = 0x53,
	OP_MEMBER_GET = 0x54,
	OP_MEMBER_SET = 0x55,
	OP_BUILD_ENUM = 0x56,
	OP_GET_ENUM_TAG = 0x57,
	OP_GET_ENUM_ARG = 0x58,
	OP_BUILD_ACTOR = 0x59,
	OP_ACTOR_SEND = 0x5A,
	OP_ACTOR_QUERY = 0x5B,

	OP_ADDR_LOCAL = 0x60,
	OP_ADDR_GLOBAL = 0x61,
	OP_ADDR_MEMBER = 0x62,
	OP_ADDR_UPVALUE = 0x65,

	OP_DEREF_GET = 0x63,
	OP_DEREF_SET = 0x64,

	OP_MAKE_ITER = 0x70,
	OP_ITER_NEXT = 0x71,
	OP_ITER_DROP = 0x72,
	OP_ITER_TAKE = 0x73,
	OP_ITER_REVERSE = 0x74,
	OP_ITER_FILTER = 0x75,
	OP_ITER_TRANSFORM = 0x76,

	OP_PRINT = 0x80,
	OP_BUILD_HANDLER = 0x81,
	OP_PUSH_HANDLER = 0x82,
	OP_POP_HANDLER = 0x83,
	OP_EFFECT_INVOKE = 0x84,
	OP_BEGIN_TXN = 0x85,
	OP_END_TXN = 0x86,
	OP_BUILD_ACTOR_METHODS = 0x87,
	OP_DUP = 0x88,
	OP_SWAP = 0x89,
	OP_ASSERT = 0x8A,

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
	case OpCode::OP_GET_UPVALUE:
		return "OP_GET_UPVALUE";
	case OpCode::OP_SET_UPVALUE:
		return "OP_SET_UPVALUE";
	case OpCode::OP_CLOSURE:
		return "OP_CLOSURE";
	case OpCode::OP_GET_MODULE_MEMBER:
		return "OP_GET_MODULE_MEMBER";
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
	case OpCode::OP_BUILD_ARRAY:
		return "OP_BUILD_ARRAY";
	case OpCode::OP_BUILD_MAP:
		return "OP_BUILD_MAP";
	case OpCode::OP_INDEX_GET:
		return "OP_INDEX_GET";
	case OpCode::OP_BUILD_STRUCT:
		return "OP_BUILD_STRUCT";
	case OpCode::OP_MEMBER_GET:
		return "OP_MEMBER_GET";
	case OpCode::OP_ADDR_GLOBAL:
		return "OP_ADDR_GLOBAL";
	case OpCode::OP_ADDR_UPVALUE:
		return "OP_ADDR_UPVALUE";
	case OpCode::OP_DEREF_SET:
		return "OP_DEREF_SET";
	case OpCode::OP_DEREF_GET:
		return "OP_DEREF_GET";
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
	case OP_CLOSURE:
	case OP_GET_MODULE_MEMBER:
	case OP_ADDR_GLOBAL:
	case OP_BUILD_ACTOR:
	case OP_ACTOR_SEND:
	case OP_ACTOR_QUERY:
	case OP_EFFECT_INVOKE:
	case OP_ASSERT:
		return true;

	case OP_GET_LOCAL:
	case OP_SET_LOCAL:
	case OP_GET_UPVALUE:
	case OP_SET_UPVALUE:
	case OP_CALL:
	case OP_LOOP:
	case OP_BUILD_ARRAY:
	case OP_BUILD_MAP:
	case OP_BUILD_STRUCT:
	case OP_MEMBER_GET:
	case OP_MEMBER_SET:
	case OP_BUILD_ENUM:
	case OP_GET_ENUM_ARG:
	case OP_ADDR_LOCAL:
	case OP_ITER_NEXT:
	case OP_ADDR_UPVALUE:
	case OP_BUILD_HANDLER:
	case OP_BUILD_ACTOR_METHODS:
		return true;
	default:
		return false;
	}
}

constexpr OperandSize GetOperandSize(const OpCode opcode)
{
	using enum OpCode;
	switch (opcode)
	{
	case OP_JUMP:
	case OP_JUMP_IF_FALSE:
	case OP_JUMP_IF_TRUE:
	case OP_LOOP:
	case OP_CONSTANT:
	case OP_DEFINE_GLOBAL:
	case OP_GET_GLOBAL:
	case OP_SET_GLOBAL:
	case OP_CLOSURE:
	case OP_GET_MODULE_MEMBER:
	case OP_ADDR_GLOBAL:
	case OP_BUILD_ACTOR:
	case OP_ACTOR_SEND:
	case OP_ACTOR_QUERY:
	case OP_EFFECT_INVOKE:
	case OP_ASSERT:
		return OperandSize::Uint16;

	case OP_GET_LOCAL:
	case OP_SET_LOCAL:
	case OP_GET_UPVALUE:
	case OP_SET_UPVALUE:
	case OP_CALL:
	case OP_BUILD_ARRAY:
	case OP_BUILD_MAP:
	case OP_BUILD_STRUCT:
	case OP_MEMBER_GET:
	case OP_MEMBER_SET:
	case OP_BUILD_ENUM:
	case OP_GET_ENUM_ARG:
	case OP_ADDR_LOCAL:
	case OP_ADDR_UPVALUE:
	case OP_ITER_NEXT:
	case OP_BUILD_HANDLER:
	case OP_BUILD_ACTOR_METHODS:
		return OperandSize::Uint8;

	default:
		return OperandSize::None;
	}
}

} // namespace VM::Core
