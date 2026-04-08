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

TEST_F(VirtualMachineTest, BuiltinDiagnosticsModuleActiveAllocationsStartsAtZero)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.memory");
	WriteGetModuleMember(chunk, "active_allocations");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: 0"));
}

TEST_F(VirtualMachineTest, BuiltinCoreModuleMaxMinLenAndAbsReturnExpectedValues)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.core");
	WriteGetModuleMember(chunk, "max");
	chunk.WriteConstant(int64_t{ 4 });
	chunk.WriteConstant(int64_t{ 9 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "max_value");

	WriteGetGlobal(chunk, "std.core");
	WriteGetModuleMember(chunk, "min");
	chunk.WriteConstant(int64_t{ 4 });
	chunk.WriteConstant(int64_t{ 9 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "min_value");

	WriteGetGlobal(chunk, "std.core");
	WriteGetModuleMember(chunk, "len");
	chunk.WriteConstant(std::make_shared<const std::string>("aura"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	WriteDefineGlobal(chunk, "len_value");

	WriteGetGlobal(chunk, "std.core");
	WriteGetModuleMember(chunk, "abs");
	chunk.WriteConstant(int64_t{ -7 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: 7"));

	Value maxValue;
	ASSERT_TRUE(vm.GetContext().GetGlobal("max_value", maxValue));
	EXPECT_EQ(ValueHelper::As<int64_t>(maxValue), 9);

	Value minValue;
	ASSERT_TRUE(vm.GetContext().GetGlobal("min_value", minValue));
	EXPECT_EQ(ValueHelper::As<int64_t>(minValue), 4);

	Value lenValue;
	ASSERT_TRUE(vm.GetContext().GetGlobal("len_value", lenValue));
	EXPECT_EQ(ValueHelper::As<int64_t>(lenValue), 4);
}

TEST_F(VirtualMachineTest, BuiltinCoreModuleSortOrdersArrayInPlace)
{
	Chunk chunk;
	chunk.WriteConstant(int64_t{ 3 });
	chunk.WriteConstant(int64_t{ 1 });
	chunk.WriteConstant(int64_t{ 2 });
	chunk.Write(OP_BUILD_ARRAY);
	chunk.code.push_back(3);
	WriteDefineGlobal(chunk, "arr");

	WriteGetGlobal(chunk, "std.core");
	WriteGetModuleMember(chunk, "sort");
	WriteGetGlobal(chunk, "arr");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "arr");
	chunk.WriteConstant(int64_t{ 0 });
	chunk.Write(OP_INDEX_GET);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: 1"));
}

TEST_F(VirtualMachineTest, BuiltinIoModulePrintWritesValue)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.io");
	WriteGetModuleMember(chunk, "print");
	chunk.WriteConstant(std::make_shared<const std::string>("hello"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("hello"));
}

TEST_F(VirtualMachineTest, BuiltinMathArrayAndStringModulesWorkTogether)
{
	Chunk chunk;
	chunk.WriteConstant(int64_t{ 3 });
	chunk.WriteConstant(int64_t{ 1 });
	chunk.Write(OP_BUILD_ARRAY);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "arr");

	WriteGetGlobal(chunk, "std.array");
	WriteGetModuleMember(chunk, "push");
	WriteGetGlobal(chunk, "arr");
	chunk.WriteConstant(int64_t{ 2 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.array");
	WriteGetModuleMember(chunk, "sort");
	WriteGetGlobal(chunk, "arr");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.math");
	WriteGetModuleMember(chunk, "clamp");
	chunk.WriteConstant(int64_t{ 20 });
	chunk.WriteConstant(int64_t{ 0 });
	chunk.WriteConstant(int64_t{ 10 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(3);
	WriteDefineGlobal(chunk, "clamped");

	WriteGetGlobal(chunk, "std.text");
	WriteGetModuleMember(chunk, "concat");
	chunk.WriteConstant(std::make_shared<const std::string>("au"));
	chunk.WriteConstant(std::make_shared<const std::string>("ra"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "text");

	WriteGetGlobal(chunk, "std.array");
	WriteGetModuleMember(chunk, "pop");
	WriteGetGlobal(chunk, "arr");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: 3"));

	Value clamped;
	ASSERT_TRUE(vm.GetContext().GetGlobal("clamped", clamped));
	EXPECT_EQ(ValueHelper::As<int64_t>(clamped), 10);

	Value text;
	ASSERT_TRUE(vm.GetContext().GetGlobal("text", text));
	EXPECT_EQ(ValueHelper::ToString(text), "aura");
}

TEST_F(VirtualMachineTest, BuiltinIoModuleSupportsVariadicPrintFunctions)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.io");
	WriteGetModuleMember(chunk, "print");
	chunk.WriteConstant(std::make_shared<const std::string>("a"));
	chunk.WriteConstant(int64_t{ 1 });
	chunk.WriteConstant(true);
	chunk.Write(OP_CALL);
	chunk.code.push_back(3);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.io");
	WriteGetModuleMember(chunk, "println");
	chunk.WriteConstant(std::make_shared<const std::string>("b"));
	chunk.WriteConstant(int64_t{ 2 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.io");
	WriteGetModuleMember(chunk, "printf");
	chunk.WriteConstant(std::make_shared<const std::string>("%s=%d"));
	chunk.WriteConstant(std::make_shared<const std::string>("x"));
	chunk.WriteConstant(int64_t{ 42 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(3);
	chunk.Write(OP_RETURN);

	const auto output = RunAndCapture(chunk);
	EXPECT_THAT(output, ::testing::HasSubstr("a 1 true"));
	EXPECT_THAT(output, ::testing::HasSubstr("b 2"));
	EXPECT_THAT(output, ::testing::HasSubstr("x=42"));
}

TEST_F(VirtualMachineTest, BuiltinLogModuleFatalRaisesRuntimeError)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.log");
	WriteGetModuleMember(chunk, "Fatal");
	chunk.WriteConstant(std::make_shared<const std::string>("boom"));
	chunk.WriteConstant(int64_t{ 7 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_THAT(std::string(vm.GetContext().GetError()), ::testing::HasSubstr("Fatal log invoked"));
}

TEST_F(VirtualMachineTest, BuiltinDiagnosticsModuleAllocAndFreeUpdateTrackedMemory)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.memory");
	WriteGetModuleMember(chunk, "alloc");
	chunk.WriteConstant(int64_t{ 64 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	WriteDefineGlobal(chunk, "ptr");

	WriteGetGlobal(chunk, "std.memory");
	WriteGetModuleMember(chunk, "active_bytes");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "bytes_after_alloc");

	WriteGetGlobal(chunk, "std.memory");
	WriteGetModuleMember(chunk, "free");
	WriteGetGlobal(chunk, "ptr");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.memory");
	WriteGetModuleMember(chunk, "active_bytes");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: 0"));

	Value bytesAfterAlloc;
	ASSERT_TRUE(vm.GetContext().GetGlobal("bytes_after_alloc", bytesAfterAlloc));
	EXPECT_EQ(ValueHelper::As<int64_t>(bytesAfterAlloc), 64);
}

TEST_F(VirtualMachineTest, BuiltinDiagnosticsModuleDetectsUseAfterFree)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.memory");
	WriteGetModuleMember(chunk, "alloc");
	chunk.WriteConstant(int64_t{ 8 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	WriteDefineGlobal(chunk, "ptr");

	WriteGetGlobal(chunk, "std.memory");
	WriteGetModuleMember(chunk, "free");
	WriteGetGlobal(chunk, "ptr");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "ptr");
	chunk.Write(OP_DEREF_GET);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_THAT(std::string(vm.GetContext().GetError()), ::testing::HasSubstr("Use after free"));
}

TEST_F(VirtualMachineTest, BuiltinDiagnosticsModuleClassifiesPointerAsNotSendSafe)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.memory");
	WriteGetModuleMember(chunk, "alloc");
	chunk.WriteConstant(int64_t{ 8 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	WriteDefineGlobal(chunk, "ptr");

	WriteGetGlobal(chunk, "std.memory");
	WriteGetModuleMember(chunk, "is_send");
	WriteGetGlobal(chunk, "ptr");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: false"));
}

TEST_F(VirtualMachineTest, BuiltinDiagnosticsModuleClassifiesPrimitiveArrayAsSendSafe)
{
	Chunk chunk;
	chunk.WriteConstant(int64_t{ 1 });
	chunk.WriteConstant(int64_t{ 2 });
	chunk.WriteConstant(int64_t{ 3 });
	chunk.Write(OP_BUILD_ARRAY);
	chunk.code.push_back(3);
	WriteDefineGlobal(chunk, "arr");

	WriteGetGlobal(chunk, "std.memory");
	WriteGetModuleMember(chunk, "is_send");
	WriteGetGlobal(chunk, "arr");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: true"));
}

TEST_F(VirtualMachineTest, BuiltinDiagnosticsModuleAssertNoLeaksFailsWhenAllocationSurvives)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.memory");
	WriteGetModuleMember(chunk, "alloc");
	chunk.WriteConstant(int64_t{ 32 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.memory");
	WriteGetModuleMember(chunk, "assert_no_leaks");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_THAT(std::string(vm.GetContext().GetError()), ::testing::HasSubstr("Memory leak detected"));
}

TEST_F(VirtualMachineTest, BuiltinSyncModuleDetectsLockGraphDeadlock)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "spawn");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "t1");

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "spawn");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "t2");

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "mutex");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "m1");

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "mutex");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "m2");

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "lock");
	WriteGetGlobal(chunk, "t1");
	WriteGetGlobal(chunk, "m1");
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "lock");
	WriteGetGlobal(chunk, "t2");
	WriteGetGlobal(chunk, "m2");
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "lock");
	WriteGetGlobal(chunk, "t1");
	WriteGetGlobal(chunk, "m2");
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "lock");
	WriteGetGlobal(chunk, "t2");
	WriteGetGlobal(chunk, "m1");
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_THAT(std::string(vm.GetContext().GetError()), ::testing::HasSubstr("Deadlock detected"));
}

TEST_F(VirtualMachineTest, BuiltinSyncModuleReportsWouldDeadlockBeforeLock)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "spawn");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "t1");

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "spawn");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "t2");

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "mutex");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "m1");

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "mutex");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "m2");

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "lock");
	WriteGetGlobal(chunk, "t1");
	WriteGetGlobal(chunk, "m1");
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "lock");
	WriteGetGlobal(chunk, "t2");
	WriteGetGlobal(chunk, "m2");
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "lock");
	WriteGetGlobal(chunk, "t1");
	WriteGetGlobal(chunk, "m2");
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "would_deadlock");
	WriteGetGlobal(chunk, "t2");
	WriteGetGlobal(chunk, "m1");
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: true"));
}

TEST_F(VirtualMachineTest, BuiltinSyncModuleRejectsSelfDeadlock)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "current_thread");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "t");

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "mutex");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "m");

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "lock");
	WriteGetGlobal(chunk, "t");
	WriteGetGlobal(chunk, "m");
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "lock");
	WriteGetGlobal(chunk, "t");
	WriteGetGlobal(chunk, "m");
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_THAT(std::string(vm.GetContext().GetError()), ::testing::HasSubstr("self-deadlock"));
}

TEST_F(VirtualMachineTest, BuiltinSyncModuleRejectsUnlockByNonOwner)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "spawn");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "t1");

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "spawn");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "t2");

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "mutex");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "m");

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "lock");
	WriteGetGlobal(chunk, "t1");
	WriteGetGlobal(chunk, "m");
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.sync");
	WriteGetModuleMember(chunk, "unlock");
	WriteGetGlobal(chunk, "t2");
	WriteGetGlobal(chunk, "m");
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_THAT(std::string(vm.GetContext().GetError()), ::testing::HasSubstr("non-owner"));
}
