#include "Chunk.h"

#include <limits>

namespace VM::Execution
{

void Chunk::Write(Core::OpCode opcode)
{
	code.push_back(static_cast<uint8_t>(opcode));
}

void Chunk::WriteConstant(const Core::Value& value)
{
	Write(Core::OpCode::OP_CONSTANT);
	code.push_back(AddConstant(value));
}

void Chunk::WriteJump(const Core::OpCode opcode, const uint16_t offset)
{
	Write(opcode);
	code.push_back(static_cast<uint8_t>((offset >> 8) & 0xff));
	code.push_back(static_cast<uint8_t>(offset & 0xff));
}

uint8_t Chunk::AddConstant(const Core::Value& value)
{
	constants.push_back(value);
	if (constants.size() > std::numeric_limits<uint8_t>::max())
	{
		throw std::overflow_error("Too many constants in chunk");
	}
	return static_cast<uint8_t>(constants.size() - 1);
}

void Chunk::Clear()
{
	code.clear();
	constants.clear();
	debugName.clear();
}

} // namespace VM::Execution