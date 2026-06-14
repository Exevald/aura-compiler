#include "CompilerTest.h"

TEST_F(CompilerTest, SemanticAddressOfConstRejected)
{
	const auto root = ParseCode("const x: int = 1; unsafe { var p: ptr<int> = &x; }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticPointerOpsRequireUnsafeBlock)
{
	const auto root = ParseCode(
		"var ptr_val: ptr<int>;"
		"print *ptr_val;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticResumeOnlyAllowedInEffectHandler)
{
	const auto root = ParseCode("fn bad() : int { return resume(1); }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticEffectHandlerRequiresExactlyOneResume)
{
	const auto missingResume = ParseCode(
		"effect IO { fn read() : int; }"
		"fn run() : int raises { IO } { return read(); }"
		"fn main() { handle run() with { effect read() -> { return 7; } } }");
	ASSERT_NE(missingResume, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(missingResume.get()), std::runtime_error);

	const auto doubleResume = ParseCode(
		"effect IO { fn read() : int; }"
		"fn run() : int raises { IO } { return read(); }"
		"fn main() { handle run() with { effect read() -> { resume(7); resume(8); } } }");
	ASSERT_NE(doubleResume, nullptr);
	EXPECT_THROW(compiler.Compile(doubleResume.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticEffectOperationResolvesThroughSingleAccessibleEffect)
{
	const auto root = ParseCode(
		"effect Input { fn read() : int; }"
		"effect Backup { fn read() : int; }"
		"fn run() : int raises { Input } { return read(); }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticEffectOperationRejectsAmbiguousAccessibleEffects)
{
	const auto root = ParseCode(
		"effect Input { fn read() : int; }"
		"effect Backup { fn read() : int; }"
		"fn run() : int raises { Input | Backup } { return read(); }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticMemoryAllocFreeRequireUnsafe)
{
	const auto root = ParseCode(
		"import std.memory as mem;"
		"var block = mem.alloc(8);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticUserVariadicFunctionsCompile)
{
	const auto root = ParseCode(
		"fn gather(prefix: string, ...values: any) : string {"
		"  return prefix;"
		"}"
		"fn main() : string {"
		"  return gather(\"a\", 1, true);"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticVariadicParametersMustBeTrailing)
{
	const auto root = ParseCode("fn bad(...values: any, tail: int) : void { return; }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticGoAwaitCompilesForSendableArguments)
{
	const auto root = ParseCode(
		"fn add(a: int, b: int) : int {"
		"  return a + b;"
		"}"
		"fn main() : int {"
		"  var task = go add(19, 23);"
		"  return await task;"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticAwaitRejectsNonTaskOperand)
{
	const auto root = ParseCode("fn main() : int { return await 42; }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticGoRejectsNonSendableArgument)
{
	const auto root = ParseCode(
		"fn consume(p: ptr<int>) : void { return; }"
		"fn main() : void {"
		"  unsafe {"
		"    var local: int = 7;"
		"    var p: ptr<int> = &local;"
		"    var task = go consume(p);"
		"    await task;"
		"  }"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}
