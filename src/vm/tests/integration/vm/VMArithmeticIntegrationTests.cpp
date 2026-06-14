#include "../../support/BytecodeVmIntegrationSupport.h"

#include <gmock/gmock-matchers.h>

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

TEST(IntegrationTest, IntAndDoubleArithmeticPromotesToDouble)
{
	const auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		chunk.WriteConstant(int64_t{ 10 });
		chunk.WriteConstant(3.5);
		chunk.Write(OP_ADD);
		chunk.Write(OP_RETURN);

		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("Result: 13.5"));
}

TEST(IntegrationTest, ShortCircuitOr)
{
	const auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		chunk.WriteConstant(true);

			chunk.Write(OP_JUMP_IF_TRUE);
			chunk.code.push_back(0);
			chunk.code.push_back(3);

		chunk.WriteConstant(false);

		chunk.WriteConstant(true);
		chunk.Write(OP_RETURN);
		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("Result: true"));
}

TEST(IntegrationTest, ShortCircuitAnd)
{
	const auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		chunk.WriteConstant(false);

			chunk.Write(OP_JUMP_IF_FALSE);
			chunk.code.push_back(0);
			chunk.code.push_back(3);

		chunk.WriteConstant(true);

		chunk.WriteConstant(false);
		chunk.Write(OP_RETURN);
		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("Result: false"));
}
