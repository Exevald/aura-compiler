#include "VirtualMachine.h"

#include <format>
#include <iostream>

namespace VM::Execution
{

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

	if (const auto result = Run(); result != ExecutionResult::Success)
	{
		if (m_context.HasError())
		{
			std::cerr << "VM Error: " << m_context.GetError() << "\n";
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

		const int result = Dispatch(instr);

		if (result < 0)
		{
			return ExecutionResult::RuntimeError;
		}

		if (result > 0)
		{
			m_ip = static_cast<size_t>(result - 1);
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
	if (OpCodeHasOperand(opcode) && m_ip < m_currentChunk->GetCodeSize())
	{
		operand = m_currentChunk->GetCode()[m_ip++];
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
		if (instr.operand >= m_currentChunk->GetConstants().size())
		{
			m_context.RaiseError("Constant index " + std::to_string(instr.operand) + " out of bounds");
			return -1;
		}
		m_context.PushValue(m_currentChunk->GetConstants()[instr.operand]);
		return 0;
	}

	case OP_NEGATE: {
		if (m_context.StackEmpty())
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
		auto b = m_context.PopValue();
		auto a = m_context.PopValue();

		if (Core::ValueHelper::IsString(a) || Core::ValueHelper::IsString(b))
		{
			std::string concatenated = Core::ValueHelper::ToString(a) + Core::ValueHelper::ToString(b);
			m_context.PushValue(m_stringPool.Intern(concatenated));
		}
		else
		{
			m_context.PushValue(Core::ValueHelper::Add(a, b));
		}
		return 0;
	}

	case OP_SUBTRACT:
	case OP_MULTIPLY:
	case OP_DIVIDE: {
		if (m_context.StackSize() < 2)
		{
			m_context.RaiseError("Stack underflow in binary operation");
			return -1;
		}
		Core::Value b = m_context.PopValue();
		Core::Value a = m_context.PopValue();

		try
		{
			if (instr.opcode == OP_SUBTRACT)
				m_context.PushValue(Core::ValueHelper::Subtract(a, b));
			else if (instr.opcode == OP_MULTIPLY)
				m_context.PushValue(Core::ValueHelper::Multiply(a, b));
			else
				m_context.PushValue(Core::ValueHelper::Divide(a, b));
		}
		catch (const std::runtime_error& e)
		{
			m_context.RaiseError(e.what());
			return -1;
		}
		return 0;
	}

	case OP_DEFINE_GLOBAL: {
		if (instr.operand >= m_currentChunk->GetConstants().size())
		{
			m_context.RaiseError("Global name index out of bounds");
			return -1;
		}
		if (m_context.StackEmpty())
		{
			m_context.RaiseError("Stack underflow in OP_DEFINE_GLOBAL");
			return -1;
		}
		std::string name = Core::ValueHelper::ToString(m_currentChunk->GetConstants()[instr.operand]);
		m_context.DefineGlobal(name, m_context.PopValue());
		return 0;
	}

	case OP_GET_GLOBAL:
	case OP_SET_GLOBAL: {
		if (instr.operand >= m_currentChunk->GetConstants().size())
		{
			m_context.RaiseError("Global index out of bounds");
			return -1;
		}
		std::string name = Core::ValueHelper::ToString(m_currentChunk->GetConstants()[instr.operand]);

		if (instr.opcode == OP_GET_GLOBAL)
		{
			Core::Value val;
			if (!m_context.GetGlobal(name, val))
			{
				m_context.RaiseError("Undefined variable: " + name);
				return -1;
			}
			m_context.PushValue(val);
		}
		else
		{
			if (m_context.StackEmpty())
			{
				m_context.RaiseError("Stack underflow in OP_SET_GLOBAL");
				return -1;
			}
			if (!m_context.SetGlobal(name, m_context.PeekValue(0)))
			{
				m_context.RaiseError("Undefined variable: " + name);
				return -1;
			}
		}
		return 0;
	}

	case OP_GET_LOCAL: {
		if (instr.operand >= m_context.StackSize())
		{
			m_context.RaiseError("Local variable index " + std::to_string(instr.operand) + " out of bounds");
			return -1;
		}
		m_context.PushValue(m_context.GetAt(instr.operand));
		return 0;
	}

	case OP_SET_LOCAL: {
		if (instr.operand >= m_context.StackSize())
		{
			m_context.RaiseError("Local assignment index " + std::to_string(instr.operand) + " out of bounds");
			return -1;
		}
		if (m_context.StackEmpty())
		{
			m_context.RaiseError("Stack underflow in OP_SET_LOCAL");
			return -1;
		}
		m_context.SetAt(instr.operand, m_context.PeekValue(0));
		return 0;
	}

	case OP_JUMP_IF_FALSE: {
		if (m_context.StackEmpty())
		{
			m_context.RaiseError("Stack underflow in OP_JUMP_IF_FALSE");
			return -1;
		}
		if (!Core::ValueHelper::As<bool>(m_context.PopValue()))
		{
			return static_cast<int>(instr.operand) + 1;
		}
		return 0;
	}

	case OP_JUMP:
		return static_cast<int>(instr.operand) + 1;

	default:
		m_context.RaiseError("Unknown opcode: " + std::to_string(static_cast<uint8_t>(instr.opcode)));
		return -1;
	}
}

void VirtualMachine::DebugPrintInstruction(const Core::Instruction& instr) const
{
	std::cout << "["
			  << (m_ip - 1)
			  << "] "
			  << Core::GetOpCodeName(instr.opcode)
			  << " ";

	if (instr.operand != 0)
	{
		std::cout << "arg=" << static_cast<int>(instr.operand);
	}
	std::cout << "\n";
}

} // namespace VM::Execution