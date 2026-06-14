#include "ASMGeneratorTest.h"

TEST_F(ASMGeneratorTest, EmitsTypedVariableDeclarationsAndPrint)
{
	const auto asmText = EmitAsm(
		"var answer: int = 42;"
		"var ratio: float = 3.5;"
		"var name: string = \"aura\";"
		"var ok = true;"
		"var missing = null;"
		"print name;");

	EXPECT_EQ(
		asmText,
		"DECLARE_VAR answer:int\n"
		"PUSH_INT 42\n"
		"STORE answer\n"
		"DECLARE_VAR ratio:float\n"
		"PUSH_FLOAT 3.5\n"
		"STORE ratio\n"
		"DECLARE_VAR name:string\n"
		"PUSH_STRING \"aura\"\n"
		"STORE name\n"
		"DECLARE_VAR ok\n"
		"PUSH_BOOL true\n"
		"STORE ok\n"
		"DECLARE_VAR missing\n"
		"PUSH_NULL\n"
		"STORE missing\n"
		"LOAD name\n"
		"PRINT\n");
}

TEST_F(ASMGeneratorTest, EmitsUnaryAndBinaryExpressionsInEvaluationOrder)
{
	const auto asmText = EmitAsm(
		"var value = -1 + 2 * 3;"
		"var flag = not false;");

	EXPECT_EQ(
		asmText,
		"DECLARE_VAR value\n"
		"PUSH_INT 1\n"
		"UNARY_NEG\n"
		"PUSH_INT 2\n"
		"PUSH_INT 3\n"
		"MUL\n"
		"ADD\n"
		"STORE value\n"
		"DECLARE_VAR flag\n"
		"PUSH_BOOL false\n"
		"UNARY_NOT\n"
		"STORE flag\n");
}

TEST_F(ASMGeneratorTest, EmitsAssignmentAsValueThenStore)
{
	const auto asmText = EmitAsm(
		"var counter: int = 0;"
		"counter = counter + 1;");

	EXPECT_EQ(
		asmText,
		"DECLARE_VAR counter:int\n"
		"PUSH_INT 0\n"
		"STORE counter\n"
		"LOAD counter\n"
		"PUSH_INT 1\n"
		"ADD\n"
		"STORE counter\n");
}

TEST_F(ASMGeneratorTest, EmitsIfWithoutElseLabels)
{
	const auto asmText = EmitAsm(
		"if (true) {"
		"  print 1;"
		"}");

	EXPECT_EQ(
		asmText,
		"PUSH_BOOL true\n"
		"JUMP_IF_FALSE L0\n"
		"PUSH_INT 1\n"
		"PRINT\n"
		"LABEL L0\n");
}

TEST_F(ASMGeneratorTest, EmitsIfElseControlFlow)
{
	const auto asmText = EmitAsm(
		"if (1 < 2) {"
		"  print 3;"
		"} else {"
		"  print 4;"
		"}");

	EXPECT_EQ(
		asmText,
		"PUSH_INT 1\n"
		"PUSH_INT 2\n"
		"CMP_LT\n"
		"JUMP_IF_FALSE L0\n"
		"PUSH_INT 3\n"
		"PRINT\n"
		"JUMP L1\n"
		"LABEL L0\n"
		"PUSH_INT 4\n"
		"PRINT\n"
		"LABEL L1\n");
}
