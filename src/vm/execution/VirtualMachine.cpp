#include "VirtualMachine.h"

#include <format>
#include <iostream>
#include <stdexcept>

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

VirtualMachine::VirtualMachine() = default;

bool VirtualMachine::Interpret(Chunk* chunk)
{
	if (!chunk)
	{
		std::cerr << "Error: null chunk passed to Interpret\n";
		return false;
	}

	m_currentChunk = chunk;
	m_context.ClearStack();
	m_context.ClearError();
	m_ip = 0;
	m_stepsExecuted = 0;

	auto result = Run();

	if (result != ExecutionResult::Success)
	{
		if (m_context.HasError())
		{
			std::cerr << std::format("VM Error: {}\n", m_context.GetError());
		}
		else
		{
			std::cerr << "VM Execution failed\n";
		}
		return false;
	}

	return true;
}

void VirtualMachine::RegisterExtension(Core::OpCode opcode, ExtensionHandler handler)
{
	m_extensions[opcode] = std::move(handler);
}

ExecutionResult VirtualMachine::Run()
{
	for (;;)
	{
		if (++m_stepsExecuted > m_maxSteps)
		{
			return ExecutionResult::Timeout;
		}

		if (m_ip >= m_currentChunk->GetCodeSize())
		{
			return ExecutionResult::Success;
		}

		Core::Instruction instr = DecodeInstruction();

		if (m_debugMode)
		{
			DebugPrintInstruction(instr);
		}

		int result = Dispatch(instr);

		if (result < 0)
		{
			return ExecutionResult::RuntimeError;
		}

		if (result > 0)
		{
			m_ip = static_cast<size_t>(result);
		}

		if (instr.opcode == Core::OpCode::OP_RETURN)
		{
			return ExecutionResult::Success;
		}
	}
}

Core::Instruction VirtualMachine::DecodeInstruction()
{
	if (m_ip >= m_currentChunk->GetCodeSize())
	{
		return Core::Instruction{ Core::OpCode::OP_RETURN, 0 };
	}

	uint8_t opcode_byte = m_currentChunk->GetCode()[m_ip++];
	auto opcode = static_cast<Core::OpCode>(opcode_byte);

	uint8_t operand = 0;
	if (opcode == Core::OpCode::OP_CONSTANT)
	{
		if (m_ip < m_currentChunk->GetCodeSize())
		{
			operand = m_currentChunk->GetCode()[m_ip++];
		}
	}

	return Core::Instruction{ opcode, operand };
}

int VirtualMachine::Dispatch(const Core::Instruction& instr)
{
	using enum Core::OpCode;

	if (auto it = m_extensions.find(instr.opcode); it != m_extensions.end())
	{
		return it->second(instr.opcode, m_context);
	}

	switch (instr.opcode)
	{
	case OP_RETURN: {
		if (!m_context.StackEmpty())
		{
			Core::Value result = m_context.PopValue();
			std::cout << "Result: ";
			Core::ValueHelper::PrintValue(result, std::cout);
			std::cout << "\n";
		}
		else
		{
			std::cout << "Result: null\n";
		}
		return 0;
	}

	case OP_CONSTANT: {
		uint8_t index = instr.operand;
		if (index >= m_currentChunk->GetConstants().size())
		{
			m_context.RaiseError(std::format("Constant index {} out of bounds (max: {})",
				index, m_currentChunk->GetConstants().size()));
			return -1;
		}

		m_context.PushValue(m_currentChunk->GetConstants()[index]);
		return 0;
	}

	case OP_NEGATE: {
		if (m_context.StackSize() < 1)
		{
			m_context.RaiseError("Stack underflow in OP_NEGATE");
			return -1;
		}
		Core::Value operand = m_context.PopValue();
		m_context.PushValue(Core::ValueHelper::Negate(operand));
		return 0;
	}

	case OP_ADD: {
		if (m_context.StackSize() < 2)
		{
			m_context.RaiseError("Stack underflow in OP_ADD");
			return -1;
		}
		Core::Value b = m_context.PopValue();
		Core::Value a = m_context.PopValue();
		m_context.PushValue(Core::ValueHelper::Add(a, b));
		return 0;
	}

	case OP_SUBTRACT: {
		if (m_context.StackSize() < 2)
		{
			m_context.RaiseError("Stack underflow in OP_SUBTRACT");
			return -1;
		}
		Core::Value b = m_context.PopValue();
		Core::Value a = m_context.PopValue();
		m_context.PushValue(Core::ValueHelper::Subtract(a, b));
		return 0;
	}

	case OP_MULTIPLY: {
		if (m_context.StackSize() < 2)
		{
			m_context.RaiseError("Stack underflow in OP_MULTIPLY");
			return -1;
		}
		Core::Value b = m_context.PopValue();
		Core::Value a = m_context.PopValue();
		m_context.PushValue(Core::ValueHelper::Multiply(a, b));
		return 0;
	}

	case OP_DIVIDE: {
		if (m_context.StackSize() < 2)
		{
			m_context.RaiseError("Stack underflow in OP_DIVIDE");
			return -1;
		}
		Core::Value b = m_context.PopValue();
		Core::Value a = m_context.PopValue();
		m_context.PushValue(Core::ValueHelper::Divide(a, b));
		return 0;
	}

	case OP_JUMP: {
		return static_cast<int>(m_ip + instr.operand);
	}

	case OP_JUMP_IF_FALSE: {
		if (m_context.StackSize() < 1)
		{
			m_context.RaiseError("Stack underflow in OP_JUMP_IF_FALSE");
			return -1;
		}
		Core::Value cond = m_context.PopValue();
		if (!Core::ValueHelper::As<bool>(cond))
		{
			return static_cast<int>(m_ip + instr.operand);
		}
		return 0;
	}

	default: {
		m_context.RaiseError(std::format("Unknown opcode: 0x{:02X}",
			static_cast<uint8_t>(instr.opcode)));
		return -1;
	}
	}
}

void VirtualMachine::DebugPrintInstruction(const Core::Instruction& instr) const
{
	std::cout << std::format("[{:04}] {:<15} ",
		m_ip - 1,
		Core::GetOpCodeName(instr.opcode));

	if (instr.operand != 0)
	{
		std::cout << std::format("arg={}", instr.operand);
	}
	std::cout << "\n";
}

void VirtualMachine::DebugPrintStack() const
{
	std::cout << "Stack [" << m_context.StackSize() << "]: ";
	for (size_t i = 0; i < m_context.StackSize(); ++i)
	{
		if (i > 0)
			std::cout << ", ";
		Core::ValueHelper::PrintValue(m_context.PeekValue(i), std::cout);
	}
	std::cout << "\n";
}

} // namespace VM::Execution