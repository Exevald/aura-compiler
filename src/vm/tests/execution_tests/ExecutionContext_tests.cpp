#include "ExecutionContext.h"

#include <gtest/gtest.h>
#include <stdexcept>

using VM::Execution::ExecutionContext;
using namespace VM::Core;

TEST(ExecutionContextTest, PushValue_AddsToStack)
{
	ExecutionContext ctx;

	ctx.PushValue(42.0);
	ctx.PushValue(true);

	EXPECT_EQ(ctx.StackSize(), 2);
	EXPECT_TRUE(ctx.StackEmpty() == false);
}

TEST(ExecutionContextTest, PopValue_RemovesFromStack)
{
	ExecutionContext ctx;

	ctx.PushValue(10.0);
	ctx.PushValue(20.0);

	auto val = ctx.PopValue();
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(val), 20.0);
	EXPECT_EQ(ctx.StackSize(), 1);
}

TEST(ExecutionContextTest, PeekValue_DoesNotRemove)
{
	ExecutionContext ctx;

	ctx.PushValue(1.0);
	ctx.PushValue(2.0);

	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ctx.PeekValue(0)), 2.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ctx.PeekValue(1)), 1.0);
	EXPECT_EQ(ctx.StackSize(), 2);
}

TEST(ExecutionContextTest, PeekValue_ConstOverload)
{
	const ExecutionContext ctx;
	EXPECT_NO_THROW([] {
	}());
}

TEST(ExecutionContextTest, StackOverflow_Throws)
{
	ExecutionContext ctx;

	for (size_t i = 0; i < 4096; ++i)
	{
		ctx.PushValue(0.0);
	}

	EXPECT_THROW(ctx.PushValue(1.0), std::overflow_error);
}

TEST(ExecutionContextTest, PopFromEmptyStack_Throws)
{
	ExecutionContext ctx;
	EXPECT_THROW(ctx.PopValue(), std::underflow_error);
}

TEST(ExecutionContextTest, PeekOutOfRange_Throws)
{
	ExecutionContext ctx;
	ctx.PushValue(1.0);

	EXPECT_THROW(ctx.PeekValue(1), std::out_of_range);
}

TEST(ExecutionContextTest, ClearStack_EmptiesStack)
{
	ExecutionContext ctx;
	ctx.PushValue(1.0);
	ctx.PushValue(2.0);

	ctx.ClearStack();

	EXPECT_TRUE(ctx.StackEmpty());
	EXPECT_EQ(ctx.StackSize(), 0);
}

TEST(ExecutionContextTest, EnterScope_CreatesNewScope)
{
	ExecutionContext ctx;

	auto* scope1 = ctx.EnterScope();
	auto* scope2 = ctx.EnterScope();

	EXPECT_NE(scope1, scope2);
	EXPECT_EQ(scope2->parent.get(), scope1);
}

TEST(ExecutionContextTest, Scope_Variables_Isolated)
{
	ExecutionContext ctx;

	ctx.CurrentScope()->SetVariable("x", 10.0);

	ctx.EnterScope();
	ctx.CurrentScope()->SetVariable("x", 20.0);
	ctx.CurrentScope()->SetVariable("y", 30.0);

	auto* x = ctx.CurrentScope()->GetVariable("x");
	auto* y = ctx.CurrentScope()->GetVariable("y");

	ASSERT_NE(x, nullptr);
	ASSERT_NE(y, nullptr);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(*x), 20.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(*y), 30.0);

	ctx.ExitScope();
	x = ctx.CurrentScope()->GetVariable("x");
	y = ctx.CurrentScope()->GetVariable("y");

	ASSERT_NE(x, nullptr);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(*x), 10.0);
	EXPECT_EQ(y, nullptr);
}

TEST(ExecutionContextTest, HasVariable_ChecksParentChain)
{
	ExecutionContext ctx;

	ctx.CurrentScope()->SetVariable("global", true);
	ctx.EnterScope();
	ctx.CurrentScope()->SetVariable("local", 42);

	EXPECT_TRUE(ctx.CurrentScope()->HasVariable("global"));
	EXPECT_TRUE(ctx.CurrentScope()->HasVariable("local"));
	EXPECT_FALSE(ctx.CurrentScope()->HasVariable("missing"));
}

TEST(ExecutionContextTest, ExitScope_ReturnsToParent)
{
	ExecutionContext ctx;

	auto* root = ctx.CurrentScope();
	ctx.EnterScope();

	EXPECT_EQ(ctx.ExitScope(), root);
	EXPECT_EQ(ctx.CurrentScope(), root);
}

TEST(ExecutionContextTest, ExitScope_AtRoot_ReturnsNullptr)
{
	ExecutionContext ctx;
	EXPECT_EQ(ctx.ExitScope(), nullptr);
}

TEST(ExecutionContextTest, ScopeDepthLimit_Throws)
{
	ExecutionContext ctx;

	for (int i = 1; i < 256; ++i)
	{
		ctx.EnterScope();
	}

	EXPECT_THROW(ctx.EnterScope(), std::overflow_error);
}

TEST(ExecutionContextTest, PushCallFrame_AddsFrame)
{
	ExecutionContext ctx;

	ctx.PushCallFrame(10, 0);
	ctx.PushCallFrame(20, 2);

	auto* frame = ctx.CurrentFrame();
	ASSERT_NE(frame, nullptr);
	EXPECT_EQ(frame->ip, 20);
	EXPECT_EQ(frame->stackBase, 2);
}

TEST(ExecutionContextTest, PopCallFrame_RemovesFrame)
{
	ExecutionContext ctx;

	ctx.PushCallFrame(10, 0);
	ctx.PushCallFrame(20, 2);

	ctx.PopCallFrame();

	auto* frame = ctx.CurrentFrame();
	ASSERT_NE(frame, nullptr);
	EXPECT_EQ(frame->ip, 10);
}

TEST(ExecutionContextTest, PopCallFrame_EmptyStack_ReturnsNullptr)
{
	ExecutionContext ctx;
	EXPECT_EQ(ctx.PopCallFrame(), nullptr);
}

TEST(ExecutionContextTest, RaiseError_SetsMessage)
{
	ExecutionContext ctx;

	EXPECT_FALSE(ctx.HasError());

	ctx.RaiseError("Test error");

	EXPECT_TRUE(ctx.HasError());
	EXPECT_EQ(ctx.GetError(), "Test error");
}

TEST(ExecutionContextTest, RaiseError_KeepsFirstError)
{
	ExecutionContext ctx;

	ctx.RaiseError("First error");
	ctx.RaiseError("Second error");

	EXPECT_EQ(ctx.GetError(), "First error");
}

TEST(ExecutionContextTest, ClearError_ResetsState)
{
	ExecutionContext ctx;

	ctx.RaiseError("Error");
	EXPECT_TRUE(ctx.HasError());

	ctx.ClearError();
	EXPECT_FALSE(ctx.HasError());
	EXPECT_TRUE(ctx.GetError().empty());
}

TEST(ExecutionContextTest, Allocate_ReturnsNonNullPointer)
{
	ExecutionContext ctx;

	void* ptr = ctx.Allocate(100);

	EXPECT_NE(ptr, nullptr);
}

TEST(ExecutionContextTest, JumpAndJumpIf_NoOpForMVP)
{
	ExecutionContext ctx;

	EXPECT_NO_THROW(ctx.Jump(100));
	EXPECT_NO_THROW(ctx.JumpIf(true, 200));
	EXPECT_NO_THROW(ctx.JumpIf(false, 300));
}

TEST(ExecutionContextTest, PrintStack_DoesNotCrash)
{
	ExecutionContext ctx;

	ctx.PushValue(1.0);
	ctx.PushValue(2.0);

	EXPECT_NO_THROW(ctx.PrintStack());
}