#include "../../core/values/ValueHelper.h"
#include "VirtualMachine.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <sstream>

using namespace VM::Core;
using namespace VM::Execution;
using enum OpCode;
using VM::Core::Function;

class VMFunctionTest : public ::testing::Test
{
protected:
	VirtualMachine vm;

	std::string RunAndCapture(const Chunk& chunk)
	{
		const std::ostringstream oss;
		std::streambuf* old = std::cout.rdbuf(oss.rdbuf());

		if (vm.Interpret(&chunk))
		{
			if (vm.GetContext().StackSize() > 0)
			{
				std::cout << "Result: ";
				const auto val = vm.GetContext().PeekValue(0);
				ValueHelper::PrintValue(val, std::cout);
			}
		}
		else if (vm.GetContext().HasError())
		{
			std::cout << "Error: " << vm.GetContext().GetError();
		}

		std::cout.rdbuf(old);
		return oss.str();
	}
};

TEST_F(VMFunctionTest, DefineAndCallFunction)
{
	auto addFunc = std::make_shared<Function>();
	addFunc->name = "add";
	addFunc->arity = 2;

	addFunc->chunk->Write(OP_GET_LOCAL);
	addFunc->chunk->code.push_back(0);
	addFunc->chunk->Write(OP_GET_LOCAL);
	addFunc->chunk->code.push_back(1);
	addFunc->chunk->Write(OP_ADD);
	addFunc->chunk->Write(OP_RETURN);

	Chunk mainChunk;
	const uint8_t funcIdx = mainChunk.AddConstant(addFunc);

	mainChunk.Write(OP_CONSTANT);
	mainChunk.code.push_back(funcIdx);

	mainChunk.WriteConstant(15.0);
	mainChunk.WriteConstant(27.0);

	mainChunk.Write(OP_CALL);
	mainChunk.code.push_back(2);

	mainChunk.Write(OP_RETURN);

	const std::string output = RunAndCapture(mainChunk);

	EXPECT_THAT(output, ::testing::HasSubstr("Result: 42"));
}

TEST_F(VMFunctionTest, NestedFunctionCalls)
{
	auto squareFunc = std::make_shared<Function>();
	squareFunc->arity = 1;
	squareFunc->chunk->Write(OP_GET_LOCAL);
	squareFunc->chunk->code.push_back(0);
	squareFunc->chunk->Write(OP_GET_LOCAL);
	squareFunc->chunk->code.push_back(0);
	squareFunc->chunk->Write(OP_MULTIPLY);
	squareFunc->chunk->Write(OP_RETURN);

	auto doubleSquare = std::make_shared<Function>();
	doubleSquare->arity = 1;
	uint8_t sqIdx = doubleSquare->chunk->AddConstant(squareFunc);

	doubleSquare->chunk->Write(OP_CONSTANT);
	doubleSquare->chunk->code.push_back(sqIdx);
	doubleSquare->chunk->Write(OP_GET_LOCAL);
	doubleSquare->chunk->code.push_back(0);
	doubleSquare->chunk->Write(OP_CALL);
	doubleSquare->chunk->code.push_back(1);

	doubleSquare->chunk->Write(OP_CONSTANT);
	doubleSquare->chunk->code.push_back(sqIdx);
	doubleSquare->chunk->Write(OP_GET_LOCAL);
	doubleSquare->chunk->code.push_back(0);
	doubleSquare->chunk->Write(OP_CALL);
	doubleSquare->chunk->code.push_back(1);

	doubleSquare->chunk->Write(OP_ADD);
	doubleSquare->chunk->Write(OP_RETURN);

	Chunk mainChunk;
	mainChunk.WriteConstant(doubleSquare);
	mainChunk.WriteConstant(3.0);
	mainChunk.Write(OP_CALL);
	mainChunk.code.push_back(1);
	mainChunk.Write(OP_RETURN);

	const std::string output = RunAndCapture(mainChunk);

	EXPECT_THAT(output, ::testing::HasSubstr("Result: 18"));
}

TEST_F(VMFunctionTest, RecursiveSum)
{
	auto recFunc = std::make_shared<Function>();
	recFunc->name = "recursiveSum";
	recFunc->arity = 1;

	recFunc->chunk->Write(OP_GET_LOCAL);
	recFunc->chunk->code.push_back(0);

	recFunc->chunk->Write(OP_JUMP_IF_FALSE);
	recFunc->chunk->code.push_back(0);
	recFunc->chunk->code.push_back(13);

	recFunc->chunk->WriteConstant(recFunc);
	recFunc->chunk->Write(OP_GET_LOCAL);
	recFunc->chunk->code.push_back(0);
	recFunc->chunk->WriteConstant(1.0);
	recFunc->chunk->Write(OP_SUBTRACT);
	recFunc->chunk->Write(OP_CALL);
	recFunc->chunk->code.push_back(1);
	recFunc->chunk->Write(OP_GET_LOCAL);
	recFunc->chunk->code.push_back(0);
	recFunc->chunk->Write(OP_ADD);
	recFunc->chunk->Write(OP_RETURN);

	recFunc->chunk->WriteConstant(0.0);
	recFunc->chunk->Write(OP_RETURN);

	Chunk mainChunk;
	mainChunk.WriteConstant(recFunc);
	mainChunk.WriteConstant(5.0);
	mainChunk.Write(OP_CALL);
	mainChunk.code.push_back(1);
	mainChunk.Write(OP_RETURN);

	const std::string output = RunAndCapture(mainChunk);
	EXPECT_THAT(output, ::testing::HasSubstr("Result: 15"));
}