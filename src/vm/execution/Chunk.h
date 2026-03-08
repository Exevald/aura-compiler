#pragma once

#include "../core/Instruction.h"
#include "../core/Value.h"

#include <functional>
#include <memory>

namespace VM::Execution
{

struct Chunk
{
	std::vector<uint8_t> code;
	std::vector<Core::Value> constants;
	std::string debugName;

	void Write(Core::OpCode opcode);
	void WriteConstant(const Core::Value& value);
	uint8_t AddConstant(const Core::Value& value);
	[[nodiscard]] size_t GetCodeSize() const { return code.size(); }
	[[nodiscard]] const std::vector<uint8_t>& GetCode() const { return code; }
	[[nodiscard]] const std::vector<Core::Value>& GetConstants() const { return constants; }
	void Clear();
};

} // namespace VM::Execution