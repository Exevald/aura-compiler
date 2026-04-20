#include "VirtualMachine.h"
#include "common/VirtualMachineRuntimeSupport.h"

#include <iostream>

namespace VM::Execution
{

using Core::Value;
using Detail::Fail;
using Detail::ReadErrorMessage;
using Detail::ReadStringConstant;
using Detail::RuntimeError;

void VirtualMachine::DebugPrintInstruction(const Core::Instruction& instr) const
{
	if (!m_context.HasFrames())
	{
		return;
	}

	std::cout << "[" << (m_context.CurrentFrame().ip - 1) << "] "
			  << Core::GetOpCodeName(instr.opcode) << " ";

	if (Core::OpCodeHasOperand(instr.opcode))
	{
		std::cout << "arg=" << static_cast<int>(instr.operand);
	}
	std::cout << "\n";
}

Core::Instruction VirtualMachine::DecodeInstruction()
{
	return Core::Instruction(Core::OpCode::OP_RETURN);
}

} // namespace VM::Execution
