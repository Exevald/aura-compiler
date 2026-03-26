#include "../../core/values/ValueHelper.h"
#include "../src/vm/execution/VirtualMachine.h"

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

	std::ostringstream oss;
	std::streambuf* old = std::cout.rdbuf(oss.rdbuf());

	testFunc(vm, chunk);

	if (!vm.GetContext().HasError() && vm.GetContext().StackSize() > 0)
	{
		std::cout << "Result: ";
		ValueHelper::PrintValue(vm.GetContext().PeekValue(0), std::cout);
	}

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
		chunk.code.push_back(2);

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
		chunk.code.push_back(2);

		chunk.WriteConstant(true);

		chunk.WriteConstant(false);
		chunk.Write(OP_RETURN);
		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("Result: false"));
}

TEST(IntegrationTest, DropAndTakeIterator)
{
	const auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		chunk.WriteConstant(10.0);
		chunk.WriteConstant(20.0);
		chunk.WriteConstant(30.0);
		chunk.WriteConstant(40.0);
		chunk.Write(OP_BUILD_ARRAY);
		chunk.code.push_back(4);
		chunk.Write(OP_MAKE_ITER);

		chunk.WriteConstant(1.0);
		chunk.Write(OP_ITER_DROP);
		chunk.WriteConstant(2.0);
		chunk.Write(OP_ITER_TAKE);

		const size_t start = chunk.GetCodeSize();
		chunk.Write(OP_ITER_NEXT);
		chunk.code.push_back(4);

		chunk.Write(OP_PRINT);

		const auto offset = static_cast<uint16_t>(chunk.GetCodeSize() - start + 3);
		chunk.Write(OP_LOOP);
		chunk.code.push_back(offset >> 8);
		chunk.code.push_back(offset & 0xFF);

		chunk.Write(OP_RETURN);
		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("20"));
	EXPECT_THAT(output, ::testing::HasSubstr("30"));
}

TEST(IntegrationTest, TransformIterator)
{
	const auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		auto multiplyFn = std::make_shared<Function>();
		multiplyFn->arity = 1;
		multiplyFn->chunk->Write(OP_GET_LOCAL);
		multiplyFn->chunk->code.push_back(0);
		multiplyFn->chunk->WriteConstant(2.0);
		multiplyFn->chunk->Write(OP_MULTIPLY);
		multiplyFn->chunk->Write(OP_RETURN);

		chunk.WriteConstant(1.0);
		chunk.WriteConstant(5.0);
		chunk.Write(OP_BUILD_ARRAY);
		chunk.code.push_back(2);
		chunk.Write(OP_MAKE_ITER);
		chunk.WriteConstant(multiplyFn);
		chunk.Write(OP_ITER_TRANSFORM);

		size_t start = chunk.GetCodeSize();
		chunk.Write(OP_ITER_NEXT);
		chunk.code.push_back(4);

		chunk.Write(OP_PRINT);

		const auto offset = static_cast<uint16_t>(chunk.GetCodeSize() - start + 3);
		chunk.Write(OP_LOOP);
		chunk.code.push_back(offset >> 8);
		chunk.code.push_back(offset & 0xFF);

		chunk.Write(OP_RETURN);
		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("2"));
	EXPECT_THAT(output, ::testing::HasSubstr("10"));
}

TEST(IntegrationTest, Filter)
{
	const auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		auto filterFn = std::make_shared<Function>();
		filterFn->arity = 1;
		filterFn->chunk->Write(OP_GET_LOCAL);
		filterFn->chunk->code.push_back(0);
		filterFn->chunk->WriteConstant(15.0);
		filterFn->chunk->Write(OP_GREATER);
		filterFn->chunk->Write(OP_RETURN);

		chunk.WriteConstant(10.0);
		chunk.WriteConstant(20.0);
		chunk.WriteConstant(5.0);
		chunk.WriteConstant(30.0);
		chunk.Write(OP_BUILD_ARRAY);
		chunk.code.push_back(4);
		chunk.Write(OP_MAKE_ITER);

		chunk.WriteConstant(filterFn);
		chunk.Write(OP_ITER_FILTER);

		size_t start = chunk.GetCodeSize();
		chunk.Write(OP_ITER_NEXT);
		chunk.code.push_back(4);

		chunk.Write(OP_PRINT);

		const auto offset = static_cast<uint16_t>(chunk.GetCodeSize() - start + 3);
		chunk.Write(OP_LOOP);
		chunk.code.push_back(offset >> 8);
		chunk.code.push_back(offset & 0xFF);

		chunk.Write(OP_RETURN);
		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("20"));
	EXPECT_THAT(output, ::testing::HasSubstr("30"));
}

TEST(IntegrationTest, ComplexChain)
{
	const auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		chunk.WriteConstant(1.0);
		chunk.WriteConstant(2.0);
		chunk.WriteConstant(3.0);
		chunk.Write(OP_BUILD_ARRAY);
		chunk.code.push_back(3);
		chunk.Write(OP_MAKE_ITER);
		chunk.Write(OP_ITER_REVERSE);

		auto m10 = std::make_shared<Function>();
		m10->arity = 1;
		m10->chunk->Write(OP_GET_LOCAL);
		m10->chunk->code.push_back(0);
		m10->chunk->WriteConstant(10.0);
		m10->chunk->Write(OP_MULTIPLY);
		m10->chunk->Write(OP_RETURN);

		chunk.WriteConstant(m10);
		chunk.Write(OP_ITER_TRANSFORM);

		chunk.WriteConstant(1.0);
		chunk.Write(OP_ITER_DROP);

		const size_t start = chunk.GetCodeSize();
		chunk.Write(OP_ITER_NEXT);
		chunk.code.push_back(4);

		chunk.Write(OP_PRINT);

		const auto offset = static_cast<uint16_t>(chunk.GetCodeSize() - start + 3);
		chunk.Write(OP_LOOP);
		chunk.code.push_back(offset >> 8);
		chunk.code.push_back(offset & 0xFF);

		chunk.Write(OP_RETURN);
		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("20"));
	EXPECT_THAT(output, ::testing::HasSubstr("10"));
}