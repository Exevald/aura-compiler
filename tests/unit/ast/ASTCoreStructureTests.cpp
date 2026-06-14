#include "utils/ASTTest.h"

TEST_F(ASTTest, ASTStructureForSimpleAssignment)
{
	const auto root = ParseCode("var x = 42;");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<VarDeclNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "x"));
}

TEST_F(ASTTest, ASTPriorityMath)
{
	const auto root = ParseCode("var x = 2 + 3 * 4;");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<BinaryExprNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "4"));
}

TEST_F(ASTTest, FullModuleAndImportStructure)
{
	const auto root = ParseCode(
		"module sys.network;"
		"import std.io as io;"
		"export var version = 1;");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<ModuleDeclNode>(root.get()));
	EXPECT_TRUE(HasNode<ImportDeclNode>(root.get()));
	EXPECT_TRUE(HasNode<ExportDeclNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "io"));
	EXPECT_TRUE(HasLeaf(root.get(), "version"));
}

TEST_F(ASTTest, FunctionWithComplexSignature)
{
	const auto root = ParseCode(
		"comptime fn calculate<T: Numeric>(x: T, retries: int = 3) : T "
		"with { ctx: Context } "
		"raises { Error } "
		"requires (x > 0) "
		"ensures (retries >= 0) "
		"{ return x; }");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<FunctionDeclNode>(root.get()));
	EXPECT_TRUE(HasNode<ContractNode>(root.get()));
	EXPECT_TRUE(HasNode<BinaryExprNode>(root.get()));
}

TEST_F(ASTTest, ControlFlowAndIterators)
{
	const auto root = ParseCode(
		"fn demo() {"
		"  if (true) { print 1; } else { print 0; }"
		"  while (cond) { }"
		"  iter (x of list) { print x; }"
		"}");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<IfStatementNode>(root.get()));
	EXPECT_TRUE(HasNode<WhileStatementNode>(root.get()));
	EXPECT_TRUE(HasNode<IterNode>(root.get()));
}

TEST_F(ASTTest, AlgebraicEffectsAST)
{
	const auto root = ParseCode(
		"effect Logger { fn log(s: string): void; } "
		"fn test() { "
		"  handle do_work() with { "
		"    effect log(m) -> { print m; } "
		"  }"
		"}");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasLeaf(root.get(), "Logger"));
	EXPECT_TRUE(HasLeaf(root.get(), "do_work"));
}

TEST_F(ASTTest, TransactionAndHandleNodes)
{
	const auto root = ParseCode(
		"effect IO { fn read() : int; }"
		"fn run() raises { IO } { return read(); }"
		"fn main() {"
		"  transaction(shared db | shared cache) {"
		"    handle run() with { effect read() -> { return 7; } }"
		"  }"
		"}");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<EffectDeclNode>(root.get()));
	EXPECT_TRUE(HasNode<TransactionNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "db"));
	EXPECT_TRUE(HasLeaf(root.get(), "cache"));
	EXPECT_TRUE(HasNode<HandleNode>(root.get()));
}

TEST_F(ASTTest, ActorDeclarationAST)
{
	const auto root = ParseCode(
		"actor Wallet {"
		"  state balance: int = 0;"
		"  msg deposit(amount: int) { balance = balance + amount; }"
		"  query getBalance() : int { return balance; }"
		"}");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<ActorDeclNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "Wallet"));
	EXPECT_TRUE(HasLeaf(root.get(), "balance"));
	EXPECT_TRUE(HasLeaf(root.get(), "deposit"));
	EXPECT_TRUE(HasLeaf(root.get(), "getBalance"));
}

TEST_F(ASTTest, ExpressionDeepHierarchy)
{
	const auto root = ParseCode("var res = 1 + 2 * 3 == 7;");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<BinaryExprNode>(root.get()));
}

TEST_F(ASTTest, ClosuresAndArrays)
{
	const auto root = ParseCode(
		"var list = [1, 2, 3];"
		"print list[0];");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<ArrayLiteralNode>(root.get()));
	EXPECT_TRUE(HasNode<IndexNode>(root.get()));
}
