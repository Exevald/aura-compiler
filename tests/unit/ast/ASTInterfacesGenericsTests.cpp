#include "utils/ASTTest.h"

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
