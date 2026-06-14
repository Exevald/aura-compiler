#include "values/ValueHelper.h"
#include "ExecutionContext.h"
#include "SharedRuntime.h"

#include <gtest/gtest.h>
#include <stdexcept>

using VM::Execution::ExecutionContext;
using namespace VM::Core;

static FunctionPtr CreateDummyFunction(const std::string& name = "test")
{
	auto func = std::make_shared<Function>();
	func->name = name;
	return func;
}

static ClosurePtr CreateDummyClosure(const std::string& name = "test")
{
	auto closure = std::make_shared<Closure>();
	closure->function = CreateDummyFunction(name);
	return closure;
}

TEST(ExecutionContextTest, TransactionRollbackRestoresGlobalsAndHeapState)
{
	ExecutionContext ctx;
	ctx.DefineGlobal("counter", 1.0);
	ctx.DefineThreadLocalGlobal("__thread_local.counter", 2.0);

	auto mutex = ctx.CreateMutex();
	ASSERT_NE(mutex, nullptr);
	ASSERT_TRUE(ctx.BeginTransaction(mutex));

	auto array = std::make_shared<Array>();
	array->elements = { 10.0, 20.0 };
	ctx.RecordArrayWrite(array, 1, array->elements[1]);
	array->elements[1] = 99.0;

	auto instance = std::make_shared<Instance>();
	instance->fields = { 3.0 };
	ctx.RecordInstanceFieldWrite(instance, 0, instance->fields[0]);
	instance->fields[0] = 42.0;

	ASSERT_TRUE(ctx.SetGlobal("counter", 7.0));
	ASSERT_TRUE(ctx.SetThreadLocalGlobal("__thread_local.counter", 8.0));

	ctx.UnwindTransactions(0);

	Value globalValue;
	ASSERT_TRUE(ctx.GetGlobal("counter", globalValue));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(globalValue), 1.0);

	Value threadLocalValue;
	ASSERT_TRUE(ctx.GetThreadLocalGlobal("__thread_local.counter", threadLocalValue));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(threadLocalValue), 2.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(array->elements[1]), 20.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(instance->fields[0]), 3.0);
}

TEST(ExecutionContextTest, NestedTransactionRollbackRestoresInnerSavepointOnly)
{
	ExecutionContext ctx;
	ctx.DefineGlobal("counter", 1.0);

	auto mutex = ctx.CreateMutex();
	ASSERT_NE(mutex, nullptr);
	ASSERT_TRUE(ctx.BeginTransaction(mutex));
	ASSERT_TRUE(ctx.SetGlobal("counter", 2.0));

	ASSERT_TRUE(ctx.BeginTransaction(mutex));
	ASSERT_TRUE(ctx.SetGlobal("counter", 3.0));
	ctx.UnwindTransactions(1);

	Value globalValue;
	ASSERT_TRUE(ctx.GetGlobal("counter", globalValue));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(globalValue), 2.0);

	ASSERT_TRUE(ctx.EndTransaction());
	ASSERT_TRUE(ctx.GetGlobal("counter", globalValue));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(globalValue), 2.0);
}

TEST(ExecutionContextTest, TransactionDefersGlobalAndThreadLocalCommitUntilEndTransaction)
{
	ExecutionContext ctx;
	ctx.DefineGlobal("counter", 1.0);
	ctx.DefineThreadLocalGlobal("__thread_local.counter", 2.0);

	auto mutex = ctx.CreateMutex();
	ASSERT_NE(mutex, nullptr);
	ASSERT_TRUE(ctx.BeginTransaction(mutex));
	ASSERT_TRUE(ctx.SetGlobal("counter", 7.0));
	ASSERT_TRUE(ctx.SetThreadLocalGlobal("__thread_local.counter", 8.0));

	Value visibleInTxn;
	ASSERT_TRUE(ctx.GetGlobal("counter", visibleInTxn));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visibleInTxn), 7.0);
	ASSERT_TRUE(ctx.GetThreadLocalGlobal("__thread_local.counter", visibleInTxn));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visibleInTxn), 8.0);

	ASSERT_TRUE(ctx.EndTransaction());

	Value committed;
	ASSERT_TRUE(ctx.GetGlobal("counter", committed));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(committed), 7.0);
	ASSERT_TRUE(ctx.GetThreadLocalGlobal("__thread_local.counter", committed));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(committed), 8.0);
}

TEST(ExecutionContextTest, NestedTransactionCommitMergesDeferredWritesIntoParent)
{
	ExecutionContext ctx;
	ctx.DefineGlobal("counter", 1.0);

	auto mutex = ctx.CreateMutex();
	ASSERT_NE(mutex, nullptr);
	ASSERT_TRUE(ctx.BeginTransaction(mutex));
	ASSERT_TRUE(ctx.SetGlobal("counter", 2.0));

	ASSERT_TRUE(ctx.BeginTransaction(mutex));
	ASSERT_TRUE(ctx.SetGlobal("counter", 3.0));
	ASSERT_TRUE(ctx.EndTransaction());

	Value duringOuter;
	ASSERT_TRUE(ctx.GetGlobal("counter", duringOuter));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(duringOuter), 3.0);

	ASSERT_TRUE(ctx.EndTransaction());

	Value committed;
	ASSERT_TRUE(ctx.GetGlobal("counter", committed));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(committed), 3.0);
}

TEST(ExecutionContextTest, TransactionDefersArrayAndInstanceCommitUntilEndTransaction)
{
	ExecutionContext ctx;
	auto array = std::make_shared<Array>();
	array->elements = { 10.0, 20.0 };
	auto instance = std::make_shared<Instance>();
	instance->fields = { 3.0, 4.0 };

	auto mutex = ctx.CreateMutex();
	ASSERT_NE(mutex, nullptr);
	ASSERT_TRUE(ctx.BeginTransaction(mutex));
	ASSERT_TRUE(ctx.SetArrayElement(array, 1, 99.0));
	ASSERT_TRUE(ctx.SetInstanceField(instance, 0, 42.0));

	Value visibleInTxn;
	ASSERT_TRUE(ctx.GetArrayElement(array, 1, visibleInTxn));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visibleInTxn), 99.0);
	ASSERT_TRUE(ctx.GetInstanceField(instance, 0, visibleInTxn));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visibleInTxn), 42.0);

	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(array->elements[1]), 20.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(instance->fields[0]), 3.0);

	ASSERT_TRUE(ctx.EndTransaction());

	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(array->elements[1]), 99.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(instance->fields[0]), 42.0);
}

TEST(ExecutionContextTest, NestedTransactionCommitMergesDeferredHeapWritesIntoParent)
{
	ExecutionContext ctx;
	auto array = std::make_shared<Array>();
	array->elements = { 10.0, 20.0 };
	auto instance = std::make_shared<Instance>();
	instance->fields = { 3.0 };

	auto mutex = ctx.CreateMutex();
	ASSERT_NE(mutex, nullptr);
	ASSERT_TRUE(ctx.BeginTransaction(mutex));
	ASSERT_TRUE(ctx.SetArrayElement(array, 0, 11.0));
	ASSERT_TRUE(ctx.SetInstanceField(instance, 0, 4.0));

	ASSERT_TRUE(ctx.BeginTransaction(mutex));
	ASSERT_TRUE(ctx.SetArrayElement(array, 0, 12.0));
	ASSERT_TRUE(ctx.SetInstanceField(instance, 0, 5.0));
	ASSERT_TRUE(ctx.EndTransaction());

	Value visibleInOuter;
	ASSERT_TRUE(ctx.GetArrayElement(array, 0, visibleInOuter));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visibleInOuter), 12.0);
	ASSERT_TRUE(ctx.GetInstanceField(instance, 0, visibleInOuter));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visibleInOuter), 5.0);

	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(array->elements[0]), 10.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(instance->fields[0]), 3.0);

	ASSERT_TRUE(ctx.EndTransaction());

	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(array->elements[0]), 12.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(instance->fields[0]), 5.0);
}

TEST(ExecutionContextTest, TransactionDefersArrayStructuralMutationsUntilEndTransaction)
{
	ExecutionContext ctx;
	auto array = std::make_shared<Array>();
	array->elements = { 3.0, 1.0 };

	auto mutex = ctx.CreateMutex();
	ASSERT_NE(mutex, nullptr);
	ASSERT_TRUE(ctx.BeginTransaction(mutex));
	ASSERT_TRUE(ctx.PushArrayElement(array, 2.0));

	Value popped;
	ASSERT_TRUE(ctx.PopArrayElement(array, popped));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(popped), 2.0);
	ASSERT_TRUE(ctx.PushArrayElement(array, 4.0));

	size_t visibleSize = 0;
	ASSERT_TRUE(ctx.GetArraySize(array, visibleSize));
	EXPECT_EQ(visibleSize, 3);
	EXPECT_EQ(array->elements.size(), 2);

	ASSERT_TRUE(ctx.EndTransaction());

	EXPECT_EQ(array->elements.size(), 3);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(array->elements[0]), 3.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(array->elements[1]), 1.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(array->elements[2]), 4.0);
}

TEST(ExecutionContextTest, MultiRegionTransactionAcquiresAndCommitsAcrossSeveralMutexes)
{
	ExecutionContext ctx;
	ctx.DefineGlobal("left", 1.0);
	ctx.DefineGlobal("right", 2.0);

	auto leftMutex = ctx.CreateMutex();
	auto rightMutex = ctx.CreateMutex();
	ASSERT_NE(leftMutex, nullptr);
	ASSERT_NE(rightMutex, nullptr);

	ASSERT_TRUE(ctx.BeginTransaction(std::vector<MutexPtr>{ rightMutex, leftMutex }));
	ASSERT_TRUE(ctx.SetGlobal("left", 10.0));
	ASSERT_TRUE(ctx.SetGlobal("right", 20.0));

	Value visible;
	ASSERT_TRUE(ctx.GetGlobal("left", visible));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visible), 10.0);
	ASSERT_TRUE(ctx.GetGlobal("right", visible));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visible), 20.0);

	ASSERT_TRUE(ctx.EndTransaction());

	ASSERT_TRUE(ctx.GetGlobal("left", visible));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visible), 10.0);
	ASSERT_TRUE(ctx.GetGlobal("right", visible));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visible), 20.0);
	EXPECT_FALSE(ctx.IsMutexLocked(leftMutex->id));
	EXPECT_FALSE(ctx.IsMutexLocked(rightMutex->id));
}

TEST(ExecutionContextTest, NestedMultiRegionTransactionMergesIntoOuterTransaction)
{
	ExecutionContext ctx;
	ctx.DefineGlobal("left", 1.0);
	ctx.DefineGlobal("right", 2.0);
	ctx.DefineGlobal("third", 3.0);

	auto leftMutex = ctx.CreateMutex();
	auto rightMutex = ctx.CreateMutex();
	auto thirdMutex = ctx.CreateMutex();
	ASSERT_TRUE(ctx.BeginTransaction(std::vector<MutexPtr>{ leftMutex, rightMutex }));
	ASSERT_TRUE(ctx.SetGlobal("left", 10.0));

	ASSERT_TRUE(ctx.BeginTransaction(std::vector<MutexPtr>{ rightMutex, thirdMutex }));
	ASSERT_TRUE(ctx.SetGlobal("right", 20.0));
	ASSERT_TRUE(ctx.SetGlobal("third", 30.0));
	ASSERT_TRUE(ctx.EndTransaction());

	Value visible;
	ASSERT_TRUE(ctx.GetGlobal("third", visible));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visible), 30.0);
	EXPECT_TRUE(ctx.IsMutexLocked(leftMutex->id));
	EXPECT_TRUE(ctx.IsMutexLocked(rightMutex->id));
	EXPECT_TRUE(ctx.IsMutexLocked(thirdMutex->id));

	ASSERT_TRUE(ctx.EndTransaction());

	ASSERT_TRUE(ctx.GetGlobal("left", visible));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visible), 10.0);
	ASSERT_TRUE(ctx.GetGlobal("right", visible));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visible), 20.0);
	ASSERT_TRUE(ctx.GetGlobal("third", visible));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(visible), 30.0);
	EXPECT_FALSE(ctx.IsMutexLocked(leftMutex->id));
	EXPECT_FALSE(ctx.IsMutexLocked(rightMutex->id));
	EXPECT_FALSE(ctx.IsMutexLocked(thirdMutex->id));
}