#pragma once

#include "../core/OpCode.h"
#include "../core/Value.h"
#include "../execution/ExecutionContext.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace VM::Operations
{

class IOperationHandler
{
public:
	virtual ~IOperationHandler() = default;
	virtual int Execute(Core::OpCode opcode, Execution::ExecutionContext& ctx) = 0;
	[[nodiscard]] virtual std::string_view GetName() const = 0;
};

class BinaryArithmeticHandler : public IOperationHandler
{
public:
	using BinaryFunc = std::function<Core::Value(const Core::Value&, const Core::Value&)>;

	BinaryArithmeticHandler(Core::OpCode op, std::string_view name, BinaryFunc func)
		: m_opcode(op)
		, m_name(name)
		, m_func(std::move(func))
	{
	}

	int Execute(Core::OpCode opcode, Execution::ExecutionContext& ctx) override
	{
		if (opcode != m_opcode)
			return -1;

		try
		{
			if (ctx.StackSize() < 2)
			{
				ctx.RaiseError("Stack underflow");
				return -1;
			}
			Core::Value rhs = ctx.PopValue();
			Core::Value lhs = ctx.PopValue();
			Core::Value result = m_func(lhs, rhs);
			ctx.PushValue(result);
			return 0;
		}
		catch (const std::exception& e)
		{
			ctx.RaiseError(e.what());
			return -1;
		}
	}

	[[nodiscard]] std::string_view GetName() const override { return m_name; }

protected:
	Core::OpCode m_opcode;
	std::string_view m_name;
	BinaryFunc m_func;
};

class UnaryHandler : public IOperationHandler
{
public:
	using UnaryFunc = std::function<Core::Value(const Core::Value&)>;

	UnaryHandler(Core::OpCode op, std::string_view name, UnaryFunc func)
		: m_opcode(op)
		, m_name(name)
		, m_func(std::move(func))
	{
	}

	int Execute(Core::OpCode opcode, Execution::ExecutionContext& ctx) override
	{
		if (opcode != m_opcode)
			return -1;

		try
		{
			if (ctx.StackSize() < 1)
			{
				ctx.RaiseError("Stack underflow");
				return -1;
			}
			Core::Value operand = ctx.PopValue();
			Core::Value result = m_func(operand);
			ctx.PushValue(result);
			return 0;
		}
		catch (const std::exception& e)
		{
			ctx.RaiseError(e.what());
			return -1;
		}
	}

	[[nodiscard]] std::string_view GetName() const override { return m_name; }

protected:
	Core::OpCode m_opcode;
	std::string_view m_name;
	UnaryFunc m_func;
};

class OperationRegistry
{
public:
	using HandlerPtr = std::unique_ptr<IOperationHandler>;

	static OperationRegistry& Instance();

	bool Register(Core::OpCode opcode, HandlerPtr handler);
	[[nodiscard]] IOperationHandler* Get(Core::OpCode opcode) const;
	[[nodiscard]] bool Has(Core::OpCode opcode) const;

private:
	OperationRegistry() = default;
	std::unordered_map<Core::OpCode, HandlerPtr> m_handlers;
};

template <typename Handler, typename... Args>
bool RegisterOperation(Core::OpCode opcode, Args&&... args)
{
	return OperationRegistry::Instance().Register(
		opcode,
		std::make_unique<Handler>(opcode, std::forward<Args>(args)...));
}

void RegisterBuiltInOperations();

} // namespace VM::Operations