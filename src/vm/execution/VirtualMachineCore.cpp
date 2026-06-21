#include "../core/values/ValueHelper.h"
#include "../runtime/stdlib/BuiltinModuleInstaller.h"
#include "../runtime/stdlib/core/CoreModule.h"
#include "../runtime/stdlib/memory/MemoryModule.h"
#include "VirtualMachine.h"
#include "common/VirtualMachineRuntimeSupport.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace VM::Execution
{

using Core::Value;
using Detail::Fail;
using Detail::ReadErrorMessage;
using Detail::ReadStringConstant;
using Detail::RuntimeError;

VirtualMachine::VirtualMachine()
	: VirtualMachine(std::make_shared<Runtime::SharedRuntime>(), true)
{
}

VirtualMachine::VirtualMachine(
	std::shared_ptr<Runtime::SharedRuntime> runtime,
	const bool installStdlib)
	: m_runtime(std::move(runtime))
	, m_context(m_runtime, installStdlib)
{
	m_context.SetCallableInvoker(
		[this](const Value& callee, const std::vector<Value>& args, Value& result) {
			return InvokeCallable(callee, args, result);
		});
	if (installStdlib)
	{
		InstallStdlib();
	}
}

void VirtualMachine::InstallStdlib() const
{
	Runtime::InstallBuiltinStdlib(*m_runtime);
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
		m_context.UnwindAllTransactions();
		m_context.ClearHandlers();
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
			const size_t codeSize = frame.function->chunk->GetCodeSize();

			if (frame.ip >= codeSize)
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
			const auto requireBytes = [&](const size_t count) -> bool {
				if (frame.ip + count > codeSize)
				{
					m_context.RaiseError("Truncated bytecode stream");
					return false;
				}
				return true;
			};

			if (const auto opSize = Core::GetOperandSize(opcode);
				opSize == Core::OperandSize::Uint8)
			{
				if (!requireBytes(1))
				{
					return ExecutionResult::RuntimeError;
				}
				operand = frame.function->chunk->GetCode()[frame.ip++];
			}
			else if (opSize == Core::OperandSize::Uint16)
			{
				if (!requireBytes(2))
				{
					return ExecutionResult::RuntimeError;
				}
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
	const size_t codeSize = frame.function->chunk->GetCodeSize();
	const auto ensureRemaining = [&](const size_t count) -> bool {
		if (frame.ip + count > codeSize)
		{
			return Fail(m_context, "Truncated bytecode stream");
		}
		return true;
	};

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
			if (instr.operand > codeSize - frame.ip)
			{
				return Fail(m_context, "Jump exceeds bytecode bounds");
			}
			frame.ip += instr.operand;
		}
		return 0;
	}

	case OP_JUMP_IF_FALSE: {
		if (bool condition = Core::ValueHelper::As<bool>(m_context.PopValue());
			!condition)
		{
			if (instr.operand > codeSize - frame.ip)
			{
				return Fail(m_context, "Jump exceeds bytecode bounds");
			}
			frame.ip += instr.operand;
		}
		return 0;
	}

	case OP_JUMP: {
		if (instr.operand > codeSize - frame.ip)
		{
			return Fail(m_context, "Jump exceeds bytecode bounds");
		}
		frame.ip += instr.operand;
		return 0;
	}

	case OP_LOOP: {
		if (instr.operand > frame.ip)
		{
			return Fail(m_context, "Loop offset exceeds current instruction pointer");
		}
		frame.ip -= instr.operand;
		return 0;
	}

	case OP_POP: {
		m_context.PopValue();
		return 0;
	}

	case OP_DUP:
		m_context.PushValue(m_context.PeekValue(0));
		return 0;

	case OP_SWAP: {
		Value top = m_context.PopValue();
		Value next = m_context.PopValue();
		m_context.PushValue(top);
		m_context.PushValue(next);
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

	case OP_BUILD_MAP: {
		return DataExecutor{ *this }.BuildMap(static_cast<uint8_t>(instr.operand));
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

	case OP_BUILD_ACTOR: {
		if (!ensureRemaining(1))
		{
			return RuntimeError;
		}
		const uint8_t fieldCount = frame.function->chunk->code[frame.ip++];
		return DataExecutor{ *this }.BuildActor(instr.operand, fieldCount);
	}

	case OP_INDEX_SET:
		return DataExecutor{ *this }.HandleArrayIndexSet();

	case OP_MEMBER_SET:
		return DataExecutor{ *this }.HandleMemberSet(static_cast<uint8_t>(instr.operand));

	case OP_BUILD_ENUM: {
		if (!ensureRemaining(1))
		{
			return RuntimeError;
		}
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

	case OP_BEGIN_TXN: {
		const Value mutexValue = m_context.PopValue();
		if (std::holds_alternative<Core::MutexPtr>(mutexValue))
		{
			return m_context.BeginTransaction(std::get<Core::MutexPtr>(mutexValue)) ? 0 : RuntimeError;
		}
		if (std::holds_alternative<Core::ArrayPtr>(mutexValue))
		{
			const auto mutexArray = std::get<Core::ArrayPtr>(mutexValue);
			if (!mutexArray)
			{
				return Fail(m_context, "Transaction requires mutex handles");
			}
			std::vector<Core::MutexPtr> mutexes;
			mutexes.reserve(mutexArray->elements.size());
			for (const auto& element : mutexArray->elements)
			{
				if (!std::holds_alternative<Core::MutexPtr>(element))
				{
					return Fail(m_context, "Transaction requires mutex handles");
				}
				mutexes.push_back(std::get<Core::MutexPtr>(element));
			}
			return m_context.BeginTransaction(mutexes) ? 0 : RuntimeError;
		}
		return Fail(m_context, "Transaction requires a mutex handle or mutex array");
	}

	case OP_END_TXN:
		return m_context.EndTransaction() ? 0 : RuntimeError;

	case OP_MAKE_ITER: {
		Value iterable = m_context.PopValue();
		auto it = std::make_shared<Core::Iterator>();

		if (std::holds_alternative<Core::ArrayPtr>(iterable))
		{
			auto arr = std::get<Core::ArrayPtr>(iterable);
			auto index = std::make_shared<size_t>(0);

			it->next = [this, arr, index]() -> std::pair<bool, Value> {
				size_t size = 0;
				if (!m_context.GetArraySize(arr, size))
				{
					return { false, std::monostate{} };
				}
				if (*index < size)
				{
					Value value;
					if (!m_context.GetArrayElement(arr, (*index)++, value))
					{
						return { false, std::monostate{} };
					}
					return { true, value };
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
			if (jumpOffset > codeSize - frame.ip)
			{
				return Fail(m_context, "Jump exceeds bytecode bounds");
			}
			frame.ip += jumpOffset;
		}
		return 0;
	}

	case OP_ITER_TAKE: {
		auto n = Core::ValueHelper::As<int64_t>(m_context.PopValue());
		Value v = m_context.PopValue();
		if (!std::holds_alternative<Core::IteratorPtr>(v) || !std::get<Core::IteratorPtr>(v))
		{
			return Fail(m_context, "Iterator transform requires iterator input");
		}
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
		if (!std::holds_alternative<Core::IteratorPtr>(v) || !std::get<Core::IteratorPtr>(v))
		{
			return Fail(m_context, "Iterator transform requires iterator input");
		}
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
		if (!std::holds_alternative<Core::IteratorPtr>(itVal) || !std::get<Core::IteratorPtr>(itVal))
		{
			return Fail(m_context, "Iterator transform requires iterator input");
		}
		auto sourceIt = std::get<Core::IteratorPtr>(itVal);
		Core::ClosurePtr closure;
		Core::FunctionPtr fn;
		if (std::holds_alternative<Core::ClosurePtr>(fnVal))
		{
			closure = std::get<Core::ClosurePtr>(fnVal);
			fn = closure->function;
		}
		else if (std::holds_alternative<Core::FunctionPtr>(fnVal))
		{
			fn = std::get<Core::FunctionPtr>(fnVal);
			closure = std::make_shared<Core::Closure>();
			closure->function = fn;
		}
		else
		{
			return Fail(m_context, "Iterator transform requires callable input");
		}

		auto it = std::make_shared<Core::Iterator>();
		it->next = [this, sourceIt, fn, closure]() -> std::pair<bool, Value> {
			auto [hasValue, val] = sourceIt->next();
			if (!hasValue)
			{
				return { false, std::monostate{} };
			}

			const size_t stackBase = m_context.StackSize();
			const size_t frameBase = m_context.GetFramesCount();
			const size_t transactionBase = m_context.ActiveTransactionCount();
			const size_t handlerBase = m_context.ActiveHandlerCount();

			m_context.PushValue(closure);
			m_context.PushValue(val);

			const size_t argBase = m_context.StackSize() - 1;
			m_context.PushFrame(fn, closure, argBase);

			const auto result = this->Run();
			if (result != ExecutionResult::Success)
			{
				while (m_context.GetFramesCount() > frameBase)
				{
					m_context.PopFrame();
				}
				while (m_context.StackSize() > stackBase)
				{
					m_context.PopValue();
				}
				m_context.UnwindTransactions(transactionBase);
				m_context.UnwindHandlers(handlerBase);
				throw std::runtime_error(
					m_context.HasError() ? std::string(m_context.GetError()) : "Iterator transform failed");
			}

			return { true, m_context.PopValue() };
		};

		m_context.PushValue(it);
		return 0;
	}

	case OP_ITER_REVERSE: {
		Value v = m_context.PopValue();
		if (!std::holds_alternative<Core::IteratorPtr>(v) || !std::get<Core::IteratorPtr>(v))
		{
			return Fail(m_context, "Iterator transform requires iterator input");
		}
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
					if (buffer->empty())
					{
						*initialized = true;
						return { false, std::monostate{} };
					}
					*index = static_cast<int64_t>(buffer->size()) - 1;
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
		if (!std::holds_alternative<Core::IteratorPtr>(itVal) || !std::get<Core::IteratorPtr>(itVal))
		{
			return Fail(m_context, "Iterator filter requires iterator input");
		}
		auto sourceIt = std::get<Core::IteratorPtr>(itVal);
		Core::ClosurePtr closure;
		Core::FunctionPtr fn;
		if (std::holds_alternative<Core::ClosurePtr>(fnVal))
		{
			closure = std::get<Core::ClosurePtr>(fnVal);
			fn = closure->function;
		}
		else if (std::holds_alternative<Core::FunctionPtr>(fnVal))
		{
			fn = std::get<Core::FunctionPtr>(fnVal);
			closure = std::make_shared<Core::Closure>();
			closure->function = fn;
		}
		else
		{
			return Fail(m_context, "Iterator filter requires callable input");
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

				const size_t stackBase = m_context.StackSize();
				const size_t frameBase = m_context.GetFramesCount();
				const size_t transactionBase = m_context.ActiveTransactionCount();
				const size_t handlerBase = m_context.ActiveHandlerCount();

				m_context.PushValue(closure);
				m_context.PushValue(val);

				const size_t argBase = m_context.StackSize() - 1;
				m_context.PushFrame(fn, closure, argBase);

				const auto result = this->Run();
				if (result != ExecutionResult::Success)
				{
					while (m_context.GetFramesCount() > frameBase)
					{
						m_context.PopFrame();
					}
					while (m_context.StackSize() > stackBase)
					{
						m_context.PopValue();
					}
					m_context.UnwindTransactions(transactionBase);
					m_context.UnwindHandlers(handlerBase);
					throw std::runtime_error(
						m_context.HasError() ? std::string(m_context.GetError()) : "Iterator filter failed");
				}

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

	case OP_BUILD_HANDLER: {
		auto handlers = std::make_shared<Core::EffectHandlerMap>();
		for (int i = 0; i < static_cast<int>(instr.operand); ++i)
		{
			const Value opNameValue = m_context.PopValue();
			const Value handlerValue = m_context.PopValue();
			if (!std::holds_alternative<Core::StringPtr>(opNameValue))
			{
				return Fail(m_context, "Handler build requires operation names");
			}
			handlers->handlers[*std::get<Core::StringPtr>(opNameValue)] = handlerValue;
		}
		m_context.PushValue(handlers);
		return 0;
	}

	case OP_BUILD_ACTOR_METHODS: {
		auto methods = std::make_shared<Core::ActorMethodMap>();
		for (int i = 0; i < static_cast<int>(instr.operand); ++i)
		{
			const Value methodNameValue = m_context.PopValue();
			const Value methodValue = m_context.PopValue();
			if (!std::holds_alternative<Core::StringPtr>(methodNameValue))
			{
				return Fail(m_context, "Actor method table requires method names");
			}
			methods->methods[*std::get<Core::StringPtr>(methodNameValue)] = methodValue;
		}
		m_context.PushValue(methods);
		return 0;
	}

	case OP_ASSERT: {
		const auto message = ReadStringConstant(constants, instr.operand, m_context);
		if (!message)
		{
			return RuntimeError;
		}
		if (!Core::ValueHelper::As<bool>(m_context.PopValue()))
		{
			return Fail(m_context, *message);
		}
		return 0;
	}

	case OP_PUSH_HANDLER: {
		const Value handlerValue = m_context.PopValue();
		if (!std::holds_alternative<Core::HandlerMapPtr>(handlerValue))
		{
			return Fail(m_context, "Expected handler map");
		}
		m_context.PushHandlerMap(std::get<Core::HandlerMapPtr>(handlerValue));
		return 0;
	}

	case OP_POP_HANDLER:
		return m_context.PopHandlerMap() ? 0 : RuntimeError;

	case OP_EFFECT_INVOKE: {
		const uint8_t argCount = frame.function->chunk->code[frame.ip++];
		const auto effectName = ReadStringConstant(constants, instr.operand, m_context);
		if (!effectName)
		{
			return RuntimeError;
		}
		const Value handler = m_context.ResolveHandledEffect(*effectName);
		if (std::holds_alternative<std::monostate>(handler))
		{
			return Fail(m_context, "Unhandled effect: " + *effectName);
		}
		std::vector<Value> args;
		args.reserve(argCount);
		for (size_t i = 0; i < argCount; ++i)
		{
			args.push_back(m_context.PopValue());
		}
		std::ranges::reverse(args);
		const size_t calleeIdx = m_context.StackSize();
		m_context.PushValue(handler);
		for (const auto& arg : args)
		{
			m_context.PushValue(arg);
		}
		return HandleResolvedCall(handler, argCount, calleeIdx);
	}

	case OP_ACTOR_SEND:
	case OP_ACTOR_QUERY: {
		const uint8_t argCount = frame.function->chunk->code[frame.ip++];
		const auto methodName = ReadStringConstant(constants, instr.operand, m_context);
		if (!methodName)
		{
			return RuntimeError;
		}
		const size_t actorIdx = m_context.StackSize() - argCount - 1;
		const Value actorValue = m_context.GetAt(actorIdx);
		if (!std::holds_alternative<Core::ActorPtr>(actorValue) || !std::get<Core::ActorPtr>(actorValue))
		{
			return Fail(m_context, "Actor call requires actor instance");
		}
		auto actor = std::get<Core::ActorPtr>(actorValue);
		if (!actor->methods || !actor->methods->methods.contains(*methodName))
		{
			return Fail(m_context, "Unknown actor method: " + *methodName);
		}
		std::vector<Value> args;
		args.reserve(argCount);
		for (size_t i = 0; i < argCount; ++i)
		{
			args.push_back(m_context.GetAt(actorIdx + 1 + i));
		}
		while (m_context.StackSize() > actorIdx)
		{
			m_context.PopValue();
		}
		if (instr.opcode == OP_ACTOR_SEND)
		{
			return EnqueueActorSend(actor, *methodName, std::move(args));
		}
		return EnqueueActorQuery(actor, *methodName, std::move(args));
	}

	default:
		m_context.RaiseError("Unknown opcode: "
			+ std::to_string(static_cast<uint8_t>(instr.opcode)));
		return -1;
	}
}

} // namespace VM::Execution
