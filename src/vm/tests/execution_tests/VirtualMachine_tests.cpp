#include "ExecutionContext.h"
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
		return CaptureOutput([&] {
			vm.Interpret(&chunk);
		});
	}
};

TEST_F(VirtualMachineTest, InterpretNullChunkReturnsFalse)
{
	EXPECT_FALSE(vm.Interpret(nullptr));
}

TEST_F(VirtualMachineTest, InterpretEmptyChunkSuccess)
{
	Chunk chunk;
	EXPECT_TRUE(vm.Interpret(&chunk));
}

TEST_F(VirtualMachineTest, ReturnPrintsValue)
{
	const Chunk chunk = MakeReturnChunk(42.0);
	const std::string output = RunAndCapture(const_cast<Chunk&>(chunk));

	EXPECT_THAT(output, ::testing::HasSubstr("Result: 42"));
}

TEST_F(VirtualMachineTest, ReturnWithEmptyStackPrintsNull)
{
	Chunk chunk;
	chunk.Write(OP_RETURN);

	std::string output = RunAndCapture(chunk);

	EXPECT_THAT(output, ::testing::HasSubstr("Result: null"));
}

TEST_F(VirtualMachineTest, LoadConstantPushesValue)
{
	Chunk chunk;
	chunk.WriteConstant(123.456);
	chunk.Write(OP_RETURN);

	const std::string output = RunAndCapture(chunk);

	EXPECT_THAT(output, ::testing::HasSubstr("123.456"));
}

TEST_F(VirtualMachineTest, LoadConstantInvalidIndexRaisesError)
{
	Chunk chunk;
	chunk.Write(OP_CONSTANT);
	chunk.code.push_back(99);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_TRUE(vm.GetContext().HasError());
	EXPECT_THAT(vm.GetContext().GetError(), ::testing::HasSubstr("out of bounds"));
}

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

TEST_F(VirtualMachineTest, MaxStepsEnforcesTimeout)
{
	Chunk chunk;
	chunk.Write(OP_JUMP);
	chunk.code.push_back(0);

	vm.SetMaxSteps(10);
	EXPECT_FALSE(vm.Interpret(&chunk));
}

TEST_F(VirtualMachineTest, DebugModePrintsInstructions)
{
	Chunk chunk;
	chunk.WriteConstant(42.0);
	chunk.Write(OP_RETURN);

	vm.SetDebugMode(true);
	const std::string output = RunAndCapture(chunk);

	EXPECT_THAT(output, ::testing::HasSubstr("OP_CONSTANT"));
	EXPECT_THAT(output, ::testing::HasSubstr("OP_RETURN"));
}

TEST_F(VirtualMachineTest, RegisterExtensionCallsHandler)
{
	Chunk chunk;
	constexpr auto OP_TEST = static_cast<OpCode>(0xF0);

	bool handlerCalled = false;
	vm.RegisterExtension(OP_TEST, [&](OpCode, ExecutionContext& ctx) {
		handlerCalled = true;
		ctx.PushValue(999.0);
		return 0;
	});

	chunk.Write(OP_TEST);
	chunk.Write(OP_RETURN);

	const std::string output = RunAndCapture(chunk);

	EXPECT_TRUE(handlerCalled);
	EXPECT_THAT(output, ::testing::HasSubstr("Result: 999"));
}

TEST_F(VirtualMachineTest, ExceptionCaughtInRunLoop)
{
	Chunk chunk;
	chunk.Write(OP_ADD);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_TRUE(vm.GetContext().HasError());
	EXPECT_THAT(std::string(vm.GetContext().GetError()), ::testing::HasSubstr("underflow"));
}

TEST_F(VirtualMachineTest, ImplicitReturnAtEndOfChunk)
{
	Chunk chunk;
	chunk.WriteConstant(10.0);
	EXPECT_TRUE(vm.Interpret(&chunk));
}

TEST_F(VirtualMachineTest, OpReturnCleansStack)
{
	Chunk chunk;
	chunk.WriteConstant(1.0);
	chunk.WriteConstant(2.0);
	chunk.WriteConstant(42.0);
	chunk.Write(OP_RETURN);

	RunAndCapture(chunk);
	EXPECT_EQ(vm.GetContext().StackSize(), 1);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(vm.GetContext().PeekValue(0)), 42.0);
}

TEST_F(VirtualMachineTest, UnknownOpcodeErrorHandling)
{
	Chunk chunk;
	chunk.code.push_back(0xEE);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_THAT(std::string(vm.GetContext().GetError()), ::testing::HasSubstr("Unknown opcode"));
}