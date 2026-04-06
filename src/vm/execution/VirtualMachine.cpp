#include "VirtualMachine.h"
#include "../core/values/ValueHelper.h"
#include "../runtime/DiagnosticsModule.h"
#include "../runtime/SyncModule.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <utility>

namespace VM::Execution
{

using Core::Value;

namespace
{
constexpr int RuntimeError = -1;

int Fail(ExecutionContext& context, const std::string& message)
{
	context.RaiseError(message);
	return RuntimeError;
}

std::optional<std::string> ReadStringConstant(
	const std::vector<Value>& constants,
	const uint16_t operand,
	ExecutionContext& context)
{
	if (operand >= constants.size())
	{
		Fail(context, "Constant index out of bounds");
		return std::nullopt;
	}

	return Core::ValueHelper::ToString(constants[operand]);
}
} // namespace

VirtualMachine::VirtualMachine()
{
	Runtime::DiagnosticsModule::Install(m_context);
	Runtime::SyncModule::Install(m_context);
}

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
	auto closure = std::make_shared<Core::Closure>();
	closure->function = topLevel;

	m_context.PushValue(closure);
	m_context.PushFrame(topLevel, closure, 1);

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
			uint16_t operand = 0;

			if (const auto opSize = Core::GetOperandSize(opcode);
				opSize == Core::OperandSize::Uint8)
			{
				operand = frame.function->chunk->GetCode()[frame.ip++];
			}
			else if (opSize == Core::OperandSize::Uint16)
			{
				const uint8_t msb = frame.function->chunk->GetCode()[frame.ip++];
				const uint8_t lsb = frame.function->chunk->GetCode()[frame.ip++];
				operand = static_cast<uint16_t>((msb << 8) | lsb);
			}

			Core::Instruction instr(opcode, operand);
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
	case OP_CALL:
		return HandleCall(instr.operand);

	case OP_RETURN:
		return HandleReturn();

	case OP_CONSTANT:
		if (instr.operand >= constants.size())
		{
			return Fail(m_context, "Constant index out of bounds");
		}
		m_context.PushValue(constants[instr.operand]);
		return 0;

	case OP_GET_LOCAL:
		m_context.PushValue(m_context.GetLocal(instr.operand));
		return 0;

	case OP_SET_LOCAL:
		m_context.SetLocal(instr.operand, m_context.PeekValue(0));
		return 0;

	case OP_GET_UPVALUE:
		m_context.PushValue(m_context.GetUpvalue(instr.operand));
		return 0;

	case OP_SET_UPVALUE:
		m_context.SetUpvalue(instr.operand, m_context.PeekValue(0));
		return 0;

	case OP_DEFINE_GLOBAL:
	case OP_GET_GLOBAL:
	case OP_SET_GLOBAL:
		return HandleGlobal(instr.opcode, instr.operand, constants);

	case OP_CLOSURE:
		return HandleClosure(instr.operand, constants);

	case OP_ADD: {
		Value b = m_context.PopValue();
		Value a = m_context.PopValue();
		m_context.PushValue(Core::ValueHelper::Add(a, b));
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
		return DataExecutor{ *this }.BuildArray(static_cast<uint8_t>(instr.operand));
	}

	case OP_INDEX_GET:
		return DataExecutor{ *this }.HandleArrayIndexGet();

	case OP_MEMBER_GET:
		return DataExecutor{ *this }.HandleMemberGet(static_cast<uint8_t>(instr.operand));

	case OP_GET_MODULE_MEMBER:
		return DataExecutor{ *this }.HandleModuleMemberGet(instr.operand, constants);

	case OP_BUILD_STRUCT: {
		return DataExecutor{ *this }.BuildStruct(static_cast<uint8_t>(instr.operand));
	}

	case OP_INDEX_SET:
		return DataExecutor{ *this }.HandleArrayIndexSet();

	case OP_MEMBER_SET:
		return DataExecutor{ *this }.HandleMemberSet(static_cast<uint8_t>(instr.operand));

	case OP_BUILD_ENUM: {
		uint8_t tag = instr.operand;
		uint8_t argCount = frame.function->chunk->code[frame.ip++];
		return DataExecutor{ *this }.BuildEnum(tag, argCount);
	}

	case OP_GET_ENUM_TAG:
		return DataExecutor{ *this }.HandleEnumTagGet();

	case OP_GET_ENUM_ARG:
		return DataExecutor{ *this }.HandleEnumArgGet(static_cast<uint8_t>(instr.operand));

	case OP_ADDR_LOCAL:
		return DataExecutor{ *this }.HandleAddressOfLocal(static_cast<uint8_t>(instr.operand));

	case OP_ADDR_GLOBAL:
		return DataExecutor{ *this }.HandleAddressOfGlobal(instr.operand, constants);

	case OP_ADDR_MEMBER:
		return DataExecutor{ *this }.HandleAddressOfMember();

	case OP_ADDR_UPVALUE:
		return DataExecutor{ *this }.HandleAddressOfUpvalue(static_cast<uint8_t>(instr.operand));

	case OP_DEREF_GET:
		return DataExecutor{ *this }.HandleDerefGet();

	case OP_DEREF_SET:
		return DataExecutor{ *this }.HandleDerefSet();

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
			return Fail(m_context, "Object is not iterable");
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
		Core::ClosurePtr closure;
		Core::FunctionPtr fn;
		if (std::holds_alternative<Core::ClosurePtr>(fnVal))
		{
			closure = std::get<Core::ClosurePtr>(fnVal);
			fn = closure->function;
		}
		else
		{
			fn = std::get<Core::FunctionPtr>(fnVal);
			closure = std::make_shared<Core::Closure>();
			closure->function = fn;
		}

		auto it = std::make_shared<Core::Iterator>();
		it->next = [this, sourceIt, fn, closure]() -> std::pair<bool, Value> {
			auto [hasValue, val] = sourceIt->next();
			if (!hasValue)
			{
				return { false, std::monostate{} };
			}

			m_context.PushValue(closure);
			m_context.PushValue(val);

			const size_t argBase = m_context.StackSize() - 1;
			m_context.PushFrame(fn, closure, argBase);

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
		Core::ClosurePtr closure;
		Core::FunctionPtr fn;
		if (std::holds_alternative<Core::ClosurePtr>(fnVal))
		{
			closure = std::get<Core::ClosurePtr>(fnVal);
			fn = closure->function;
		}
		else
		{
			fn = std::get<Core::FunctionPtr>(fnVal);
			closure = std::make_shared<Core::Closure>();
			closure->function = fn;
		}

		auto it = std::make_shared<Core::Iterator>();
		it->next = [this, sourceIt, fn, closure]() -> std::pair<bool, Value> {
			while (true)
			{
				auto [hasValue, val] = sourceIt->next();
				if (!hasValue)
				{
					return { false, std::monostate{} };
				}

				m_context.PushValue(closure);
				m_context.PushValue(val);

				const size_t argBase = m_context.StackSize() - 1;
				m_context.PushFrame(fn, closure, argBase);

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

int VirtualMachine::HandleCall(const uint16_t argCount)
{
	const size_t calleeIdx = m_context.StackSize() - argCount - 1;
	const Value callee = m_context.GetAt(calleeIdx);
	Core::ClosurePtr closure;
	Core::FunctionPtr func;

	if (std::holds_alternative<Core::ClosurePtr>(callee))
	{
		closure = std::get<Core::ClosurePtr>(callee);
		func = closure ? closure->function : nullptr;
	}
	else if (std::holds_alternative<Core::FunctionPtr>(callee))
	{
		func = std::get<Core::FunctionPtr>(callee);
		closure = std::make_shared<Core::Closure>();
		closure->function = func;
	}
	else if (std::holds_alternative<Core::NativeFunctionPtr>(callee))
	{
		const auto& native = std::get<Core::NativeFunctionPtr>(callee);
		if (!native)
		{
			return Fail(m_context, "Can only call functions");
		}

		if (argCount != native->arity)
		{
			return Fail(m_context, "Expected " + std::to_string(native->arity) + " args");
		}

		std::vector<Value> args;
		args.reserve(argCount);
		for (size_t i = 0; i < argCount; ++i)
		{
			args.push_back(m_context.GetAt(calleeIdx + 1 + i));
		}

		const Value result = native->invoke(m_context, args);
		if (m_context.HasError())
		{
			return RuntimeError;
		}

		while (m_context.StackSize() > calleeIdx)
		{
			m_context.PopValue();
		}

		m_context.PushValue(result);
		return 0;
	}
	else
	{
		return Fail(m_context, "Can only call functions");
	}

	if (!func)
	{
		return Fail(m_context, "Can only call functions");
	}

	if (argCount != func->arity)
	{
		return Fail(m_context, "Expected " + std::to_string(func->arity) + " args");
	}

	const size_t newBase = m_context.StackSize() - argCount;
	m_context.PushFrame(func, closure, newBase);
	return 0;
}

int VirtualMachine::HandleReturn()
{
	const CallFrame& frame = m_context.CurrentFrame();
	Value result = std::monostate{};
	if (m_context.StackSize() > frame.stackBase)
	{
		result = m_context.PopValue();
	}

	const size_t funcSlot = frame.stackBase - 1;
	m_context.PopFrame();

	while (m_context.StackSize() > funcSlot)
	{
		m_context.PopValue();
	}

	m_context.PushValue(result);
	return 0;
}

int VirtualMachine::HandleGlobal(
	const Core::OpCode opcode,
	const uint16_t operand,
	const std::vector<Core::Value>& constants)
{
	const auto name = ReadStringConstant(constants, operand, m_context);
	if (!name)
	{
		return RuntimeError;
	}

	if (opcode == Core::OpCode::OP_DEFINE_GLOBAL)
	{
		m_context.DefineGlobal(*name, m_context.PopValue());
		return 0;
	}

	if (opcode == Core::OpCode::OP_GET_GLOBAL)
	{
		Value val;
		if (!m_context.GetGlobal(*name, val))
		{
			return Fail(m_context, "Undefined variable: " + *name);
		}
		m_context.PushValue(val);
		return 0;
	}

	if (!m_context.SetGlobal(*name, m_context.PeekValue(0)))
	{
		return Fail(m_context, "Undefined variable: " + *name);
	}
	return 0;
}

int VirtualMachine::HandleClosure(const uint16_t operand, const std::vector<Core::Value>& constants)
{
	if (operand >= constants.size())
	{
		return Fail(m_context, "Constant index out of bounds");
	}

	if (!std::holds_alternative<Core::FunctionPtr>(constants[operand]))
	{
		return Fail(m_context, "OP_CLOSURE requires function constant");
	}

	const auto& function = std::get<Core::FunctionPtr>(constants[operand]);
	auto closure = std::make_shared<Core::Closure>();
	closure->function = function;
	closure->captures.resize(function->captureNames.size());

	for (size_t i = function->captureNames.size(); i > 0; --i)
	{
		closure->captures[i - 1] = m_context.PopValue();
	}

	m_context.PushValue(closure);
	return 0;
}

int VirtualMachine::DataExecutor::BuildArray(const uint8_t count) const
{
	auto array = std::make_shared<Core::Array>();
	array->elements.reserve(count);

	for (int i = 0; i < count; ++i)
	{
		array->elements.push_back(vm.m_context.PopValue());
	}
	std::ranges::reverse(array->elements);

	vm.m_context.PushValue(array);
	return 0;
}

int VirtualMachine::DataExecutor::BuildStruct(const uint8_t fieldCount) const
{
	auto inst = std::make_shared<Core::Instance>();
	inst->fields.resize(fieldCount);

	for (int i = fieldCount - 1; i >= 0; --i)
	{
		inst->fields[i] = vm.m_context.PopValue();
	}
	vm.m_context.PushValue(inst);
	return 0;
}

int VirtualMachine::DataExecutor::BuildEnum(const uint8_t tag, const uint8_t argCount) const
{
	auto ev = std::make_shared<Core::EnumVariant>();
	ev->tag = tag;
	ev->args.resize(argCount);

	for (int i = argCount - 1; i >= 0; --i)
	{
		ev->args[i] = vm.m_context.PopValue();
	}

	vm.m_context.PushValue(ev);
	return 0;
}

int VirtualMachine::DataExecutor::HandleArrayIndexGet() const
{
	const Value indexVal = vm.m_context.PopValue();
	const Value container = vm.m_context.PopValue();
	if (!std::holds_alternative<Core::ArrayPtr>(container))
	{
		return Fail(vm.m_context, "Only arrays can be indexed");
	}

	const auto& arr = std::get<Core::ArrayPtr>(container);
	const auto idx = Core::ValueHelper::As<int64_t>(indexVal);
	if (idx < 0 || idx >= arr->elements.size())
	{
		return Fail(vm.m_context, "Index out of bounds");
	}

	vm.m_context.PushValue(arr->elements[idx]);
	return 0;
}

int VirtualMachine::DataExecutor::HandleArrayIndexSet() const
{
	const Value val = vm.m_context.PopValue();
	const Value indexVal = vm.m_context.PopValue();
	const Value container = vm.m_context.PopValue();
	if (!std::holds_alternative<Core::ArrayPtr>(container))
	{
		return Fail(vm.m_context, "Only arrays support indexed assignment");
	}

	const auto& arr = std::get<Core::ArrayPtr>(container);
	const auto idx = Core::ValueHelper::As<int64_t>(indexVal);
	if (idx < 0 || static_cast<size_t>(idx) >= arr->elements.size())
	{
		return Fail(vm.m_context, "Array index out of bounds");
	}

	arr->elements[idx] = val;
	vm.m_context.PushValue(val);
	return 0;
}

int VirtualMachine::DataExecutor::HandleMemberGet(const uint8_t fieldIdx) const
{
	const Value obj = vm.m_context.PopValue();
	if (!std::holds_alternative<Core::InstancePtr>(obj))
	{
		return Fail(vm.m_context, "Only instances have members");
	}

	const auto& inst = std::get<Core::InstancePtr>(obj);
	if (fieldIdx >= inst->fields.size())
	{
		return Fail(vm.m_context, "Field index out of bounds");
	}

	vm.m_context.PushValue(inst->fields[fieldIdx]);
	return 0;
}

int VirtualMachine::DataExecutor::HandleMemberSet(const uint8_t fieldIdx) const
{
	const Value val = vm.m_context.PopValue();
	const Value obj = vm.m_context.PopValue();
	if (!std::holds_alternative<Core::InstancePtr>(obj))
	{
		return Fail(vm.m_context, "Only instances have members");
	}

	const auto& inst = std::get<Core::InstancePtr>(obj);
	if (fieldIdx >= inst->fields.size())
	{
		return Fail(vm.m_context, "Field index out of bounds");
	}

	inst->fields[fieldIdx] = val;
	vm.m_context.PushValue(val);
	return 0;
}

int VirtualMachine::DataExecutor::HandleModuleMemberGet(
	const uint16_t operand,
	const std::vector<Core::Value>& constants) const
{
	const auto memberName = ReadStringConstant(constants, operand, vm.m_context);
	if (!memberName)
	{
		return RuntimeError;
	}

	const Value object = vm.m_context.PopValue();
	if (!std::holds_alternative<Core::ModulePtr>(object))
	{
		return Fail(vm.m_context, "Only modules support named member access");
	}

	const auto& module = std::get<Core::ModulePtr>(object);
	Value member;
	if (!vm.m_context.GetGlobal(module->name + "." + *memberName, member))
	{
		return Fail(vm.m_context, "Undefined module member: " + module->name + "." + *memberName);
	}

	vm.m_context.PushValue(member);
	return 0;
}

int VirtualMachine::DataExecutor::HandleEnumTagGet() const
{
	const Value value = vm.m_context.PopValue();
	if (!std::holds_alternative<Core::EnumPtr>(value))
	{
		return Fail(vm.m_context, "Value is not an enum variant");
	}

	vm.m_context.PushValue(std::get<Core::EnumPtr>(value)->tag);
	return 0;
}

int VirtualMachine::DataExecutor::HandleEnumArgGet(const uint8_t argIndex) const
{
	const Value value = vm.m_context.PopValue();
	if (!std::holds_alternative<Core::EnumPtr>(value))
	{
		return Fail(vm.m_context, "Value is not an enum variant");
	}

	const auto& enumValue = std::get<Core::EnumPtr>(value);
	if (argIndex >= enumValue->args.size())
	{
		return Fail(vm.m_context, "Enum argument index out of bounds");
	}

	vm.m_context.PushValue(enumValue->args[argIndex]);
	return 0;
}

int VirtualMachine::DataExecutor::HandleAddressOfLocal(const uint8_t slot) const
{
	auto ptr = std::make_shared<Core::Pointer>();
	ptr->targetName = "&local:" + std::to_string(slot);
	ptr->get = [this, slot] { return vm.m_context.GetLocal(slot); };
	ptr->set = [this, slot](Value val) { vm.m_context.SetLocal(slot, std::move(val)); };
	vm.m_context.PushValue(ptr);
	return 0;
}

int VirtualMachine::DataExecutor::HandleAddressOfGlobal(
	const uint16_t operand,
	const std::vector<Core::Value>& constants) const
{
	const auto name = ReadStringConstant(constants, operand, vm.m_context);
	if (!name)
	{
		return RuntimeError;
	}

	auto ptr = std::make_shared<Core::Pointer>();
	ptr->targetName = "&global:" + *name;
	ptr->get = [this, name] {
		Value val;
		vm.m_context.GetGlobal(*name, val);
		return val;
	};
	ptr->set = [this, name](Value val) {
		vm.m_context.SetGlobal(*name, std::move(val));
	};
	vm.m_context.PushValue(ptr);
	return 0;
}

int VirtualMachine::DataExecutor::HandleAddressOfMember() const
{
	const Value indexVal = vm.m_context.PopValue();
	const Value container = vm.m_context.PopValue();
	auto ptr = std::make_shared<Core::Pointer>();

	if (std::holds_alternative<Core::ArrayPtr>(container))
	{
		const auto arr = std::get<Core::ArrayPtr>(container);
		const auto idx = Core::ValueHelper::As<int64_t>(indexVal);
		if (idx < 0 || static_cast<size_t>(idx) >= arr->elements.size())
		{
			return Fail(vm.m_context, "Array index out of bounds");
		}

		ptr->get = [arr, idx] { return arr->elements[idx]; };
		ptr->set = [arr, idx](const Value& v) { arr->elements[idx] = v; };
	}
	else if (std::holds_alternative<Core::InstancePtr>(container))
	{
		const auto inst = std::get<Core::InstancePtr>(container);
		const auto idx = Core::ValueHelper::As<int64_t>(indexVal);
		if (idx < 0 || static_cast<size_t>(idx) >= inst->fields.size())
		{
			return Fail(vm.m_context, "Field index out of bounds");
		}

		ptr->get = [inst, idx] { return inst->fields[idx]; };
		ptr->set = [inst, idx](const Value& v) { inst->fields[idx] = v; };
	}
	else
	{
		return Fail(vm.m_context, "Only arrays and instances support addressable members");
	}

	vm.m_context.PushValue(ptr);
	return 0;
}

int VirtualMachine::DataExecutor::HandleAddressOfUpvalue(const uint8_t slot) const
{
	auto ptr = std::make_shared<Core::Pointer>();
	ptr->targetName = "&upvalue:" + std::to_string(slot);
	ptr->get = [this, slot] { return vm.m_context.GetUpvalue(slot); };
	ptr->set = [this, slot](Value val) { vm.m_context.SetUpvalue(slot, std::move(val)); };
	vm.m_context.PushValue(ptr);
	return 0;
}

int VirtualMachine::DataExecutor::HandleDerefGet() const
{
	const Value value = vm.m_context.PopValue();
	if (!std::holds_alternative<Core::PointerPtr>(value))
	{
		return Fail(vm.m_context, "Can only dereference pointer types");
	}

	vm.m_context.PushValue(std::get<Core::PointerPtr>(value)->get());
	return 0;
}

int VirtualMachine::DataExecutor::HandleDerefSet() const
{
	const Value val = vm.m_context.PopValue();
	const Value pointerValue = vm.m_context.PopValue();
	if (!std::holds_alternative<Core::PointerPtr>(pointerValue))
	{
		return Fail(vm.m_context, "Can only dereference pointer types");
	}

	std::get<Core::PointerPtr>(pointerValue)->set(val);
	vm.m_context.PushValue(val);
	return 0;
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