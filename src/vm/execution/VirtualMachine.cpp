#include "VirtualMachine.h"

#include <iostream>

namespace VM::Execution
{

using Core::Value;

VirtualMachine::VirtualMachine() = default;

bool VirtualMachine::Interpret(const Chunk* chunk)
{
	if (!chunk)
	{
		return false;
	}

	m_context.ClearStack();
	m_context.ClearError();
	m_stepsExecuted = 0;

	auto topLevel = std::make_shared<Core::Function>();
	topLevel->name = "top_level";
	topLevel->chunk->code = chunk->code;
	topLevel->chunk->constants = chunk->constants;

	m_context.PushValue(topLevel);
	m_context.PushFrame(topLevel, 1);

	if (Run() != ExecutionResult::Success)
	{
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
		try
		{
			if (!m_context.HasFrames())
			{
				return ExecutionResult::Success;
			}

			CallFrame& frame = m_context.CurrentFrame();

			if (frame.ip >= frame.function->chunk->GetCodeSize())
			{
				Core::Instruction implicitReturn(Core::OpCode::OP_RETURN, 0);
				Dispatch(implicitReturn);
				if (!m_context.HasFrames())
				{
					return ExecutionResult::Success;
				}
				continue;
			}

			uint8_t byte = frame.function->chunk->GetCode()[frame.ip++];
			const auto opcode = static_cast<Core::OpCode>(byte);
			uint8_t operand = 0;
			if (Core::OpCodeHasOperand(opcode))
			{
				if (frame.ip < frame.function->chunk->GetCodeSize())
				{
					operand = frame.function->chunk->GetCode()[frame.ip++];
				}
			}

			Core::Instruction instr(opcode, operand);
			if (++m_stepsExecuted > m_maxSteps)
			{
				return ExecutionResult::Timeout;
			}
			if (m_debugMode)
			{
				DebugPrintInstruction(instr);
			}

			const int result = Dispatch(instr);

			if (result < 0)
			{
				if (!m_context.HasError())
				{
					return ExecutionResult::RuntimeError;
				}
				return ExecutionResult::RuntimeError;
			}

			if (!m_context.HasFrames())
			{
				return ExecutionResult::Success;
			}

			if (result != 0)
			{
				auto& currentIp = m_context.CurrentFrame().ip;
				currentIp = static_cast<int>(currentIp) + result;
			}

			if (result > 0)
			{
				m_context.CurrentFrame().ip += static_cast<size_t>(result);
			}
		}
		catch (const std::exception& e)
		{
			m_context.RaiseError(e.what());
			return ExecutionResult::RuntimeError;
		}
	}
}

int VirtualMachine::Dispatch(const Core::Instruction& instr)
{
	using enum Core::OpCode;

	CallFrame& frame = m_context.CurrentFrame();
	auto& constants = frame.function->chunk->constants;

	if (auto it = m_extensions.find(instr.opcode); it != m_extensions.end())
	{
		return it->second(instr.opcode, m_context);
	}

	switch (instr.opcode)
	{
	case OP_CALL: {
		int argCount = instr.operand;
		size_t calleeIdx = m_context.StackSize() - argCount - 1;
		Value callee = m_context.GetAt(calleeIdx);

		if (!std::holds_alternative<Core::FunctionPtr>(callee))
		{
			m_context.RaiseError("Can only call functions");
			return -1;
		}

		auto func = std::get<Core::FunctionPtr>(callee);
		if (argCount != func->arity)
		{
			m_context.RaiseError("Expected " + std::to_string(func->arity) + " args");
			return -1;
		}

		size_t newBase = m_context.StackSize() - argCount;
		m_context.PushFrame(func, newBase);
		return 0;
	}

	case OP_RETURN: {
		Value result = std::monostate{};
		if (m_context.StackSize() > frame.stackBase)
		{
			result = m_context.PopValue();
		}

		size_t funcSlot = frame.stackBase - 1;
		m_context.PopFrame();

		if (!m_context.HasFrames())
		{
			std::cout << "Result: ";
			Core::ValueHelper::PrintValue(result, std::cout);
			std::cout << "\n";
		}

		while (m_context.StackSize() > funcSlot)
		{
			m_context.PopValue();
		}

		m_context.PushValue(result);
		return 0;
	}

	case OP_CONSTANT:
		if (instr.operand >= constants.size())
		{
			m_context.RaiseError("Constant index out of bounds");
			return -1;
		}
		m_context.PushValue(constants[instr.operand]);
		return 0;

	case OP_GET_LOCAL:
		m_context.PushValue(m_context.GetLocal(instr.operand));
		return 0;

	case OP_SET_LOCAL:
		m_context.SetLocal(instr.operand, m_context.PeekValue(0));
		return 0;

	case OP_DEFINE_GLOBAL: {
		if (instr.operand >= constants.size())
		{
			m_context.RaiseError("Constant index out of bounds");
			return -1;
		}
		std::string name = Core::ValueHelper::ToString(constants[instr.operand]);
		m_context.DefineGlobal(name, m_context.PopValue());
		return 0;
	}

	case OP_GET_GLOBAL:
	case OP_SET_GLOBAL: {
		if (instr.operand >= constants.size())
		{
			m_context.RaiseError("Constant index out of bounds");
			return -1;
		}
		std::string name = Core::ValueHelper::ToString(constants[instr.operand]);
		if (instr.opcode == OP_GET_GLOBAL)
		{
			Value val;
			if (!m_context.GetGlobal(name, val))
			{
				m_context.RaiseError("Undefined variable: " + name);
				return -1;
			}
			m_context.PushValue(val);
		}
		else
		{
			if (!m_context.SetGlobal(name, m_context.PeekValue(0)))
			{
				m_context.RaiseError("Undefined variable: " + name);
				return -1;
			}
		}
		return 0;
	}

	case OP_ADD: {
		auto b = m_context.PopValue();
		if (auto a = m_context.PopValue();
			Core::ValueHelper::IsString(a) || Core::ValueHelper::IsString(b))
		{
			m_context.PushValue(
				m_stringPool.Intern(
					Core::ValueHelper::ToString(a) + Core::ValueHelper::ToString(b)));
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
		auto b = m_context.PopValue();
		auto a = m_context.PopValue();
		if (instr.opcode == OP_SUBTRACT)
		{
			m_context.PushValue(Core::ValueHelper::Subtract(a, b));
		}
		else if (instr.opcode == OP_MULTIPLY)
		{
			m_context.PushValue(Core::ValueHelper::Multiply(a, b));
		}
		else
		{
			m_context.PushValue(Core::ValueHelper::Divide(a, b));
		}
		return 0;
	}

	case OP_NEGATE: {
		m_context.PushValue(Core::ValueHelper::Negate(m_context.PopValue()));
		return 0;
	}

	case OP_AND: {
		Value b = m_context.PopValue();
		Value a = m_context.PopValue();
		m_context.PushValue(Core::ValueHelper::PerformBinaryLogic(a, b, std::logical_and<bool>{}));
		return 0;
	}

	case OP_OR: {
		Value b = m_context.PopValue();
		Value a = m_context.PopValue();
		m_context.PushValue(Core::ValueHelper::PerformBinaryLogic(a, b, std::logical_or<bool>{}));
		return 0;
	}

	case OP_EQUAL: {
		Value b = m_context.PopValue();
		Value a = m_context.PopValue();
		m_context.PushValue(Core::ValueHelper::Equal(a, b));
		return 0;
	}

	case OP_NOT_EQUAL: {
		Value b = m_context.PopValue();
		Value a = m_context.PopValue();
		m_context.PushValue(!Core::ValueHelper::Equal(a, b));
		return 0;
	}

	case OP_GREATER: {
		Value b = m_context.PopValue();
		Value a = m_context.PopValue();
		m_context.PushValue(Core::ValueHelper::Greater(a, b));
		return 0;
	}

	case OP_GREATER_EQUAL: {
		Value b = m_context.PopValue();
		Value a = m_context.PopValue();
		bool isLess = std::get<bool>(Core::ValueHelper::Less(a, b));
		m_context.PushValue(!isLess);
		return 0;
	}

	case OP_LESS: {
		Value b = m_context.PopValue();
		Value a = m_context.PopValue();
		m_context.PushValue(Core::ValueHelper::Less(a, b));
		return 0;
	}

	case OP_LESS_EQUAL: {
		Value b = m_context.PopValue();
		Value a = m_context.PopValue();
		bool isGreater = std::get<bool>(Core::ValueHelper::Greater(a, b));
		m_context.PushValue(!isGreater);
		return 0;
	}

	case OP_NOT: {
		m_context.PushValue(Core::ValueHelper::PerformUnaryLogic(m_context.PopValue()));
		return 0;
	}

	case OP_JUMP_IF_TRUE: {
		if (Core::ValueHelper::As<bool>(m_context.PeekValue(0)))
		{
			return instr.operand;
		}
		m_context.PopValue();
		return 0;
	}

	case OP_JUMP_IF_FALSE: {
		if (!Core::ValueHelper::As<bool>(m_context.PeekValue(0)))
		{
			return instr.operand;
		}
		m_context.PopValue();
		return 0;
	}

	case OP_JUMP: {
		return instr.operand;
	}

	case OP_LOOP: {
		return -static_cast<int>(instr.operand);
	}

	default:
		m_context.RaiseError("Unknown opcode: " + std::to_string(static_cast<uint8_t>(instr.opcode)));
		return -1;
	}
}

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