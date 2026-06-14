#include "utils/ASTTest.h"

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
