#include "../../src/ast/AST.h"
#include "../../src/lexer/Lexer.h"
#include "../../src/parser/Parser.h"
#include "../../src/vm/core/OpCode.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace VM::Core;

namespace
{

std::filesystem::path GrammarPath()
{
	return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "grammar.md";
}

std::string NormalizeGrammar(std::istream& input)
{
	std::stringstream raw;
	raw << input.rdbuf();

	std::stringstream lines(raw.str());
	std::stringstream normalized;
	std::string line;
	while (std::getline(lines, line))
	{
		if (line.rfind("```", 0) != 0)
		{
			normalized << line << '\n';
		}
	}
	return normalized.str();
}

const std::string& CachedGrammarText()
{
	static const std::string grammar = [] {
		std::ifstream file(GrammarPath());
		if (!file.is_open())
		{
			return std::string{};
		}
		return NormalizeGrammar(file);
	}();
	return grammar;
}

} // namespace

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
	void Visit(UnaryExprNode& n) override
	{
		Check(n);
		if (n.op == targetValue)
		{
			foundValue = true;
		}
		if (n.operand)
		{
			n.operand->Accept(*this);
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
		if (n.member == targetValue)
		{
			foundValue = true;
		}
		if (n.object)
		{
			n.object->Accept(*this);
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
		if (n.explicitType == targetValue)
		{
			foundValue = true;
		}
		if (n.initializer)
		{
			n.initializer->Accept(*this);
		}
	}
	void Visit(TypeAliasNode& n) override
	{
		Check(n);
		if (n.name == targetValue || n.aliasedType == targetValue)
		{
			foundValue = true;
		}
		for (const auto& typeParam : n.typeParams)
		{
			if (typeParam.name == targetValue)
			{
				foundValue = true;
			}
			for (const auto& constraint : typeParam.constraints)
			{
				if (constraint == targetValue)
				{
					foundValue = true;
				}
			}
		}
	}
	void Visit(InterfaceDeclNode& n) override
	{
		Check(n);
		if (n.name == targetValue)
		{
			foundValue = true;
		}
		for (const auto& method : n.methods)
		{
			if (method.name == targetValue || method.returnType == targetValue)
			{
				foundValue = true;
			}
			for (const auto& param : method.params)
			{
				if (param.name == targetValue || param.typeName == targetValue)
				{
					foundValue = true;
				}
			}
		}
	}
	void Visit(StructDeclNode& n) override
	{
		Check(n);
		if (n.name == targetValue)
		{
			foundValue = true;
		}
	for (const auto& field : n.fields)
	{
		if (field.name == targetValue || field.typeName == targetValue)
		{
			foundValue = true;
		}
	}
	for (const auto& interfaceName : n.implementedInterfaces)
	{
		if (interfaceName == targetValue)
		{
			foundValue = true;
		}
	}
	for (const auto& method : n.methods)
	{
		if (method.name == targetValue || method.returnType == targetValue)
		{
			foundValue = true;
		}
		for (const auto& param : method.params)
		{
			if (param.name == targetValue || param.typeName == targetValue)
			{
				foundValue = true;
			}
		}
		if (method.body)
		{
			method.body->Accept(*this);
		}
		for (const auto& metadataNode : method.metadata)
		{
			if (metadataNode)
			{
				metadataNode->Accept(*this);
			}
		}
	}
	for (auto& m : n.metadata)
	{
		if (m)
		{
			m->Accept(*this);
			}
		}
	}
	void Visit(EnumDeclNode& n) override
	{
		Check(n);
		if (n.name == targetValue)
		{
			foundValue = true;
		}
		for (const auto& variant : n.variants)
		{
			if (variant.name == targetValue)
			{
				foundValue = true;
			}
			for (const auto& argType : variant.argTypes)
			{
				if (argType == targetValue)
				{
					foundValue = true;
				}
			}
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
	void Visit(ExportDeclNode& n) override
	{
		Check(n);
		if (n.exportedName == targetValue)
		{
			foundValue = true;
		}
		if (n.declaration)
		{
			n.declaration->Accept(*this);
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
		for (const auto& typeParam : n.typeParams)
		{
			if (typeParam.name == targetValue)
			{
				foundValue = true;
			}
			for (const auto& constraint : typeParam.constraints)
			{
				if (constraint == targetValue)
				{
					foundValue = true;
				}
			}
		}
		for (auto& m : n.metadata)
		{
			if (m)
			{
				m->Accept(*this);
			}
		}
	}
	void Visit(FunctionExprNode& n) override
	{
		Check(n);
		if (n.body)
		{
			n.body->Accept(*this);
		}
	}
	void Visit(CallNode& n) override
	{
		Check(n);
		if (n.callee)
		{
			n.callee->Accept(*this);
		}
		for (const auto& a : n.args)
		{
			a->Accept(*this);
		}
	}
	void Visit(MemberAccessNode& n) override
	{
		Check(n);
		if (n.object)
		{
			n.object->Accept(*this);
		}
		if (n.member == targetValue)
		{
			foundValue = true;
		}
	}
	void Visit(ModuleDeclNode& n) override
	{
		Check(n);
		if (n.qualifiedName == targetValue)
		{
			foundValue = true;
		}
	}
	void Visit(ImportDeclNode& n) override
	{
		Check(n);
		if (n.qualifiedName == targetValue || n.alias == targetValue)
		{
			foundValue = true;
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
	void Visit(UnsafeNode& n) override
	{
		Check(n);
		if (n.body)
		{
			n.body->Accept(*this);
		}
		if (targetValue == "unsafe")
		{
			foundValue = true;
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
		for (auto& adapter : n.adapters)
		{
			if (adapter.argument)
			{
				adapter.argument->Accept(*this);
			}
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
		std::ifstream file(GrammarPath());
		if (!file.is_open())
		{
			return nullptr;
		}

		Lexer lexer(source);
		SLRParser parser(lexer, NormalizeGrammar(file));
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
	EXPECT_TRUE(HasNode<ModuleDeclNode>(root.get()));
	EXPECT_TRUE(HasNode<ImportDeclNode>(root.get()));
	EXPECT_TRUE(HasNode<ExportDeclNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "io"));
	EXPECT_TRUE(HasLeaf(root.get(), "version"));
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

TEST_F(ASTTest, UnaryExpressionParsing)
{
	const auto root = ParseCode("var ok = not false; var num = -5;");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<UnaryExprNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "not"));
	EXPECT_TRUE(HasLeaf(root.get(), "false"));
}

TEST_F(ASTTest, StructDeclarationAndFieldAccessParsing)
{
	const auto root = ParseCode(
		"struct Point { x: int; y: int; }"
		"var p: Point = Point(1, 2);"
		"print p.x;");
	ASSERT_NE(root, nullptr);

	EXPECT_TRUE(HasNode<StructDeclNode>(root.get()));
	EXPECT_TRUE(HasNode<MemberAccessNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "Point"));
	EXPECT_TRUE(HasLeaf(root.get(), "x"));
	EXPECT_TRUE(HasLeaf(root.get(), "y"));
}

TEST_F(ASTTest, StructFieldAssignmentParsing)
{
	const auto root = ParseCode(
		"struct Point { x: int; y: int; }"
		"var p = Point(1, 2);"
		"p.x = 3;");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<AssignmentNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "x"));
}

TEST_F(ASTTest, NestedStructFieldAccessAndAssignmentParsing)
{
	const auto root = ParseCode(
		"struct Inner { x: int; }"
		"struct Outer { inner: Inner; }"
		"var outer = Outer(Inner(1));"
		"print outer.inner.x;"
		"outer.inner.x = 2;");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<StructDeclNode>(root.get()));
	EXPECT_TRUE(HasNode<MemberAccessNode>(root.get()));
	EXPECT_TRUE(HasNode<AssignmentNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "inner"));
	EXPECT_TRUE(HasLeaf(root.get(), "x"));
}

TEST_F(ASTTest, ImportedStructConstructorParsing)
{
	const auto root = ParseCode(
		"module app.main;"
		"import models as m;"
		"var p = m.Point(1, 2);"
		"print p.x;");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<ImportDeclNode>(root.get()));
	EXPECT_TRUE(HasNode<CallNode>(root.get()));
	EXPECT_TRUE(HasNode<MemberAccessNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "Point"));
	EXPECT_TRUE(HasLeaf(root.get(), "x"));
}

TEST_F(ASTTest, StructWholeFieldAssignmentParsing)
{
	const auto root = ParseCode(
		"struct Inner { x: int; }"
		"struct Outer { inner: Inner; }"
		"var outer = Outer(Inner(1));"
		"outer.inner = Inner(2);");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<AssignmentNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "inner"));
	EXPECT_TRUE(HasLeaf(root.get(), "Inner"));
}

TEST_F(ASTTest, EnumDeclarationAndTagAccessParsing)
{
	const auto root = ParseCode(
		"enum Option { None | Some(int) }"
		"var value = Some(42);"
		"print value.tag;");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<EnumDeclNode>(root.get()));
	EXPECT_TRUE(HasNode<CallNode>(root.get()));
	EXPECT_TRUE(HasNode<MemberAccessNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "Option"));
	EXPECT_TRUE(HasLeaf(root.get(), "Some"));
	EXPECT_TRUE(HasLeaf(root.get(), "tag"));
}

TEST_F(ASTTest, ImportedEnumConstructorParsing)
{
	const auto root = ParseCode(
		"module app.main;"
		"import models as m;"
		"var value = m.Some(42);"
		"print value.tag;");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<ImportDeclNode>(root.get()));
	EXPECT_TRUE(HasNode<CallNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "Some"));
	EXPECT_TRUE(HasLeaf(root.get(), "tag"));
}

TEST_F(ASTTest, EnumArgumentIndexParsing)
{
	const auto root = ParseCode(
		"enum Option { None | Some(int) }"
		"var value = Some(42);"
		"print value[0];");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<EnumDeclNode>(root.get()));
	EXPECT_TRUE(HasNode<IndexNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "Some"));
	EXPECT_TRUE(HasLeaf(root.get(), "0"));
}

TEST_F(ASTTest, ConstTypeAliasAndPointerParsing)
{
	const auto root = ParseCode(
		"type Score = int;"
		"const limit: Score = 42;"
		"var ptr_val: ptr<int>;"
		"unsafe { *(ptr_val) = limit; print *(ptr_val); }");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<TypeAliasNode>(root.get()));
	EXPECT_TRUE(HasNode<UnaryExprNode>(root.get()));
	EXPECT_TRUE(HasNode<AssignmentNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "Score"));
	EXPECT_TRUE(HasLeaf(root.get(), "*"));
}

TEST_F(ASTTest, AddressOfParsing)
{
	const auto root = ParseCode(
		"struct Point { x: int; }"
		"fn demo() {"
		"  unsafe {"
		"    var x: int = 1;"
		"    var p: ptr<int> = &x;"
		"    var point = Point(2);"
		"    var field_ptr: ptr<int> = &point.x;"
		"    print *p;"
		"  }"
		"}");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<UnsafeNode>(root.get()));
	EXPECT_TRUE(HasNode<UnaryExprNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "unsafe"));
	EXPECT_TRUE(HasLeaf(root.get(), "&"));
	EXPECT_TRUE(HasLeaf(root.get(), "point"));
	EXPECT_TRUE(HasLeaf(root.get(), "x"));
}

TEST_F(ASTTest, InterfaceDeclarationParsing)
{
	const auto root = ParseCode(
		"interface Reader {"
		"  fn read(buf: [int]) : int;"
		"  fn close() : void;"
		"}"
		"var reader: Reader;");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<InterfaceDeclNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "Reader"));
	EXPECT_TRUE(HasLeaf(root.get(), "read"));
	EXPECT_TRUE(HasLeaf(root.get(), "close"));
}

TEST_F(ASTTest, StructImplementsAndMethodsParsing)
{
	const auto root = ParseCode(
		"interface Reader { fn read() : int; }"
		"struct FileReader implements Reader {"
		"  value: int;"
		"  fn read() : int { return value; }"
		"}"
		"var reader = FileReader(42);"
		"print reader.read();");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<StructDeclNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "FileReader"));
	EXPECT_TRUE(HasLeaf(root.get(), "Reader"));
	EXPECT_TRUE(HasLeaf(root.get(), "read"));
	EXPECT_TRUE(HasLeaf(root.get(), "value"));
}

TEST_F(ASTTest, GenericFunctionAndAliasParsing)
{
	const auto root = ParseCode(
		"type Box<T> = [T];"
		"fn identity<T: int>(value: T) : T { return value; }"
		"var values: Box<int> = [identity(42)];");
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(HasNode<TypeAliasNode>(root.get()));
	EXPECT_TRUE(HasNode<FunctionDeclNode>(root.get()));
	EXPECT_TRUE(HasLeaf(root.get(), "Box"));
	EXPECT_TRUE(HasLeaf(root.get(), "T"));
	EXPECT_TRUE(HasLeaf(root.get(), "int"));
	EXPECT_TRUE(HasLeaf(root.get(), "identity"));
}
