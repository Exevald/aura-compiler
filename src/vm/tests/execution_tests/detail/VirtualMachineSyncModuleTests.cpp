#include "../VirtualMachineTestSupport.h"

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
