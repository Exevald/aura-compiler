#include "../core/values/ValueHelper.h"
#include "../runtime/stdlib/DiagnosticsModule.h"
#include "VirtualMachine.h"
#include "common/VirtualMachineRuntimeSupport.h"

#include <algorithm>
#include <utility>

namespace VM::Execution
{

using Core::Value;
using Detail::Fail;
using Detail::ReadErrorMessage;
using Detail::ReadStringConstant;
using Detail::RuntimeError;

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

int VirtualMachine::DataExecutor::BuildActor(const uint16_t blueprintOperand, const uint8_t fieldCount) const
{
	const auto typeName = ReadStringConstant(vm.m_context.CurrentFrame().function->chunk->constants, blueprintOperand, vm.m_context);
	if (!typeName)
	{
		return RuntimeError;
	}

	auto actor = std::make_shared<Core::Actor>();
	actor->typeName = *typeName;
	actor->state = std::make_shared<Core::Instance>();
	actor->state->fields.resize(fieldCount);

	for (int i = fieldCount - 1; i >= 0; --i)
	{
		actor->state->fields[i] = vm.m_context.PopValue();
	}

	Value methodTableValue;
	if (!vm.m_context.GetGlobal(actor->typeName + ".__methods", methodTableValue)
		|| !std::holds_alternative<Core::ActorMethodMapPtr>(methodTableValue))
	{
		return Fail(vm.m_context, "Missing actor method table for " + actor->typeName);
	}

	actor->methods = std::get<Core::ActorMethodMapPtr>(methodTableValue);
	actor->runtimeId = vm.m_runtime->RegisterActor(actor);
	vm.StartActorWorker(actor);
	vm.m_context.PushValue(actor);
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
	const size_t absoluteIndex = vm.m_context.CurrentFrame().stackBase + slot;
	auto ptr = std::make_shared<Core::Pointer>();
	ptr->targetName = "&local:" + std::to_string(slot);
	ptr->get = [this, absoluteIndex] { return vm.m_context.GetAt(absoluteIndex); };
	ptr->set = [this, absoluteIndex](const Value& val) { vm.m_context.SetAt(absoluteIndex, val); };
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

} // namespace VM::Execution
