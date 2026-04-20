#include "../VirtualMachineTestSupport.h"

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
