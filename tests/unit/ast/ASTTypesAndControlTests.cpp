#include "utils/ASTTest.h"

TEST_F(ASTTest, ComptimeBlocks)
{
	const auto root = ParseCode("var x = comptime { return 1 + 1; };");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasLeaf(root.get(), "comptime"));
}

TEST_F(ASTTest, ExplicitVariableTypeParsing)
{
	const auto root = ParseCode("shared var x : ptr<int>;");
	ASSERT_NE(root, nullptr);

	class VarFinder final : public ASTSearcher
	{
	public:
		VarDeclNode* varNode = nullptr;

		void Visit(VarDeclNode& n) override
		{
			ASTSearcher::Visit(n);
			if (!varNode)
			{
				varNode = &n;
			}
		}
	};

	VarFinder finder;
	root->Accept(finder);
	auto* varNode = finder.varNode;
	ASSERT_NE(varNode, nullptr) << "Could not find VarDeclNode in the AST";

	EXPECT_EQ(varNode->name, "x");
	EXPECT_EQ(varNode->explicitType, "ptr<int>");
	EXPECT_EQ(varNode->storageClass, VarDeclNode::StorageClass::Shared);
}

TEST_F(ASTTest, FunctionReturnTypeParsing)
{
	const auto root = ParseCode("fn test() : float { return 1.0; }");
	ASSERT_NE(root, nullptr);

	FunctionDeclNode* fnNode = nullptr;
	class FunctionFinder final : public ASTSearcher
	{
	public:
		FunctionDeclNode* fnNode = nullptr;

		void Visit(FunctionDeclNode& n) override
		{
			ASTSearcher::Visit(n);
			if (!fnNode)
			{
				fnNode = &n;
			}
		}
	};

	FunctionFinder finder;
	root->Accept(finder);
	fnNode = finder.fnNode;

	ASSERT_NE(fnNode, nullptr) << "Could not find FunctionDeclNode in the AST";
	EXPECT_EQ(fnNode->name, "test");
	EXPECT_EQ(fnNode->returnType, "float");
}

TEST_F(ASTTest, IterAdapterChainParsing)
{
	const auto root = ParseCode(
		"fn walk(list: [int]) {"
		"  iter (item of list with [drop(1), take(2), reverse, filter(fn(v: int) -> v > 0), transform(fn(v: int) -> v + 10)]) {"
		"    print item;"
		"  }"
		"}");
	ASSERT_NE(root, nullptr);

	class IterFinder final : public ASTSearcher
	{
	public:
		IterNode* iterNode = nullptr;

		void Visit(IterNode& n) override
		{
			ASTSearcher::Visit(n);
			if (!iterNode)
			{
				iterNode = &n;
			}
		}
	};

	IterFinder finder;
	root->Accept(finder);
	auto* iterNode = finder.iterNode;
	ASSERT_NE(iterNode, nullptr);
	ASSERT_EQ(iterNode->adapters.size(), 5);

	EXPECT_EQ(iterNode->adapters[0].kind, IterAdapterKind::Drop);
	EXPECT_EQ(iterNode->adapters[1].kind, IterAdapterKind::Take);
	EXPECT_EQ(iterNode->adapters[2].kind, IterAdapterKind::Reverse);
	EXPECT_EQ(iterNode->adapters[3].kind, IterAdapterKind::Filter);
	EXPECT_EQ(iterNode->adapters[4].kind, IterAdapterKind::Transform);

	ASSERT_NE(iterNode->adapters[0].argument, nullptr);
	ASSERT_NE(iterNode->adapters[1].argument, nullptr);
	EXPECT_EQ(iterNode->adapters[2].argument, nullptr);
	EXPECT_TRUE(HasNode<FunctionExprNode>(iterNode->adapters[3].argument.get()));
	EXPECT_TRUE(HasNode<FunctionExprNode>(iterNode->adapters[4].argument.get()));
}
