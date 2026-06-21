#include "VirtualMachineTestSupport.h"

#include <limits>

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
	chunk.WriteOperand(OP_CONSTANT, 99);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_TRUE(vm.GetContext().HasError());
	EXPECT_THAT(vm.GetContext().GetError(), ::testing::HasSubstr("out of bounds"));
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

TEST_F(VirtualMachineTest, EmptyReverseIteratorDoesNotUnderflow)
{
	Chunk chunk;
	chunk.Write(OP_BUILD_ARRAY);
	chunk.code.push_back(0);
	chunk.Write(OP_MAKE_ITER);
	chunk.Write(OP_ITER_REVERSE);
	chunk.Write(OP_ITER_NEXT);
	chunk.code.push_back(0);
	chunk.Write(OP_RETURN);

	EXPECT_TRUE(vm.Interpret(&chunk));
}

TEST_F(VirtualMachineTest, LoopWithOversizedOffsetFailsGracefully)
{
	Chunk chunk;
	chunk.Write(OP_LOOP);
	chunk.WriteOperand(OP_LOOP, std::numeric_limits<uint16_t>::max());

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_TRUE(vm.GetContext().HasError());
	EXPECT_THAT(std::string(vm.GetContext().GetError()), ::testing::HasSubstr("Loop offset"));
}

TEST_F(VirtualMachineTest, IteratorTransformPropagatesNestedError)
{
	Chunk chunk;
	chunk.WriteConstant(1.0);
	chunk.Write(OP_BUILD_ARRAY);
	chunk.code.push_back(1);
	chunk.Write(OP_MAKE_ITER);

	auto fn = std::make_shared<Function>();
	fn->name = "bad_transform";
	fn->arity = 1;
	fn->chunk->Write(OP_ADD);
	fn->chunk->Write(OP_RETURN);

	chunk.WriteConstant(fn);
	chunk.Write(OP_ITER_TRANSFORM);
	chunk.Write(OP_ITER_NEXT);
	chunk.code.push_back(0);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_TRUE(vm.GetContext().HasError());
	EXPECT_THAT(std::string(vm.GetContext().GetError()), ::testing::HasSubstr("underflow"));
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
