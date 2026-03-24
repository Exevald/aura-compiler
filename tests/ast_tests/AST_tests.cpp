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

class ASTTest : public ::testing::Test
{
protected:
	static ASTNodePtr ParseCode(const std::string& source)
	{
		std::ifstream file("grammar.txt");
		std::stringstream buffer;
		buffer << file.rdbuf();
		file.close();

		Lexer lexer(source);
		SLRParser parser(lexer, buffer.str());
		if (!parser.Parse())
		{
			return nullptr;
		}
		return parser.GetRoot();
	}
};

bool HasNode(ASTNode* node, const std::string& ruleLhs)
{
	if (!node)
	{
		return false;
	}
	if (const auto* internal = dynamic_cast<InternalNode*>(node))
	{
		if (internal->ruleLhs == ruleLhs)
		{
			return true;
		}
		for (auto& child : internal->children)
		{
			if (HasNode(child.get(), ruleLhs))
			{
				return true;
			}
		}
	}
	return false;
}

bool HasLeaf(ASTNode* node, const std::string& value)
{
	if (!node)
	{
		return false;
	}
	if (const auto* leaf = dynamic_cast<LeafNode*>(node))
	{
		if (leaf->value == value)
			return true;
	}
	if (const auto* internal = dynamic_cast<InternalNode*>(node))
	{
		for (auto& child : internal->children)
		{
			if (HasLeaf(child.get(), value))
			{
				return true;
			}
		}
	}
	return false;
}

TEST_F(ASTTest, ASTStructureForSimpleAssignment)
{
	const auto root = ParseCode("var x = 42;");
	ASSERT_NE(root, nullptr);

	auto* internal = dynamic_cast<InternalNode*>(root.get());
	ASSERT_NE(internal, nullptr);

	bool foundVarDecl = false;
	std::function<void(ASTNode*)> search = [&](ASTNode* node) {
		if (const auto* in = dynamic_cast<InternalNode*>(node))
		{
			if (in->ruleLhs == "var_decl_no_semi")
			{
				foundVarDecl = true;
			}
			for (auto& child : in->children)
			{
				search(child.get());
			}
		}
	};
	search(root.get());
	EXPECT_TRUE(foundVarDecl);
}

TEST_F(ASTTest, ASTPriorityMath)
{
	const auto root = ParseCode("var x = 2 + 3 * 4;");
	ASSERT_NE(root, nullptr);

	auto* program = dynamic_cast<InternalNode*>(root.get());
	ASSERT_NE(program, nullptr);
	EXPECT_EQ(program->ruleLhs, "program");
}

TEST_F(ASTTest, FullModuleAndImportStructure)
{
	const auto root = ParseCode(
		"module sys.network;"
		"import std.io as io;"
		"export var version = 1;");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode(root.get(), "module_decl"));
	EXPECT_TRUE(HasNode(root.get(), "qualified_id"));
	EXPECT_TRUE(HasNode(root.get(), "import_decl"));
	EXPECT_TRUE(HasNode(root.get(), "export_decl"));
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
	EXPECT_TRUE(HasNode(root.get(), "func_decl"));
	EXPECT_TRUE(HasNode(root.get(), "type_params"));
	EXPECT_TRUE(HasNode(root.get(), "type_constraint"));
	EXPECT_TRUE(HasNode(root.get(), "context_req_opt"));
	EXPECT_TRUE(HasNode(root.get(), "effect_spec_opt"));
	EXPECT_TRUE(HasNode(root.get(), "contract_list"));
}

TEST_F(ASTTest, DataStructuresAST)
{
	const auto root = ParseCode(
		"struct Point { x: int; y: int; } "
		"enum Result<T> { Ok(T) | Err(string) } "
		"interface Drawable { fn draw(): void; }");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode(root.get(), "struct_decl_no_semi"));
	EXPECT_TRUE(HasNode(root.get(), "field_decl_list"));
	EXPECT_TRUE(HasNode(root.get(), "enum_decl_no_semi"));
	EXPECT_TRUE(HasNode(root.get(), "variant_list"));
	EXPECT_TRUE(HasNode(root.get(), "interface_decl_no_semi"));
	EXPECT_TRUE(HasNode(root.get(), "method_sig_list"));
}

TEST_F(ASTTest, ControlFlowAndIterators)
{
	const auto root = ParseCode(
		"fn demo() {"
		"  if (true) { print 1; } else { print 0; }"
		"  while (cond) { }"
		"  iter (x of list with [take(5), reverse]) { print x; }"
		"}");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode(root.get(), "if_stmt"));
	EXPECT_TRUE(HasNode(root.get(), "else_opt"));
	EXPECT_TRUE(HasNode(root.get(), "while_stmt"));
	EXPECT_TRUE(HasNode(root.get(), "iter_stmt"));
	EXPECT_TRUE(HasNode(root.get(), "adapter_chain_opt"));
	EXPECT_TRUE(HasLeaf(root.get(), "reverse"));
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
	EXPECT_TRUE(HasNode(root.get(), "effect_def_no_semi"));
	EXPECT_TRUE(HasNode(root.get(), "handle_stmt"));
	EXPECT_TRUE(HasNode(root.get(), "handler_list"));
	EXPECT_TRUE(HasNode(root.get(), "handler"));
}

TEST_F(ASTTest, ActorModelAST)
{
	const auto root = ParseCode(
		"actor Counter { "
		"  state val: int = 0; "
		"  msg inc() { val = val + 1; } "
		"  query get(): int { return val; } "
		"}");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode(root.get(), "actor_decl_no_semi"));
	EXPECT_TRUE(HasNode(root.get(), "actor_field"));
	EXPECT_TRUE(HasNode(root.get(), "actor_method"));
	EXPECT_TRUE(HasLeaf(root.get(), "msg"));
	EXPECT_TRUE(HasLeaf(root.get(), "query"));
}

TEST_F(ASTTest, ExpressionDeepHierarchy)
{
	const auto root = ParseCode("var res = 1 + 2 * 3 == 7;");
	ASSERT_NE(root, nullptr);

	EXPECT_TRUE(HasNode(root.get(), "equality"));
	EXPECT_TRUE(HasNode(root.get(), "additive"));
	EXPECT_TRUE(HasNode(root.get(), "multiplicative"));
}

TEST_F(ASTTest, ClosuresAndArrays)
{
	const auto root = ParseCode(
		"var f = fn(x) -> x * x;"
		"var list = [1, 2, 3];"
		"var call = f(list[0]);");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode(root.get(), "arrow_func"));
	EXPECT_TRUE(HasNode(root.get(), "array_lit"));
	EXPECT_TRUE(HasNode(root.get(), "trailer"));
}

TEST_F(ASTTest, MemoryManagementAST)
{
	const auto root = ParseCode(
		"fn main() {"
		"  transaction(shared db) { db.update(); }"
		"  unsafe { *(p) = 10; }"
		"}");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode(root.get(), "transaction_stmt"));
	EXPECT_TRUE(HasNode(root.get(), "region_expr"));
	EXPECT_TRUE(HasNode(root.get(), "unsafe_stmt"));
}

TEST_F(ASTTest, ComptimeBlocks)
{
	const auto root = ParseCode("var x = comptime { return 1 + 1; };");
	ASSERT_NE(root, nullptr);

	EXPECT_TRUE(HasNode(root.get(), "block_stmt"));
	EXPECT_TRUE(HasLeaf(root.get(), "comptime"));
}