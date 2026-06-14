#include "CompilerTest.h"

TEST_F(CompilerTest, BytecodeModuleImportResolvesMemberAccess)
{
	const auto root = ParseCode(
		"module app.main;"
		"import math as m;"
		"fn run() { print m.square; }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
}

TEST_F(CompilerTest, FileImportCompilesAcrossModules)
{
	WriteFile(
		m_tempRoot / "samples" / "math_utils.aura",
		"module samples.math_utils;"
		"fn sum(a: int, b: int) : int { return a + b; }");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.math_utils as math;"
		"print math.sum(20, 22);");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
	});
}

TEST_F(CompilerTest, FolderPackageImportCompilesAcrossModules)
{
	WriteFile(
		m_tempRoot / "samples" / "math_utils" / "api.aura",
		"module samples.math_utils;"
		"fn sum(a: int, b: int) : int { return a + b; }");
	WriteFile(
		m_tempRoot / "samples" / "main" / "main.aura",
		"module samples.main;"
		"import samples.math_utils as math;"
		"print math.sum(20, 22);");

	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
	});
}

TEST_F(CompilerTest, MultiFilePackageCompilesAcrossModules)
{
	WriteFile(
		m_tempRoot / "samples" / "math_utils" / "a_helpers.aura",
		"module samples.math_utils;"
		"fn add(a: int, b: int) : int { return a + b; }");
	WriteFile(
		m_tempRoot / "samples" / "math_utils" / "b_api.aura",
		"module samples.math_utils;"
		"fn sum(a: int, b: int) : int { return add(a, b); }");
	WriteFile(
		m_tempRoot / "samples" / "main" / "main.aura",
		"module samples.main;"
		"import samples.math_utils as math;"
		"print math.sum(20, 22);");

	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
	});
}
