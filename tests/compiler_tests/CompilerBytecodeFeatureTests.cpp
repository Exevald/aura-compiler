#include "CompilerTest.h"

TEST_F(CompilerTest, BytecodeClosureCapturesOuterVariable)
{
	const auto root = ParseCode(
		"fn make_adder(n : int) : int {"
		"  var add = fn(x : int) -> x + n;"
		"  return add(2);"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_CLOSURE));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_UPVALUE));
}

TEST_F(CompilerTest, BytecodeIterAdapterChain)
{
	const auto root = ParseCode(
		"fn walk(list: [int]) {"
		"  iter (item of list with [drop(1), take(2), reverse, filter(fn(v: int) -> v > 0), transform(fn(v: int) -> v + 10)]) {"
		"    print item;"
		"  }"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ITER_DROP));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ITER_TAKE));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ITER_REVERSE));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ITER_FILTER));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ITER_TRANSFORM));
}

TEST_F(CompilerTest, BytecodeUnaryAndBooleanLiterals)
{
	const auto root = ParseCode("var x = -5; var flag = not false; var empty = null;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_NEGATE));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_NOT));
}

TEST_F(CompilerTest, BytecodePointerDereferenceGetAndSet)
{
	const auto root = ParseCode(
		"type Score = int;"
		"var ptr_val: ptr<int>;"
		"unsafe { *(ptr_val) = 5; print *(ptr_val); }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DEREF_SET));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DEREF_GET));
}

TEST_F(CompilerTest, BytecodeAddressOfLocalGlobalAndMember)
{
	const auto root = ParseCode(
		"struct Point { x: int; }"
		"fn id(v: int) : int { return v; }"
		"fn demo() {"
		"  unsafe {"
		"    var x: int = 1;"
		"    var p: ptr<int> = &x;"
		"    var fp = &id;"
		"    var point = Point(2);"
		"    var mp: ptr<int> = &point.x;"
		"    var arr = [10, 20];"
		"    var ap: ptr<int> = &arr[1];"
		"    print *p;"
		"  }"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ADDR_LOCAL));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ADDR_GLOBAL));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ADDR_MEMBER));
}

TEST_F(CompilerTest, BytecodeAddressOfCapturedVariable)
{
	const auto root = ParseCode(
		"fn makeCounter() {"
		"  var x: int = 41;"
		"  var next = fn() -> {"
		"    unsafe {"
		"      var px: ptr<int> = &x;"
		"      *px = *px + 1;"
		"      return x;"
		"    }"
		"  };"
		"  return next;"
		"}"
		"var counter = makeCounter();"
		"print counter();");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ADDR_UPVALUE));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DEREF_GET));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DEREF_SET));
}

TEST_F(CompilerTest, BytecodeRefParameterPassesAddress)
{
	const auto root = ParseCode(
		"fn inc(n: ref<int>) : void {"
		"  *n = *n + 1;"
		"}"
		"var value: int = 1;"
		"inc(value);"
		"print value;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ADDR_LOCAL));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DEREF_GET));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DEREF_SET));
}

TEST_F(CompilerTest, BytecodeRefParameterPassesArrayElementAddress)
{
	const auto root = ParseCode(
		"fn setValue(n: ref<int>) : void {"
		"  *n = 9;"
		"}"
		"var values = [1, 2];"
		"setValue(values[0]);"
		"print values[0];");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ADDR_MEMBER));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DEREF_SET));
}

TEST_F(CompilerTest, BytecodeStructConstructorAndFieldAccess)
{
	const auto root = ParseCode(
		"struct Point { x: int; y: int; }"
		"var p: Point = Point(10, 20);"
		"print p.x;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_STRUCT));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_GET));
}

TEST_F(CompilerTest, BytecodeStructFieldAssignment)
{
	const auto root = ParseCode(
		"struct Point { x: int; y: int; }"
		"var p = Point(10, 20);"
		"p.x = 42;"
		"print p.x;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_SET));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_GET));
}

TEST_F(CompilerTest, BytecodeNestedStructFieldAccessAndAssignment)
{
	const auto root = ParseCode(
		"struct Inner { x: int; }"
		"struct Outer { inner: Inner; }"
		"var outer = Outer(Inner(10));"
		"outer.inner.x = 42;"
		"print outer.inner.x;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_STRUCT));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_SET));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_GET));
}

TEST_F(CompilerTest, BytecodeStructWholeFieldAssignment)
{
	const auto root = ParseCode(
		"struct Inner { x: int; }"
		"struct Outer { inner: Inner; }"
		"var outer = Outer(Inner(10));"
		"outer.inner = Inner(42);"
		"print outer.inner.x;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_STRUCT));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_SET));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_GET));
}

TEST_F(CompilerTest, BytecodeEnumConstructorAndTagAccess)
{
	const auto root = ParseCode(
		"enum Option { None | Some(int) }"
		"var value = Some(42);"
		"print value.tag;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_ENUM));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_ENUM_TAG));
}

TEST_F(CompilerTest, BytecodeEnumArgumentAccess)
{
	const auto root = ParseCode(
		"enum Option { None | Some(int) }"
		"var value = Some(42);"
		"print value[0];");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_ENUM));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_ENUM_ARG));
}
