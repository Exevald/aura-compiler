#include "VirtualMachineTestSupport.h"

TEST_F(VirtualMachineTest, AddTwoConstants)
{
	Chunk chunk;
	chunk.WriteConstant(10.0);
	chunk.WriteConstant(5.0);
	chunk.Write(OP_ADD);
	chunk.Write(OP_RETURN);

	const std::string output = RunAndCapture(chunk);
	EXPECT_THAT(output, ::testing::HasSubstr("Result: 15"));
}

TEST_F(VirtualMachineTest, SubtractBasic)
{
	Chunk chunk;
	chunk.WriteConstant(20.0);
	chunk.WriteConstant(8.0);
	chunk.Write(OP_SUBTRACT);
	chunk.Write(OP_RETURN);

	const std::string output = RunAndCapture(chunk);
	EXPECT_THAT(output, ::testing::HasSubstr("Result: 12"));
}

TEST_F(VirtualMachineTest, MultiplyBasic)
{
	Chunk chunk;
	chunk.WriteConstant(6.0);
	chunk.WriteConstant(7.0);
	chunk.Write(OP_MULTIPLY);
	chunk.Write(OP_RETURN);

	const std::string output = RunAndCapture(chunk);
	EXPECT_THAT(output, ::testing::HasSubstr("Result: 42"));
}

TEST_F(VirtualMachineTest, DivideBasic)
{
	Chunk chunk;
	chunk.WriteConstant(100.0);
	chunk.WriteConstant(4.0);
	chunk.Write(OP_DIVIDE);
	chunk.Write(OP_RETURN);

	const std::string output = RunAndCapture(chunk);
	EXPECT_THAT(output, ::testing::HasSubstr("Result: 25"));
}

TEST_F(VirtualMachineTest, DivideByZeroRaisesError)
{
	Chunk chunk;
	chunk.WriteConstant(10.0);
	chunk.WriteConstant(0.0);
	chunk.Write(OP_DIVIDE);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_TRUE(vm.GetContext().HasError());
	EXPECT_THAT(vm.GetContext().GetError(), ::testing::HasSubstr("Division by zero"));
}

TEST_F(VirtualMachineTest, NegatePositiveToNegative)
{
	Chunk chunk;
	chunk.WriteConstant(42.0);
	chunk.Write(OP_NEGATE);
	chunk.Write(OP_RETURN);

	const std::string output = RunAndCapture(chunk);
	EXPECT_THAT(output, ::testing::HasSubstr("Result: -42"));
}

TEST_F(VirtualMachineTest, AddWithOneOperandRaisesError)
{
	Chunk chunk;
	chunk.WriteConstant(10.0);
	chunk.Write(OP_ADD);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_THAT(vm.GetContext().GetError(), ::testing::HasSubstr("underflow"));
}

TEST_F(VirtualMachineTest, NegateWithEmptyStackRaisesError)
{
	Chunk chunk;
	chunk.Write(OP_NEGATE);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_THAT(vm.GetContext().GetError(), ::testing::HasSubstr("underflow"));
}

TEST_F(VirtualMachineTest, JumpUpdatesIP)
{
	Chunk chunk;
	chunk.Write(OP_JUMP);
	chunk.code.push_back(0);
	chunk.code.push_back(0);
	chunk.Write(OP_RETURN);

	EXPECT_NO_THROW(vm.Interpret(&chunk));
}

TEST_F(VirtualMachineTest, JumpIfFalseTakesBranch)
{
	Chunk chunk;
	chunk.WriteConstant(false);
	chunk.Write(OP_JUMP_IF_FALSE);
	chunk.code.push_back(1);
	chunk.Write(OP_RETURN);

	EXPECT_NO_THROW(vm.Interpret(&chunk));
}

TEST_F(VirtualMachineTest, ComparisonInstructions)
{
	Chunk chunk;
	chunk.WriteConstant(10.0);
	chunk.WriteConstant(20.0);
	chunk.Write(OP_LESS);
	chunk.Write(OP_RETURN);
	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: true"));

	chunk.Clear();
	chunk.WriteConstant(int64_t{ 10 });
	chunk.WriteConstant(10.0);
	chunk.Write(OP_EQUAL);
	chunk.Write(OP_RETURN);
	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: true"));

	chunk.Clear();
	chunk.WriteConstant(true);
	chunk.Write(OP_NOT);
	chunk.Write(OP_RETURN);
	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: false"));
}

TEST_F(VirtualMachineTest, NotEqualInstruction)
{
	Chunk chunk;
	chunk.WriteConstant(10.0);
	chunk.WriteConstant(20.0);
	chunk.Write(OP_NOT_EQUAL);
	chunk.Write(OP_RETURN);
	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: true"));
}