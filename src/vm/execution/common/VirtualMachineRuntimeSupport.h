#pragma once

#include "../VirtualMachine.h"
#include "../../core/values/ValueHelper.h"

namespace VM::Execution::Detail
{

inline constexpr int RuntimeError = -1;

inline int Fail(ExecutionContext& context, const std::string& message)
{
	context.RaiseError(message);
	return RuntimeError;
}

inline std::optional<std::string> ReadErrorMessage(ExecutionContext& context)
{
	if (!context.HasError())
	{
		return std::nullopt;
	}
	return std::string(context.GetError());
}

inline std::optional<std::string> ReadStringConstant(
	const std::vector<Core::Value>& constants,
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

} // namespace VM::Execution::Detail