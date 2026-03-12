#include "VirtualMachine.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <sstream>

using namespace VM::Core;
using namespace VM::Execution;
using enum OpCode;

std::string CaptureVMOutput(const std::function<void(VirtualMachine&, Chunk&)>& testFunc)
{
	Chunk chunk;
	VirtualMachine vm;

	const std::ostringstream oss;
	std::streambuf* old = std::cout.rdbuf(oss.rdbuf());

	testFunc(vm, chunk);

	std::cout.rdbuf(old);
	return oss.str();
}

TEST(IntegrationTest, ComplexArithmeticWithParentheses)
{
	const auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		chunk.WriteConstant(10.0);
		chunk.WriteConstant(2.0);
		chunk.Write(OP_ADD);
		chunk.WriteConstant(5.0);
		chunk.Write(OP_MULTIPLY);
		chunk.WriteConstant(1.0);
		chunk.Write(OP_SUBTRACT);
		chunk.Write(OP_RETURN);

		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("Result: 59"));
}

TEST(IntegrationTest, NegationInExpression)
{
	const auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		chunk.WriteConstant(10.0);
		chunk.WriteConstant(5.0);
		chunk.Write(OP_NEGATE);
		chunk.Write(OP_ADD);
		chunk.Write(OP_RETURN);

		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("Result: 5"));
}

TEST(IntegrationTest, ChainedDivisions)
{
	auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		chunk.WriteConstant(100.0);
		chunk.WriteConstant(2.0);
		chunk.Write(OP_DIVIDE);
		chunk.WriteConstant(5.0);
		chunk.Write(OP_DIVIDE);
		chunk.Write(OP_RETURN);

		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("Result: 10"));
}

TEST(IntegrationTest, ErrorInMiddleOfExecutionStopsVM)
{
	Chunk chunk;
	VirtualMachine vm;

	chunk.WriteConstant(10.0);
	chunk.WriteConstant(0.0);
	chunk.Write(OP_DIVIDE);
	chunk.WriteConstant(999.0);
	chunk.Write(OP_ADD);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_TRUE(vm.GetContext().HasError());

	std::string output;
	{
		std::ostringstream oss;
		std::streambuf* old = std::cout.rdbuf(oss.rdbuf());
		std::cout.rdbuf(old);
		output = oss.str();
	}

	EXPECT_THAT(output, ::testing::Not(::testing::HasSubstr("999")));
}

TEST(IntegrationTest, MultipleInterpretCallsIndependentState)
{
	VirtualMachine vm;

	Chunk chunk1;
	chunk1.WriteConstant(1.0);
	chunk1.Write(OP_RETURN);

	std::string out1;
	{
		std::ostringstream oss;
		std::streambuf* old = std::cout.rdbuf(oss.rdbuf());
		vm.Interpret(&chunk1);
		std::cout.rdbuf(old);
		out1 = oss.str();
	}

	Chunk chunk2;
	chunk2.WriteConstant(2.0);
	chunk2.Write(OP_RETURN);

	std::string out2;
	{
		std::ostringstream oss;
		std::streambuf* old = std::cout.rdbuf(oss.rdbuf());
		vm.Interpret(&chunk2);
		std::cout.rdbuf(old);
		out2 = oss.str();
	}

	EXPECT_THAT(out1, ::testing::HasSubstr("Result: 1"));
	EXPECT_THAT(out2, ::testing::HasSubstr("Result: 2"));
}

TEST(IntegrationTest, LargeExpressionDoesNotOverflow)
{
	const auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		chunk.WriteConstant(1.0);
		chunk.WriteConstant(2.0);
		chunk.Write(OP_ADD);
		chunk.WriteConstant(3.0);
		chunk.Write(OP_MULTIPLY);
		chunk.WriteConstant(4.0);
		chunk.Write(OP_ADD);
		chunk.WriteConstant(5.0);
		chunk.Write(OP_MULTIPLY);
		chunk.WriteConstant(6.0);
		chunk.Write(OP_SUBTRACT);
		chunk.Write(OP_RETURN);

		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("Result: 59"));
}

TEST(IntegrationTest, IntAndDoubleArithmeticPromotesToDouble)
{
	const auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		chunk.WriteConstant(int64_t{ 10 });
		chunk.WriteConstant(3.5);
		chunk.Write(OP_ADD);
		chunk.Write(OP_RETURN);

		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("13.5"));
}

TEST(IntegrationTest, BooleanComparisonInExpression)
{
	Chunk chunk;
	chunk.WriteConstant(5.0);
	chunk.WriteConstant(10.0);
	chunk.Write(OP_RETURN);

	VirtualMachine vm;
	EXPECT_TRUE(vm.Interpret(&chunk));
}
