#include "CompilerTest.h"

#include <chrono>

namespace
{

void TouchFile(const std::filesystem::path& path, const std::chrono::seconds offset = std::chrono::seconds{ 0 })
{
	std::filesystem::last_write_time(
		path,
		std::filesystem::file_time_type::clock::now() + offset);
}

bool ChunkContainsIntConstant(const VM::Execution::Chunk& chunk, const int64_t target)
{
	for (const auto& constant : chunk.constants)
	{
		if (std::holds_alternative<int64_t>(constant) && std::get<int64_t>(constant) == target)
		{
			return true;
		}
		if (std::holds_alternative<FunctionPtr>(constant))
		{
			const auto& fn = std::get<FunctionPtr>(constant);
			if (fn && fn->chunk && ChunkContainsIntConstant(*fn->chunk, target))
			{
				return true;
			}
		}
	}
	return false;
}

} // namespace

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

TEST_F(CompilerTest, PackageCacheRefreshesAfterSourceChange)
{
	WriteFile(
		m_tempRoot / "samples" / "math_utils.aura",
		"module samples.math_utils;"
		"export var version: int = 7;"
		"fn hidden() : int { return 35; }"
		"export fn answer() : int { return hidden() + version; };");
	TouchFile(m_tempRoot / "samples" / "math_utils.aura");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.math_utils as math;"
		"print math.version;"
		"print math.answer();");

	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	const auto first = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
	EXPECT_TRUE(ChunkContainsIntConstant(first, 7));
	EXPECT_FALSE(ChunkContainsIntConstant(first, 8));

	WriteFile(
		m_tempRoot / "samples" / "math_utils.aura",
		"module samples.math_utils;"
		"export var version: int = 8;"
		"fn hidden() : int { return 35; }"
		"export fn answer() : int { return hidden() + version; };");
	TouchFile(m_tempRoot / "samples" / "math_utils.aura", std::chrono::seconds{ 3600 });

	const auto second = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
	EXPECT_TRUE(ChunkContainsIntConstant(second, 8));
	EXPECT_FALSE(ChunkContainsIntConstant(second, 7));
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
