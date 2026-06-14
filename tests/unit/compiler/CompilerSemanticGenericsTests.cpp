#include "CompilerTest.h"

TEST_F(CompilerTest, SemanticGenericStructTypeResolves)
{
	const auto root = ParseCode(
		"struct Box<T> { value: T; fn get() : T { return self.value; } }"
		"var box: Box<int> = Box(7);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticGenericInterfaceAssignableFromGenericStruct)
{
	const auto root = ParseCode(
		"interface Reader<T> { fn read() : T; }"
		"struct Box<T> implements Reader<T> { value: T; fn read() : T { return self.value; } }"
		"var reader: Reader<int> = Box(5);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticMultiArgumentFunctionTypeAcceptsMatchingLambda)
{
	const auto root = ParseCode(
		"type Reducer = (int, int) -> int;"
		"var reduce: Reducer = fn(left: int, right: int) -> left + right;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticMultiArgumentFunctionTypeRejectsArityMismatch)
{
	const auto root = ParseCode(
		"type Reducer = (int, int) -> int;"
		"var reduce: Reducer = fn(value: int) -> value + 1;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticFunctionLikeInterfaceAssignmentRejected)
{
	const auto root = ParseCode(
		"effect IO { fn read() : int; }"
		"interface Reader { fn read(value: int) : int raises { IO }; }"
		"var reader: Reader = fn(value: int) -> value + 1;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticFunctionTypeRejectsWiderRaisedEffects)
{
	const auto root = ParseCode(
		"effect IO { fn read() : int; }"
		"type Reader = int -> int;"
		"var reader: Reader = fn(value: int) raises { IO } -> { return read() + value; };");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticGenericEnumConstructorInfersConcreteEnumType)
{
	const auto root = ParseCode(
		"enum Option<T> { Some(T) | None }"
		"var value: Option<int> = Some(1);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticActorStateDefaultsAllowOmittedTrailingArguments)
{
	const auto root = ParseCode(
		"actor Wallet {"
		"  state balance: int;"
		"  state currency: string = \"USD\";"
		"}"
		"var wallet = Wallet(5);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticRejectsNonTrailingActorStateDefaults)
{
	const auto root = ParseCode(
		"actor Wallet {"
		"  state currency: string = \"USD\";"
		"  state balance: int;"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, ImportedFunctionTypeAliasWithGenericsCompilesAcrossModules)
{
	WriteFile(
		m_tempRoot / "samples" / "types.aura",
		"module samples.types;"
		"export type Reducer<T> = (T, T) -> T;");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.types as types;"
		"var reduce: types.Reducer<int> = fn(left: int, right: int) -> left + right;");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"));
}
