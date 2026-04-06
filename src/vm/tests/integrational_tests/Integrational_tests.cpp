#include "../../core/values/ValueHelper.h"
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

TEST(IntegrationTest, BubbleSort)
{
	const auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		chunk.WriteConstant(4.0);
		chunk.WriteConstant(3.0);
		chunk.WriteConstant(1.0);
		chunk.WriteConstant(2.0);
		chunk.Write(OP_BUILD_ARRAY);
		chunk.code.push_back(4);

		chunk.WriteConstant(4.0);
		chunk.WriteConstant(0.0);
		chunk.WriteConstant(0.0);
		chunk.WriteConstant(0.0);

		const size_t outerLoopStart = chunk.GetCodeSize();
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(2);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(1);
		chunk.Write(OP_LESS);

		const size_t exitOuterJumpIdx = chunk.GetCodeSize();
		chunk.WriteJump(OP_JUMP_IF_FALSE, 0);

		chunk.WriteConstant(0.0);
		chunk.Write(OP_SET_LOCAL);
		chunk.code.push_back(3);
		chunk.Write(OP_POP);

		const size_t innerLoopStart = chunk.GetCodeSize();
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(3);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(1);
		chunk.WriteConstant(1.0);
		chunk.Write(OP_SUBTRACT);
		chunk.Write(OP_LESS);

		const size_t exitInnerJumpIdx = chunk.GetCodeSize();
		chunk.WriteJump(OP_JUMP_IF_FALSE, 0);

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(0);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(3);
		chunk.Write(OP_INDEX_GET);

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(0);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(3);
		chunk.WriteConstant(1.0);
		chunk.Write(OP_ADD);
		chunk.Write(OP_INDEX_GET);

		chunk.Write(OP_GREATER);

		const size_t skipSwapJumpIdx = chunk.GetCodeSize();
		chunk.WriteJump(OP_JUMP_IF_FALSE, 0);

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(0);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(3);
		chunk.Write(OP_INDEX_GET);
		chunk.Write(OP_SET_LOCAL);
		chunk.code.push_back(4);
		chunk.Write(OP_POP);

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(0);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(3);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(0);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(3);
		chunk.WriteConstant(1.0);
		chunk.Write(OP_ADD);
		chunk.Write(OP_INDEX_GET);
		chunk.Write(OP_INDEX_SET);
		chunk.Write(OP_POP);

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(0);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(3);
		chunk.WriteConstant(1.0);
		chunk.Write(OP_ADD);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(4);
		chunk.Write(OP_INDEX_SET);
		chunk.Write(OP_POP);

		const auto skipSwapOffset = static_cast<uint16_t>(chunk.GetCodeSize() - skipSwapJumpIdx - 3);
		chunk.code[skipSwapJumpIdx + 1] = (skipSwapOffset >> 8) & 0xFF;
		chunk.code[skipSwapJumpIdx + 2] = skipSwapOffset & 0xFF;

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(3);
		chunk.WriteConstant(1.0);
		chunk.Write(OP_ADD);
		chunk.Write(OP_SET_LOCAL);
		chunk.code.push_back(3);
		chunk.Write(OP_POP);

		const auto innerLoopOffset = static_cast<uint16_t>(chunk.GetCodeSize() - innerLoopStart + 3);
		chunk.WriteJump(OP_LOOP, innerLoopOffset);

		const auto exitInnerOffset = static_cast<uint16_t>(chunk.GetCodeSize() - exitInnerJumpIdx - 3);
		chunk.code[exitInnerJumpIdx + 1] = (exitInnerOffset >> 8) & 0xFF;
		chunk.code[exitInnerJumpIdx + 2] = exitInnerOffset & 0xFF;

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(2);
		chunk.WriteConstant(1.0);
		chunk.Write(OP_ADD);
		chunk.Write(OP_SET_LOCAL);
		chunk.code.push_back(2);
		chunk.Write(OP_POP);

		const auto outerLoopOffset = static_cast<uint16_t>(chunk.GetCodeSize() - outerLoopStart + 3);
		chunk.WriteJump(OP_LOOP, outerLoopOffset);

		const auto exitOuterOffset = static_cast<uint16_t>(chunk.GetCodeSize() - exitOuterJumpIdx - 3);
		chunk.code[exitOuterJumpIdx + 1] = (exitOuterOffset >> 8) & 0xFF;
		chunk.code[exitOuterJumpIdx + 2] = exitOuterOffset & 0xFF;

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(0);
		chunk.Write(OP_PRINT);
		chunk.Write(OP_RETURN);

		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output, ::testing::HasSubstr("[1, 2, 3, 4]"));
}

TEST(IntegrationTest, SieveOfEratosthenes)
{
	const auto output = CaptureVMOutput([](VirtualMachine& vm, Chunk& chunk) {
		constexpr int n = 10;
		for (int i = 0; i <= n; ++i)
			chunk.WriteConstant(true);
		chunk.Write(OP_BUILD_ARRAY);
		chunk.code.push_back(n + 1);

		chunk.WriteConstant(n);
		chunk.WriteConstant(2.0);
		chunk.WriteConstant(0.0);

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(0);
		chunk.WriteConstant(0.0);
		chunk.WriteConstant(false);
		chunk.Write(OP_INDEX_SET);
		chunk.Write(OP_POP);

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(0);
		chunk.WriteConstant(1.0);
		chunk.WriteConstant(false);
		chunk.Write(OP_INDEX_SET);
		chunk.Write(OP_POP);

		const size_t outerStart = chunk.GetCodeSize();
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(2);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(2);
		chunk.Write(OP_MULTIPLY);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(1);
		chunk.Write(OP_LESS_EQUAL);

		size_t exitOuterIdx = chunk.GetCodeSize();
		chunk.WriteJump(OP_JUMP_IF_FALSE, 0);

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(0);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(2);
		chunk.Write(OP_INDEX_GET);

		size_t skipInnerIdx = chunk.GetCodeSize();
		chunk.WriteJump(OP_JUMP_IF_FALSE, 0);

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(2);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(2);
		chunk.Write(OP_MULTIPLY);
		chunk.Write(OP_SET_LOCAL);
		chunk.code.push_back(3);
		chunk.Write(OP_POP);

		size_t innerStart = chunk.GetCodeSize();
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(3);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(1);
		chunk.Write(OP_LESS_EQUAL);

		size_t exitInnerIdx = chunk.GetCodeSize();
		chunk.WriteJump(OP_JUMP_IF_FALSE, 0);

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(0);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(3);
		chunk.WriteConstant(false);
		chunk.Write(OP_INDEX_SET);
		chunk.Write(OP_POP);

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(3);
		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(2);
		chunk.Write(OP_ADD);
		chunk.Write(OP_SET_LOCAL);
		chunk.code.push_back(3);
		chunk.Write(OP_POP);

		const auto innerLoopOff = static_cast<uint16_t>(chunk.GetCodeSize() - innerStart + 3);
		chunk.WriteJump(OP_LOOP, innerLoopOff);

		const auto exitInnerOff = static_cast<uint16_t>(chunk.GetCodeSize() - exitInnerIdx - 3);
		chunk.code[exitInnerIdx + 1] = (exitInnerOff >> 8) & 0xFF;
		chunk.code[exitInnerIdx + 2] = exitInnerOff & 0xFF;

		const auto skipInnerOff = static_cast<uint16_t>(chunk.GetCodeSize() - skipInnerIdx - 3);
		chunk.code[skipInnerIdx + 1] = (skipInnerOff >> 8) & 0xFF;
		chunk.code[skipInnerIdx + 2] = skipInnerOff & 0xFF;

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(2);
		chunk.WriteConstant(1.0);
		chunk.Write(OP_ADD);
		chunk.Write(OP_SET_LOCAL);
		chunk.code.push_back(2);
		chunk.Write(OP_POP);

		auto outerLoopOff = static_cast<uint16_t>(chunk.GetCodeSize() - outerStart + 3);
		chunk.WriteJump(OP_LOOP, outerLoopOff);

		auto exitOuterOff = static_cast<uint16_t>(chunk.GetCodeSize() - exitOuterIdx - 3);
		chunk.code[exitOuterIdx + 1] = (exitOuterOff >> 8) & 0xFF;
		chunk.code[exitOuterIdx + 2] = exitOuterOff & 0xFF;

		chunk.Write(OP_GET_LOCAL);
		chunk.code.push_back(0);
		chunk.Write(OP_PRINT);
		chunk.Write(OP_RETURN);

		vm.Interpret(&chunk);
	});

	EXPECT_THAT(output,
		::testing::HasSubstr("false, false, true, true, false, true, false, true, false, false, false"));
}