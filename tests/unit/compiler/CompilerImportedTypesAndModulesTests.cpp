#include "CompilerTest.h"

TEST_F(CompilerTest, ImportedStructConstructorAndFieldAccessCompileAcrossModules)
{
	WriteFile(
		m_tempRoot / "samples" / "models.aura",
		"module samples.models;"
		"struct Point { x: int; y: int; }");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.models as models;"
		"var p = models.Point(10, 20);"
		"print p.x;");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_STRUCT));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_GET));
	});
}

TEST_F(CompilerTest, ImportedNestedStructConstructorAndAssignmentCompileAcrossModules)
{
	WriteFile(
		m_tempRoot / "samples" / "models.aura",
		"module samples.models;"
		"struct Inner { x: int; }"
		"struct Outer { inner: Inner; }");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.models as models;"
		"var outer = models.Outer(models.Inner(10));"
		"outer.inner.x = 42;"
		"print outer.inner.x;");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_STRUCT));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_SET));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_GET));
	});
}

TEST_F(CompilerTest, ImportedStructWholeFieldAssignmentCompileAcrossModules)
{
	WriteFile(
		m_tempRoot / "samples" / "models.aura",
		"module samples.models;"
		"struct Inner { x: int; }"
		"struct Outer { inner: Inner; }");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.models as models;"
		"var outer = models.Outer(models.Inner(10));"
		"outer.inner = models.Inner(42);"
		"print outer.inner.x;");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_STRUCT));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_SET));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_GET));
	});
}

TEST_F(CompilerTest, ImportedEnumConstructorAndTagAccessCompileAcrossModules)
{
	WriteFile(
		m_tempRoot / "samples" / "models.aura",
		"module samples.models;"
		"enum Option { None | Some(int) }");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.models as models;"
		"var value = models.Some(42);"
		"print value.tag;");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_ENUM));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_ENUM_TAG));
	});
}

TEST_F(CompilerTest, ImportedEnumArgumentAccessCompileAcrossModules)
{
	WriteFile(
		m_tempRoot / "samples" / "models.aura",
		"module samples.models;"
		"enum Option { None | Some(int) }");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.models as models;"
		"var value = models.Some(42);"
		"print value[0];");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_ENUM));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_ENUM_ARG));
	});
}

TEST_F(CompilerTest, ImportedGenericStructNestedFieldAccessCompilesAcrossModules)
{
	WriteFile(
		m_tempRoot / "samples" / "models.aura",
		"module samples.models;"
		"struct Point { x: int; }"
		"struct Box<T> { value: T; }");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.models as models;"
		"var box: models.Box<models.Point> = models.Box(models.Point(7));"
		"print box.value.x;");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_STRUCT));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_GET));
	});
}

TEST_F(CompilerTest, ImportedGenericInterfaceAssignmentCompilesAcrossModules)
{
	WriteFile(
		m_tempRoot / "samples" / "api.aura",
		"module samples.api;"
		"interface Reader<T> { fn read() : T; }");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
			"module samples.main;"
			"import samples.api as api;"
			"struct Box implements api.Reader<int> { value: int; fn read() : int { return self.value; } }"
			"var reader: api.Reader<int> = Box(5);"
			"print reader.read();");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_CALL));
	});
}

TEST_F(CompilerTest, ImportedStructConstructorInsideComptimeCompilesAcrossModules)
{
	WriteFile(
		m_tempRoot / "samples" / "models.aura",
		"module samples.models;"
		"struct Point { x: int; y: int; }");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.models as models;"
		"const x = comptime { return models.Point(7, 9).x; };"
		"print x;");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_PRINT));
		bool containsSeven = false;
		for (const auto& constant : chunk.constants)
		{
			if ((std::holds_alternative<long long>(constant) && std::get<long long>(constant) == 7)
				|| (std::holds_alternative<double>(constant) && std::get<double>(constant) == 7.0))
			{
				containsSeven = true;
				break;
			}
		}
		EXPECT_TRUE(containsSeven);
	});
}

TEST_F(CompilerTest, ModuleDeclarationMismatchReportsClearError)
{
	WriteFile(
		m_tempRoot / "samples" / "wrong_name.aura",
		"module samples.not_wrong_name;"
		"print 1;");

	const std::string emptySource;
	const Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	try
	{
		(void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "wrong_name.aura");
		FAIL() << "Expected mismatch error";
	}
	catch (const std::runtime_error& e)
	{
		EXPECT_NE(std::string(e.what()).find("Module declaration mismatch"), std::string::npos);
	}
}

TEST_F(CompilerTest, AuraModMakesModuleNamesRelativeToProjectRoot)
{
	WriteFile(
		m_tempRoot / "aura.mod",
		"module microservicetemplate\n");
	WriteFile(
		m_tempRoot / "internal" / "health.aura",
		"module microservicetemplate.internal.health;"
		"fn ok() : int { return 1; }");
	WriteFile(
		m_tempRoot / "cmd" / "main.aura",
		"module microservicetemplate.cmd.main;"
		"import microservicetemplate.internal.health as health;"
		"print health.ok();");

	const std::string emptySource;
	const Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "cmd" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_PRINT));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_CALL));
	});
}

TEST_F(CompilerTest, AuraModMismatchReportsProjectRelativeExpectation)
{
	WriteFile(
		m_tempRoot / "aura.mod",
		"module microservicetemplate\n");
	WriteFile(
		m_tempRoot / "cmd" / "main.aura",
		"module wrong.cmd.main;"
		"print 1;");

	const std::string emptySource;
	const Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	try
	{
		(void)compiler.CompileFileToChunk(m_tempRoot / "cmd" / "main.aura");
		FAIL() << "Expected mismatch error";
	}
	catch (const std::runtime_error& e)
	{
		const std::string message = e.what();
		EXPECT_NE(message.find("Module declaration mismatch"), std::string::npos);
		EXPECT_NE(message.find("microservicetemplate.cmd.main"), std::string::npos);
		EXPECT_NE(message.find("aura.mod"), std::string::npos);
	}
}

TEST_F(CompilerTest, AuraModRejectsImportsOutsideCurrentModuleName)
{
	WriteFile(
		m_tempRoot / "aura.mod",
		"module microservicetemplate\n");
	WriteFile(
		m_tempRoot / "cmd" / "main.aura",
		"module microservicetemplate.cmd.main;"
		"import anothermodule.health as health;"
		"print 1;");

	const std::string emptySource;
	const Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	try
	{
		(void)compiler.CompileFileToChunk(m_tempRoot / "cmd" / "main.aura");
		FAIL() << "Expected import resolution error";
	}
	catch (const std::runtime_error& e)
	{
		EXPECT_NE(std::string(e.what()).find("local module imports must stay within module"), std::string::npos);
	}
}

TEST_F(CompilerTest, CyclicImportsReportClearError)
{
	WriteFile(
		m_tempRoot / "samples" / "a.aura",
		"module samples.a;"
		"import samples.b as b;"
		"fn fa() : int { return b.fb(); }");
	WriteFile(
		m_tempRoot / "samples" / "b.aura",
		"module samples.b;"
		"import samples.a as a;"
		"fn fb() : int { return 1; }");

	const std::string emptySource;
	const Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	try
	{
		(void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "a.aura");
		FAIL() << "Expected cyclic import error";
	}
	catch (const std::runtime_error& e)
	{
		EXPECT_NE(std::string(e.what()).find("Cyclic module import"), std::string::npos);
	}
}

TEST_F(CompilerTest, ModuleCacheAllowsRecompileAfterFileRemoval)
{
	const auto importedPath = m_tempRoot / "samples" / "cached_math.aura";
	const auto mainPath = m_tempRoot / "samples" / "cache_main.aura";

	WriteFile(
		importedPath,
		"module samples.cached_math;"
		"fn sum(a: int, b: int) : int { return a + b; }");
	WriteFile(
		mainPath,
		"module samples.cache_main;"
		"import samples.cached_math as math;"
		"print math.sum(1, 2);");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW((void)compiler.CompileFileToChunk(mainPath));
	std::filesystem::remove(importedPath);
	EXPECT_NO_THROW((void)compiler.CompileFileToChunk(mainPath));
}
