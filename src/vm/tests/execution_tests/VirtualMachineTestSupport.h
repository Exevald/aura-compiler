#pragma once

#include "../../core/values/ValueHelper.h"
#include "../../runtime/ExecutionContext.h"
#include "VirtualMachine.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <sstream>

using namespace VM::Core;
using enum OpCode;
using VM::Execution::Chunk;
using VM::Execution::ExecutionContext;
using VM::Execution::VirtualMachine;

class VirtualMachineTest : public ::testing::Test
{
protected:
	VirtualMachine vm;

	static uint8_t AddStringConstant(Chunk& chunk, const std::string& value)
	{
		return chunk.AddConstant(std::make_shared<const std::string>(value));
	}

	static void WriteUint8Operand(Chunk& chunk, const uint8_t operand)
	{
		chunk.code.push_back(operand);
	}

	static void WriteGetGlobal(Chunk& chunk, const std::string& name)
	{
		chunk.Write(OP_GET_GLOBAL);
		WriteUint8Operand(chunk, AddStringConstant(chunk, name));
	}

	static void WriteGetModuleMember(Chunk& chunk, const std::string& member)
	{
		chunk.Write(OP_GET_MODULE_MEMBER);
		WriteUint8Operand(chunk, AddStringConstant(chunk, member));
	}

	static void WriteDefineGlobal(Chunk& chunk, const std::string& name)
	{
		chunk.Write(OP_DEFINE_GLOBAL);
		WriteUint8Operand(chunk, AddStringConstant(chunk, name));
	}

	static void WriteActorMessage(Chunk& chunk, const OpCode opcode, const std::string& methodName, const uint8_t argCount)
	{
		chunk.Write(opcode);
		WriteUint8Operand(chunk, AddStringConstant(chunk, methodName));
		WriteUint8Operand(chunk, argCount);
	}

	static FunctionPtr MakeActorIncrementMethod()
	{
		auto fn = std::make_shared<Function>();
		fn->name = "Counter.inc";
		fn->arity = 2;
		fn->chunk->Write(OP_GET_LOCAL);
		fn->chunk->code.push_back(0);
		fn->chunk->Write(OP_GET_LOCAL);
		fn->chunk->code.push_back(0);
		fn->chunk->Write(OP_MEMBER_GET);
		fn->chunk->code.push_back(0);
		fn->chunk->Write(OP_GET_LOCAL);
		fn->chunk->code.push_back(1);
		fn->chunk->Write(OP_ADD);
		fn->chunk->Write(OP_MEMBER_SET);
		fn->chunk->code.push_back(0);
		fn->chunk->Write(OP_RETURN);
		return fn;
	}

	static FunctionPtr MakeActorGetMethod()
	{
		auto fn = std::make_shared<Function>();
		fn->name = "Counter.get";
		fn->arity = 1;
		fn->chunk->Write(OP_GET_LOCAL);
		fn->chunk->code.push_back(0);
		fn->chunk->Write(OP_MEMBER_GET);
		fn->chunk->code.push_back(0);
		fn->chunk->Write(OP_RETURN);
		return fn;
	}

	static FunctionPtr MakeActorFailMethod()
	{
		auto fn = std::make_shared<Function>();
		fn->name = "Counter.fail";
		fn->arity = 1;
		fn->chunk->WriteConstant(1.0);
		fn->chunk->WriteConstant(0.0);
		fn->chunk->Write(OP_DIVIDE);
		fn->chunk->Write(OP_RETURN);
		return fn;
	}

	static void WriteCounterMethodTable(Chunk& chunk, const bool includeFail = false)
	{
		chunk.WriteConstant(MakeActorIncrementMethod());
		WriteDefineGlobal(chunk, "Counter.inc");

		chunk.WriteConstant(MakeActorGetMethod());
		WriteDefineGlobal(chunk, "Counter.get");

		if (includeFail)
		{
			chunk.WriteConstant(MakeActorFailMethod());
			WriteDefineGlobal(chunk, "Counter.fail");
		}

		WriteGetGlobal(chunk, "Counter.inc");
		chunk.WriteConstant(std::make_shared<const std::string>("inc"));
		WriteGetGlobal(chunk, "Counter.get");
		chunk.WriteConstant(std::make_shared<const std::string>("get"));
		if (includeFail)
		{
			WriteGetGlobal(chunk, "Counter.fail");
			chunk.WriteConstant(std::make_shared<const std::string>("fail"));
		}
		chunk.Write(OP_BUILD_ACTOR_METHODS);
		WriteUint8Operand(chunk, includeFail ? 3 : 2);
		WriteDefineGlobal(chunk, "Counter.__methods");
	}

	static Chunk MakeReturnChunk(double value)
	{
		Chunk chunk;
		chunk.WriteConstant(value);
		chunk.Write(OP_RETURN);
		return chunk;
	}

	static std::string CaptureOutput(const std::function<void()>& func)
	{
		const std::ostringstream oss;
		std::streambuf* old = std::cout.rdbuf(oss.rdbuf());

		func();

		std::cout.rdbuf(old);
		return oss.str();
	}

	std::string RunAndCapture(Chunk& chunk)
	{
		std::ostringstream oss;
		std::streambuf* old = std::cout.rdbuf(oss.rdbuf());

		if (vm.Interpret(&chunk))
		{
			if (vm.GetContext().StackSize() > 0)
			{
				std::cout << "Result: ";
				auto val = vm.GetContext().PeekValue(0);
				VM::Core::ValueHelper::PrintValue(val, std::cout);
			}
		}
		else
		{
			if (vm.GetContext().HasError())
			{
				std::cout << "Error: " << vm.GetContext().GetError();
			}
		}

		std::cout.rdbuf(old);
		return oss.str();
	}
};
