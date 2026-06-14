#include "CompilerTest.h"

TEST_F(CompilerTest, SemanticUndefinedVariable)
{
	const auto root = ParseCode("var x = y + 1;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticArithmeticTypeMismatch)
{
	const auto root = ParseCode("var x = \"hello\" - 1;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticIndexingNonArray)
{
	const auto root = ParseCode("var x = 1; print x[0];");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticIndexingStringType)
{
	const auto root = ParseCode("var list = [1, 2]; var y = list[\"a\"];");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticExplicitTypeMismatch)
{
	const auto root = ParseCode("var x : int = \"hello\";");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticConstAssignmentRejected)
{
	const auto root = ParseCode("const x: int = 1; x = 2;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticTypeAliasResolvesExplicitType)
{
	const auto root = ParseCode(
		"type Score = int;"
		"const baseline: Score = 7;"
		"var current: Score = baseline;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, GenericTypeAliasResolvesExplicitType)
{
	const auto root = ParseCode(
		"type Box<T> = [T];"
		"var values: Box<int> = [1, 2, 3];");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, GenericFunctionInfersArgumentAndReturnType)
{
	const auto root = ParseCode(
		"fn identity<T>(value: T) : T { return value; }"
		"var answer: int = identity(42);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, GenericConstraintAllowsNumericOperations)
{
	const auto root = ParseCode(
		"fn doubleValue<T: int>(value: T) : int { return value + value; }"
		"print doubleValue(21);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ADD));
}

TEST_F(CompilerTest, GenericConstraintRejectsWrongArgumentType)
{
	const auto root = ParseCode(
		"fn doubleValue<T: int>(value: T) : int { return value + value; }"
		"print doubleValue(\"bad\");");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticFunctionArityMismatch)
{
	const auto root = ParseCode(
		"fn add(a, b) { return a + b; }"
		"var x = add(1);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticDefaultParametersAdjustArity)
{
	const auto root = ParseCode(
		"fn add(a: int, b: int = 2) : int { return a + b; }"
		"var x = add(1);"
		"var y = add(1, 3);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticNeverAssignableToDeclaredReturnType)
{
	const auto root = ParseCode(
		"fn failFast() : never { return failFast(); }"
		"fn wrap() : int { return failFast(); }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticReturnExpressionActsAsNever)
{
	const auto root = ParseCode(
		"fn first(flag: bool) : int {"
		"  var value = flag and return 1;"
		"  return 2;"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticThreadLocalDeclarationCompiles)
{
	const auto root = ParseCode(
		"module samples.threading;"
		"thread_local var cache: int = 1;"
		"fn read() : int { return cache; }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticArgumentTypeMismatch)
{
	const auto root = ParseCode(
		"fn square(n : int) { return n * n; }"
		"var res = square(\"not_a_number\");");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticRefArgumentAcceptsAssignableValue)
{
	const auto root = ParseCode(
		"fn inc(n: ref<int>) : void { *n = *n + 1; }"
		"var value: int = 1;"
		"inc(value);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticRefArgumentRejectsNonAssignableExpression)
{
	const auto root = ParseCode(
		"fn inc(n: ref<int>) : void { *n = *n + 1; }"
		"var value: int = 1;"
		"inc(value + 1);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticRefArgumentRejectsConstValue)
{
	const auto root = ParseCode(
		"fn inc(n: ref<int>) : void { *n = *n + 1; }"
		"const value: int = 1;"
		"inc(value);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticRefArgumentRejectsWrongType)
{
	const auto root = ParseCode(
		"fn inc(n: ref<int>) : void { *n = *n + 1; }"
		"var value: string = \"1\";"
		"inc(value);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticReturnTypeMismatch)
{
	const auto root = ParseCode(
		"fn get_num() : int { return \"oops\"; }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticValidTypeCoercion)
{
	const auto root = ParseCode(
		"fn accept_float(f : float) { return f; }"
		"var x = accept_float(42);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticCallNonFunction)
{
	const auto root = ParseCode(
		"var x = 10;"
		"x();");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticComplexRecursiveInference)
{
	const auto root = ParseCode(
		"fn get_val(n : int) : int { return n + 1; }"
		"var x : int = get_val(10);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}
TEST_F(CompilerTest, SemanticSupportsVmBinaryOperators)
{
	const auto root = ParseCode(
		"var a = 7 div 2;"
		"var b = 7 mod 2;"
		"var c = 2 <= 3;"
		"var d = 3 >= 2;"
		"var e = true and false;"
		"var f = true or false;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW({
		const auto chunk = compiler.Compile(root.get());
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DIV));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MOD));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_LESS_EQUAL));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GREATER_EQUAL));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_AND));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_OR));
	});
}

