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
