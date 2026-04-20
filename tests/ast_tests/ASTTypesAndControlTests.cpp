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

	auto* block = dynamic_cast<BlockNode*>(root.get());
	ASSERT_NE(block, nullptr) << "Root must be a BlockNode";
	ASSERT_FALSE(block->statements.empty()) << "Block must not be empty";

	auto* varNode = dynamic_cast<VarDeclNode*>(block->statements[0].get());
	ASSERT_NE(varNode, nullptr) << "First statement must be VarDeclNode";

	EXPECT_EQ(varNode->name, "x");
	EXPECT_EQ(varNode->explicitType, "ptr<int>");
	EXPECT_EQ(varNode->storageClass, VarDeclNode::StorageClass::Shared);
}

TEST_F(ASTTest, FunctionReturnTypeParsing)
{
	const auto root = ParseCode("fn test() : float { return 1.0; }");
	ASSERT_NE(root, nullptr);

	auto* block = dynamic_cast<BlockNode*>(root.get());
	ASSERT_NE(block, nullptr) << "Root must be a BlockNode";

	FunctionDeclNode* fnNode = nullptr;
	for (auto& stmt : block->statements)
	{
		if (auto* candidate = dynamic_cast<FunctionDeclNode*>(stmt.get()))
		{
			fnNode = candidate;
			break;
		}
	}

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
