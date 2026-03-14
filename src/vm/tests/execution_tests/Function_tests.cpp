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
		std::ostringstream oss;
		std::streambuf* old = std::cout.rdbuf(oss.rdbuf());
		vm.Interpret(&chunk);
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

TEST_F(VMFunctionTest, RecursiveFactorial)
{
	auto factFunc = std::make_shared<Function>();
	factFunc->name = "factorial";
	factFunc->arity = 1;

	factFunc->chunk->Write(OP_GET_LOCAL);
	factFunc->chunk->code.push_back(0);

	factFunc->chunk->Write(OP_JUMP_IF_FALSE);
	factFunc->chunk->code.push_back(17);

	factFunc->chunk->WriteConstant(factFunc);
	factFunc->chunk->Write(OP_GET_LOCAL);
	factFunc->chunk->code.push_back(0);
	factFunc->chunk->WriteConstant(1.0);
	factFunc->chunk->Write(OP_SUBTRACT);
	factFunc->chunk->Write(OP_CALL);
	factFunc->chunk->code.push_back(1);

	factFunc->chunk->Write(OP_GET_LOCAL);
	factFunc->chunk->code.push_back(0);
	factFunc->chunk->Write(OP_ADD);
	factFunc->chunk->Write(OP_RETURN);

	factFunc->chunk->WriteConstant(0.0);
	factFunc->chunk->Write(OP_RETURN);

	Chunk mainChunk;
	mainChunk.WriteConstant(factFunc);
	mainChunk.WriteConstant(5.0);
	mainChunk.Write(OP_CALL);
	mainChunk.code.push_back(1);
	mainChunk.Write(OP_RETURN);

	std::string output = RunAndCapture(mainChunk);
	EXPECT_THAT(output, ::testing::HasSubstr("Result: 15"));
}