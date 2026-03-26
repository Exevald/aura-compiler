#include "../../src/ast/AST.h"
#include "../../src/lexer/Lexer.h"
#include "../../src/parser/Parser.h"
#include "../../src/vm/core/OpCode.h"

#include <fstream>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using namespace VM::Core;

class ASTSearcher : public ASTVisitor
{
public:
	std::string targetIdentifier;
	std::string targetValue;
	std::string targetRule;
	bool foundType = false;
	bool foundIdentifier = false;
	bool foundValue = false;
	bool foundRule = false;
	const std::type_info* targetType = nullptr;

	void Check(ASTNode& node)
	{
		if (targetType && typeid(node) == *targetType)
		{
			foundType = true;
		}
	}

	void Visit(IntegerLiteralNode& n) override
	{
		Check(n);
		if (std::to_string(n.value) == targetValue)
		{
			foundValue = true;
		}
	}
	void Visit(FloatLiteralNode& n) override
	{
		Check(n);
	}
	void Visit(StringLiteralNode& n) override
	{
		Check(n);
		if (n.value == targetValue)
		{
			foundValue = true;
		}
	}
	void Visit(IdentifierNode& n) override
	{
		Check(n);
		if (n.name == targetIdentifier || n.name == targetValue)
		{
			foundIdentifier = foundValue = true;
		}
	}
	void Visit(BinaryExprNode& n) override
	{
		Check(n);
		n.left->Accept(*this);
		n.right->Accept(*this);
	}
	void Visit(AssignmentNode& n) override
	{
		Check(n);
		if (n.name == targetValue)
		{
			foundValue = true;
		}
		if (n.value)
		{
			n.value->Accept(*this);
		}
		if (n.index)
		{
			n.index->Accept(*this);
		}
	}
	void Visit(VarDeclNode& n) override
	{
		Check(n);
		if (n.name == targetValue)
		{
			foundValue = true;
		}
		if (n.initializer)
		{
			n.initializer->Accept(*this);
		}
	}
	void Visit(BlockNode& n) override
	{
		Check(n);
		for (auto& s : n.statements)
		{
			if (s)
			{
				s->Accept(*this);
			}
		}
	}
	void Visit(IfStatementNode& n) override
	{
		Check(n);
		n.condition->Accept(*this);
		n.thenBlock->Accept(*this);
		if (n.elseBlock)
		{
			n.elseBlock->Accept(*this);
		}
	}
	void Visit(WhileStatementNode& n) override
	{
		Check(n);
		n.condition->Accept(*this);
		n.body->Accept(*this);
	}
	void Visit(FunctionDeclNode& n) override
	{
		Check(n);
		if (n.name == targetValue)
		{
			foundValue = true;
		}
		if (n.body)
		{
			n.body->Accept(*this);
		}
		for (auto& m : n.metadata)
		{
			if (m)
			{
				m->Accept(*this);
			}
		}
	}
	void Visit(CallNode& n) override
	{
		Check(n);
		for (const auto& a : n.args)
		{
			a->Accept(*this);
		}
	}
	void Visit(ReturnNode& n) override
	{
		Check(n);
		if (n.value)
		{
			n.value->Accept(*this);
		}
	}
	void Visit(PrintNode& n) override
	{
		Check(n);
		if (n.value)
		{
			n.value->Accept(*this);
		}
	}
	void Visit(ArrayLiteralNode& n) override
	{
		Check(n);
		for (auto& e : n.elements)
		{
			e->Accept(*this);
		}
	}
	void Visit(IndexNode& n) override
	{
		Check(n);
		n.container->Accept(*this);
		n.index->Accept(*this);
	}
	void Visit(IterNode& n) override
	{
		Check(n);
		if (n.varName == targetValue)
		{
			foundValue = true;
		}
		if (n.collection)
		{
			n.collection->Accept(*this);
		}
		if (n.body)
		{
			n.body->Accept(*this);
		}
	}

	void Visit(RawNode& n) override
	{
		Check(n);
		if (n.ruleName == targetRule)
		{
			foundRule = true;
		}
		for (auto& c : n.children)
		{
			if (c)
			{
				c->Accept(*this);
			}
		}
	}
	void Visit(LeafNode& n) override
	{
		if (n.value == targetValue)
		{
			foundValue = true;
		}
	}
	void Visit(ComptimeNode& n) override
	{
		Check(n);
		if (n.body)
		{
			n.body->Accept(*this);
		}
		foundValue = (targetValue == "comptime");
	}
};

class ASTTest : public ::testing::Test
{
protected:
	static ASTNodePtr ParseCode(const std::string& source)
	{
		std::ifstream file("grammar.txt");
		std::stringstream buffer;
		buffer << file.rdbuf();

		Lexer lexer(source);
		SLRParser parser(lexer, buffer.str());
		if (!parser.Parse())
		{
			return nullptr;
		}
		return parser.GetRoot();
	}

	template <typename T>
	static bool HasNode(ASTNode* root)
	{
		ASTSearcher searcher;
		searcher.targetType = &typeid(T);
		if (root)
		{
			root->Accept(searcher);
		}
		return searcher.foundType;
	}

	static bool HasRule(ASTNode* root, const std::string& ruleName)
	{
		ASTSearcher searcher;
		searcher.targetRule = ruleName;
		if (root)
		{
			root->Accept(searcher);
		}
		return searcher.foundRule;
	}

	static bool HasLeaf(ASTNode* root, const std::string& value)
	{
		ASTSearcher searcher;
		searcher.targetValue = value;
		if (root)
		{
			root->Accept(searcher);
		}
		return searcher.foundValue;
	}
};

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
	EXPECT_TRUE(HasRule(root.get(), "module_decl"));
	EXPECT_TRUE(HasRule(root.get(), "import_decl"));
	EXPECT_TRUE(HasLeaf(root.get(), "network"));
	EXPECT_TRUE(HasLeaf(root.get(), "io"));
}

TEST_F(ASTTest, FunctionWithComplexSignature)
{
	const auto root = ParseCode(
		"fn calculate<T: Numeric>(x: T) : T "
		"with { ctx: Context } "
		"raises { Error } "
		"requires (x > 0) "
		"{ return x; }");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<FunctionDeclNode>(root.get()));
	EXPECT_TRUE(HasRule(root.get(), "contract_list"));
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
	EXPECT_TRUE(HasRule(root.get(), "effect_def_no_semi"));
	EXPECT_TRUE(HasRule(root.get(), "handle_stmt"));
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

TEST_F(ASTTest, ComptimeBlocks)
{
	const auto root = ParseCode("var x = comptime { return 1 + 1; };");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasLeaf(root.get(), "comptime"));
}