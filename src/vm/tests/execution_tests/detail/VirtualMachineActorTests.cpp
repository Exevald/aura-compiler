#include "../VirtualMachineTestSupport.h"

TEST_F(VirtualMachineTest, ActorSendThenQueryUsesMailboxOrdering)
{
	Chunk chunk;
	WriteCounterMethodTable(chunk);

	chunk.WriteConstant(0.0);
	chunk.Write(OP_BUILD_ACTOR);
	WriteUint8Operand(chunk, AddStringConstant(chunk, "Counter"));
	WriteUint8Operand(chunk, 1);
	WriteDefineGlobal(chunk, "counter");

	WriteGetGlobal(chunk, "counter");
	chunk.WriteConstant(5.0);
	WriteActorMessage(chunk, OP_ACTOR_SEND, "inc", 1);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "counter");
	WriteActorMessage(chunk, OP_ACTOR_QUERY, "get", 0);
	chunk.Write(OP_RETURN);

	const std::string output = RunAndCapture(chunk);
	EXPECT_THAT(output, ::testing::HasSubstr("Result: 5"));
}

TEST_F(VirtualMachineTest, ActorMessageFailureDoesNotKillMailbox)
{
	Chunk chunk;
	WriteCounterMethodTable(chunk, true);

	chunk.WriteConstant(0.0);
	chunk.Write(OP_BUILD_ACTOR);
	WriteUint8Operand(chunk, AddStringConstant(chunk, "Counter"));
	WriteUint8Operand(chunk, 1);
	WriteDefineGlobal(chunk, "counter");

	WriteGetGlobal(chunk, "counter");
	WriteActorMessage(chunk, OP_ACTOR_SEND, "fail", 0);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "counter");
	chunk.WriteConstant(7.0);
	WriteActorMessage(chunk, OP_ACTOR_SEND, "inc", 1);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "counter");
	WriteActorMessage(chunk, OP_ACTOR_QUERY, "get", 0);
	chunk.Write(OP_RETURN);

	const std::string output = RunAndCapture(chunk);
	EXPECT_THAT(output, ::testing::HasSubstr("Result: 7"));

	Value actorValue;
	ASSERT_TRUE(vm.GetContext().GetGlobal("counter", actorValue));
	ASSERT_TRUE(std::holds_alternative<ActorPtr>(actorValue));
	const auto actor = std::get<ActorPtr>(actorValue);
	ASSERT_NE(actor, nullptr);
	EXPECT_EQ(actor->failures.size(), 1);
	EXPECT_THAT(actor->failures.front(), ::testing::HasSubstr("Division by zero"));
}
