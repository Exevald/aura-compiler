#include "ExecutionContext.h"

#include <gtest/gtest.h>
#include <stdexcept>

using VM::Execution::ExecutionContext;
using namespace VM::Core;

FunctionPtr CreateDummyFunction(const std::string& name = "test")
{
	auto func = std::make_shared<Function>();
	func->name = name;
	return func;
}

TEST(ExecutionContextTest, PushValue_AddsToStack)
{
	ExecutionContext ctx;
	ctx.PushValue(42.0);
	ctx.PushValue(true);

	EXPECT_EQ(ctx.StackSize(), 2);
	EXPECT_FALSE(ctx.StackEmpty());
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

TEST(ExecutionContextTest, StackOverflow_Throws)
{
	ExecutionContext ctx;
	for (size_t i = 0; i < 4096; ++i)
	{
		ctx.PushValue(0.0);
	}
	EXPECT_THROW(ctx.PushValue(1.0), std::overflow_error);
}

TEST(ExecutionContextTest, ClearStack_EmptiesStack)
{
	ExecutionContext ctx;
	ctx.PushValue(1.0);
	ctx.PushFrame(CreateDummyFunction(), 0);

	ctx.ClearStack();

	EXPECT_TRUE(ctx.StackEmpty());
	EXPECT_FALSE(ctx.HasFrames());
}

TEST(ExecutionContextTest, Scope_Variables_Isolated)
{
	ExecutionContext ctx;
	ctx.CurrentScope()->SetVariable("x", 10.0);

	ctx.EnterScope();
	ctx.CurrentScope()->SetVariable("x", 20.0);
	ctx.CurrentScope()->SetVariable("y", 30.0);

	auto* x = ctx.CurrentScope()->GetVariable("x");
	ASSERT_NE(x, nullptr);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(*x), 20.0);

	ctx.ExitScope();
	x = ctx.CurrentScope()->GetVariable("x");
	ASSERT_NE(x, nullptr);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(*x), 10.0);
}

TEST(ExecutionContextTest, PushFrame_AddsFrame)
{
	ExecutionContext ctx;
	auto func1 = CreateDummyFunction("f1");
	auto func2 = CreateDummyFunction("f2");

	ctx.PushFrame(func1, 0);
	ctx.PushFrame(func2, 2);

	auto& frame = ctx.CurrentFrame();
	EXPECT_EQ(frame.function->name, "f2");
	EXPECT_EQ(frame.stackBase, 2);
}

TEST(ExecutionContextTest, PopFrame_RemovesFrame)
{
	ExecutionContext ctx;
	ctx.PushFrame(CreateDummyFunction("f1"), 0);
	ctx.PushFrame(CreateDummyFunction("f2"), 2);

	ctx.PopFrame();

	EXPECT_TRUE(ctx.HasFrames());
	EXPECT_EQ(ctx.CurrentFrame().function->name, "f1");
}

TEST(ExecutionContextTest, PopFrame_ThrowsIfEmpty)
{
	ExecutionContext ctx;
	EXPECT_THROW(ctx.PopFrame(), std::runtime_error);
}

TEST(ExecutionContextTest, RaiseError_SetsMessage)
{
	ExecutionContext ctx;
	ctx.RaiseError("Test error");

	EXPECT_TRUE(ctx.HasError());
	EXPECT_EQ(ctx.GetError(), "Test error");
}

TEST(ExecutionContextTest, GlobalStorage)
{
	ExecutionContext ctx;
	ctx.DefineGlobal("test", 123.0);

	Value retrieved;
	EXPECT_TRUE(ctx.GetGlobal("test", retrieved));
	EXPECT_EQ(ValueHelper::As<double>(retrieved), 123.0);
}

TEST(ExecutionContextTest, LocalStackAccess_Absolute)
{
	ExecutionContext ctx;
	ctx.PushValue(10.0);
	ctx.PushValue(20.0);

	EXPECT_EQ(ValueHelper::As<double>(ctx.GetAt(0)), 10.0);
	ctx.SetAt(1, 99.0);
	EXPECT_EQ(ValueHelper::As<double>(ctx.PeekValue(0)), 99.0);
}

TEST(ExecutionContextTest, LocalStackAccess_Relative)
{
	ExecutionContext ctx;
	ctx.PushValue(10.0);
	ctx.PushValue(20.0);
	ctx.PushValue(30.0);

	ctx.PushFrame(CreateDummyFunction(), 1);

	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ctx.GetLocal(0)), 20.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ctx.GetLocal(1)), 30.0);

	ctx.SetLocal(0, 55.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ctx.PeekValue(1)), 55.0);
}

TEST(ExecutionContextTest, Allocate_ReturnsNonNull)
{
	ExecutionContext ctx;
	void* ptr = ctx.Allocate(100);
	EXPECT_NE(ptr, nullptr);
}

TEST(ExecutionContextTest, CallFrameStackIsolation)
{
	ExecutionContext ctx;

	ctx.PushValue(10.0);
	ctx.PushValue(20.0);
	ctx.PushValue(30.0);

	const auto func = CreateDummyFunction("my_func");
	ctx.PushFrame(func, 1);

	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ctx.GetLocal(0)), 20.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ctx.GetLocal(1)), 30.0);

	ctx.SetLocal(0, 99.0);

	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(ctx.PeekValue(1)), 99.0);
}

TEST(ExecutionContextTest, FrameUnderflowProtection)
{
	ExecutionContext ctx;
	ctx.PushFrame(CreateDummyFunction("test"), 10);

	EXPECT_THROW(ctx.GetLocal(0), std::out_of_range);
}

TEST(ExecutionContextTest, MultipleFramesManagement)
{
	ExecutionContext ctx;

	ctx.PushFrame(CreateDummyFunction("main"), 0);
	ctx.PushFrame(CreateDummyFunction("sub"), 5);

	EXPECT_EQ(ctx.CurrentFrame().function->name, "sub");
	EXPECT_TRUE(ctx.HasFrames());

	ctx.PopFrame();
	EXPECT_EQ(ctx.CurrentFrame().function->name, "main");

	ctx.PopFrame();
	EXPECT_FALSE(ctx.HasFrames());
}