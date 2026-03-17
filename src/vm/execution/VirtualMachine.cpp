#include "VirtualMachine.h"
#include "../core/values/ValueHelper.h"

#include <algorithm>
#include <iostream>
#include <utility>

namespace
{

std::string ValueToString(const VM::Core::Value& value)
{
	if (std::holds_alternative<long long>(value))
	{
		return std::to_string(std::get<long long>(value));
	}
	if (std::holds_alternative<double>(value))
	{
		return std::to_string(std::get<double>(value));
	}
	if (std::holds_alternative<bool>(value))
	{
		return std::get<bool>(value) ? "true" : "false";
	}
	if (std::holds_alternative<std::shared_ptr<const std::string>>(value))
	{
		return *std::get<std::shared_ptr<const std::string>>(value);
	}
	return "";
}

} // namespace

namespace VM::Execution
{

using Core::Value;

VirtualMachine::VirtualMachine() = default;

bool VirtualMachine::Interpret(const Chunk* chunk)
{
	if (!chunk)
		return false;

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
	const size_t entryFrameCount = m_context.GetFramesCount();
	for (;;)
	{
		try
		{
			if (m_context.GetFramesCount() < entryFrameCount)
			{
				return ExecutionResult::Success;
			}

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
				return ExecutionResult::RuntimeError;
			}

			if (m_context.GetFramesCount() < entryFrameCount)
			{
				return ExecutionResult::Success;
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
		Value b = m_context.PopValue();
		Value a = m_context.PopValue();
		if (std::holds_alternative<std::shared_ptr<const std::string>>(a)
			|| std::holds_alternative<std::shared_ptr<const std::string>>(b))
		{
			std::string result = ValueToString(a) + ValueToString(b);
			m_context.PushValue(std::make_shared<const std::string>(result));
		}
		else if (std::holds_alternative<long long>(a) && std::holds_alternative<long long>(b))
		{
			m_context.PushValue(std::get<long long>(a) + std::get<long long>(b));
		}
		else if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b))
		{
			m_context.PushValue(std::get<double>(a) + std::get<double>(b));
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
		if (Core::ValueHelper::As<bool>(m_context.PopValue()))
		{
			frame.ip += instr.operand;
		}
		return 0;
	}

	case OP_JUMP_IF_FALSE: {
		if (bool condition = Core::ValueHelper::As<bool>(m_context.PopValue());
			!condition)
		{
			frame.ip += instr.operand;
		}
		return 0;
	}

	case OP_JUMP: {
		frame.ip += instr.operand;
		return 0;
	}

	case OP_LOOP: {
		frame.ip -= instr.operand;
		return 0;
	}

	case OP_POP: {
		m_context.PopValue();
		return 0;
	}

	case OP_MOD: {
		Value b = m_context.PopValue();
		Value a = m_context.PopValue();
		m_context.PushValue(Core::ValueHelper::Modulo(a, b));
		return 0;
	}

	case OP_DIV: {
		Value b = m_context.PopValue();
		Value a = m_context.PopValue();
		m_context.PushValue(Core::ValueHelper::DivideInt(a, b));
		return 0;
	}

	case OP_BUILD_ARRAY: {
		uint8_t count = instr.operand;
		auto array = std::make_shared<Core::Array>();
		array->elements.reserve(count);

		for (int i = 0; i < count; ++i)
		{
			array->elements.push_back(m_context.PopValue());
		}
		std::ranges::reverse(array->elements);

		m_context.PushValue(array);
		return 0;
	}

	case OP_INDEX_GET: {
		Value indexVal = m_context.PopValue();
		Value container = m_context.PopValue();

		if (!std::holds_alternative<Core::ArrayPtr>(container))
		{
			m_context.RaiseError("Only arrays can be indexed");
			return -1;
		}

		auto& arr = std::get<Core::ArrayPtr>(container);
		auto idx = Core::ValueHelper::As<int64_t>(indexVal);

		if (idx < 0 || idx >= arr->elements.size())
		{
			m_context.RaiseError("Index out of bounds");
			return -1;
		}

		m_context.PushValue(arr->elements[idx]);
		return 0;
	}

	case OP_MEMBER_GET: {
		uint8_t fieldIdx = instr.operand;

		if (Value obj = m_context.PopValue(); std::holds_alternative<Core::InstancePtr>(obj))
		{
			auto& inst = std::get<Core::InstancePtr>(obj);
			m_context.PushValue(inst->fields[fieldIdx]);
		}
		else
		{
			m_context.RaiseError("Only instances have members");
			return -1;
		}
		return 0;
	}

	case OP_BUILD_STRUCT: {
		uint8_t fieldCount = instr.operand;
		auto inst = std::make_shared<Core::Instance>();
		inst->fields.resize(fieldCount);

		for (int i = fieldCount - 1; i >= 0; --i)
		{
			inst->fields[i] = m_context.PopValue();
		}
		m_context.PushValue(inst);
		return 0;
	}

	case OP_INDEX_SET: {
		Value val = m_context.PopValue();
		Value indexVal = m_context.PopValue();
		Value container = m_context.PopValue();

		if (!std::holds_alternative<Core::ArrayPtr>(container))
		{
			m_context.RaiseError("Only arrays support indexed assignment");
			return -1;
		}

		auto& arr = std::get<Core::ArrayPtr>(container);
		auto idx = Core::ValueHelper::As<int64_t>(indexVal);

		if (idx < 0 || idx >= arr->elements.size())
		{
			m_context.RaiseError("Array index out of bounds");
			return -1;
		}

		arr->elements[idx] = val;
		m_context.PushValue(val);
		return 0;
	}

	case OP_MEMBER_SET: {
		uint8_t fieldIdx = instr.operand;
		Value val = m_context.PopValue();
		Value obj = m_context.PopValue();

		if (!std::holds_alternative<Core::InstancePtr>(obj))
		{
			m_context.RaiseError("Only instances have members");
			return -1;
		}

		auto& inst = std::get<Core::InstancePtr>(obj);
		if (fieldIdx >= inst->fields.size())
		{
			m_context.RaiseError("Field index out of bounds");
			return -1;
		}

		inst->fields[fieldIdx] = val;
		m_context.PushValue(val);
		return 0;
	}

	case OP_BUILD_ENUM: {
		uint8_t tag = instr.operand;
		uint8_t argCount = frame.function->chunk->code[frame.ip++];

		auto ev = std::make_shared<Core::EnumVariant>();
		ev->tag = tag;
		ev->args.resize(argCount);

		for (int i = argCount - 1; i >= 0; --i)
		{
			ev->args[i] = m_context.PopValue();
		}

		m_context.PushValue(ev);
		return 0;
	}

	case OP_GET_ENUM_TAG: {
		if (Value v = m_context.PopValue();
			std::holds_alternative<Core::EnumPtr>(v))
		{
			m_context.PushValue(std::get<Core::EnumPtr>(v)->tag);
		}
		else
		{
			m_context.RaiseError("Value is not an enum variant");
			return -1;
		}
		return 0;
	}

	case OP_ADDR_GLOBAL: {
		uint8_t nameIdx = instr.operand;
		std::string name = Core::ValueHelper::ToString(constants[nameIdx]);

		auto ptr = std::make_shared<Core::Pointer>();
		ptr->targetName = "&global:" + name;

		ptr->get = [this, name] {
			Value val;
			m_context.GetGlobal(name, val);
			return val;
		};
		ptr->set = [this, name](Value val) {
			m_context.SetGlobal(name, std::move(val));
		};

		m_context.PushValue(ptr);
		return 0;
	}

	case OP_ADDR_MEMBER: {
		Value indexVal = m_context.PopValue();
		Value container = m_context.PopValue();

		auto ptr = std::make_shared<Core::Pointer>();

		if (std::holds_alternative<Core::ArrayPtr>(container))
		{
			auto arr = std::get<Core::ArrayPtr>(container);
			auto idx = Core::ValueHelper::As<int64_t>(indexVal);

			ptr->get = [arr, idx]() { return arr->elements[idx]; };
			ptr->set = [arr, idx](const Value& v) { arr->elements[idx] = v; };
		}
		else if (std::holds_alternative<Core::InstancePtr>(container))
		{
			auto inst = std::get<Core::InstancePtr>(container);
			auto idx = Core::ValueHelper::As<int64_t>(indexVal);

			ptr->get = [inst, idx] { return inst->fields[idx]; };
			ptr->set = [inst, idx](const Value& v) { inst->fields[idx] = v; };
		}

		m_context.PushValue(ptr);
		return 0;
	}

	case OP_DEREF_GET: {
		Value v = m_context.PopValue();
		if (!std::holds_alternative<Core::PointerPtr>(v))
		{
			m_context.RaiseError("Can only dereference pointer types");
			return -1;
		}
		m_context.PushValue(std::get<Core::PointerPtr>(v)->get());
		return 0;
	}

	case OP_DEREF_SET: {
		Value val = m_context.PopValue();
		Value pVal = m_context.PopValue();

		if (!std::holds_alternative<Core::PointerPtr>(pVal))
		{
			m_context.RaiseError("Can only dereference pointer types");
			return -1;
		}

		std::get<Core::PointerPtr>(pVal)->set(val);
		m_context.PushValue(val);
		return 0;
	}

	case OP_MAKE_ITER: {
		Value iterable = m_context.PopValue();
		auto it = std::make_shared<Core::Iterator>();

		if (std::holds_alternative<Core::ArrayPtr>(iterable))
		{
			auto arr = std::get<Core::ArrayPtr>(iterable);
			auto index = std::make_shared<size_t>(0);

			it->next = [arr, index]() -> std::pair<bool, Value> {
				if (*index < arr->elements.size())
				{
					return { true, arr->elements[(*index)++] };
				}
				return { false, std::monostate{} };
			};
		}
		else
		{
			m_context.RaiseError("Object is not iterable");
			return -1;
		}

		m_context.PushValue(it);
		return 0;
	}

	case OP_ITER_NEXT: {
		uint8_t jumpOffset = instr.operand;
		Value v = m_context.PeekValue(0);

		if (!std::holds_alternative<Core::IteratorPtr>(v))
		{
			m_context.RaiseError("Expected iterator on stack");
			return -1;
		}

		if (auto [hasValue, value] = std::get<Core::IteratorPtr>(v)->next(); hasValue)
		{
			m_context.PushValue(value);
		}
		else
		{
			frame.ip += jumpOffset;
		}
		return 0;
	}

	case OP_ITER_TAKE: {
		auto n = Core::ValueHelper::As<int64_t>(m_context.PopValue());
		Value v = m_context.PopValue();
		auto sourceIt = std::get<Core::IteratorPtr>(v);

		auto it = std::make_shared<Core::Iterator>();
		auto remaining = std::make_shared<int64_t>(n);

		it->next = [sourceIt, remaining]() -> std::pair<bool, Value> {
			if (*remaining > 0)
			{
				(*remaining)--;
				return sourceIt->next();
			}
			return { false, std::monostate{} };
		};
		m_context.PushValue(it);
		return 0;
	}

	case OP_ITER_DROP: {
		auto n = Core::ValueHelper::As<int64_t>(m_context.PopValue());
		Value v = m_context.PopValue();
		auto sourceIt = std::get<Core::IteratorPtr>(v);

		auto it = std::make_shared<Core::Iterator>();
		auto dropped = std::make_shared<bool>(false);

		it->next = [sourceIt, n, dropped]() -> std::pair<bool, Value> {
			if (!*dropped)
			{
				for (int i = 0; i < n; ++i)
				{
					std::ignore = sourceIt->next();
				}
				*dropped = true;
			}
			return sourceIt->next();
		};
		m_context.PushValue(it);
		return 0;
	}

	case OP_ITER_TRANSFORM: {
		Value fnVal = m_context.PopValue();
		Value itVal = m_context.PopValue();
		auto sourceIt = std::get<Core::IteratorPtr>(itVal);
		auto fn = std::get<Core::FunctionPtr>(fnVal);

		auto it = std::make_shared<Core::Iterator>();
		it->next = [this, sourceIt, fn]() -> std::pair<bool, Value> {
			auto [hasValue, val] = sourceIt->next();
			if (!hasValue)
			{
				return { false, std::monostate{} };
			}

			m_context.PushValue(fn);
			m_context.PushValue(val);

			const size_t argBase = m_context.StackSize() - 1;
			m_context.PushFrame(fn, argBase);

			this->Run();

			return { true, m_context.PopValue() };
		};

		m_context.PushValue(it);
		return 0;
	}

	case OP_ITER_REVERSE: {
		Value v = m_context.PopValue();
		auto sourceIt = std::get<Core::IteratorPtr>(v);

		auto it = std::make_shared<Core::Iterator>();
		auto buffer = std::make_shared<std::vector<Value>>();
		auto initialized = std::make_shared<bool>(false);
		auto index = std::make_shared<int64_t>(0);

		it->next = [sourceIt, buffer, initialized, index]() -> std::pair<bool, Value> {
			if (!*initialized)
			{
				while (true)
				{
					auto [has, val] = sourceIt->next();
					if (!has)
					{
						break;
					}
					buffer->push_back(val);
				}
				*index = buffer->size() - 1;
				*initialized = true;
			}
			if (*index >= 0)
			{
				return { true, (*buffer)[(*index)--] };
			}
			return { false, std::monostate{} };
		};
		m_context.PushValue(it);
		return 0;
	}

	case OP_ITER_FILTER: {
		Value fnVal = m_context.PopValue();
		Value itVal = m_context.PopValue();
		auto sourceIt = std::get<Core::IteratorPtr>(itVal);
		auto fn = std::get<Core::FunctionPtr>(fnVal);

		auto it = std::make_shared<Core::Iterator>();
		it->next = [this, sourceIt, fn]() -> std::pair<bool, Value> {
			while (true)
			{
				auto [hasValue, val] = sourceIt->next();
				if (!hasValue)
				{
					return { false, std::monostate{} };
				}

				m_context.PushValue(fn);
				m_context.PushValue(val);

				const size_t argBase = m_context.StackSize() - 1;
				m_context.PushFrame(fn, argBase);

				this->Run();

				if (Core::ValueHelper::As<bool>(m_context.PopValue()))
				{
					return { true, val };
				}
			}
		};
		m_context.PushValue(it);
		return 0;
	}

	case OP_PRINT: {
		Value val = m_context.PopValue();
		Core::ValueHelper::PrintValue(val, std::cout);
		std::cout << "\n";
		return 0;
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