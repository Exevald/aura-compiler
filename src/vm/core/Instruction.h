#pragma once

#include "OpCode.h"

#include <cstdint>

namespace VM::Core {

struct Instruction {
	OpCode opcode;
	uint8_t operand;

	constexpr explicit Instruction(OpCode op, uint8_t arg = 0)
		: opcode(op), operand(arg) {}
};

} // namespace VM::Core