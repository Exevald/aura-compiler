#include "CompilerTest.h"

TEST_F(CompilerTest, InterfaceDeclarationAndUsageCompile)
{
	const auto root = ParseCode(
		"interface Reader {"
		"  fn read(buf: [int]) : int;"
		"  fn close() : void;"
		"}"
		"fn consume(reader: Reader) : void { return; }"
		"var current: Reader;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, ModuleDoesNotSatisfyInterfaceCompile)
{
	WriteFile(
		m_tempRoot / "samples" / "reader.aura",
		"module samples.reader;"
		"fn read(buf: [int]) : int { return 7; }"
		"fn close() : void { return; }");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.reader as reader_mod;"
		"interface Reader {"
		"  fn read(buf: [int]) : int;"
		"  fn close() : void;"
		"}"
		"var reader: Reader = reader_mod;"
		"print reader.read([1, 2]);");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"), std::runtime_error);
}

TEST_F(CompilerTest, SingleMethodInterfaceFunctionDispatchRejected)
{
	const auto root = ParseCode(
		"interface Action { fn run(v: int) : int; }"
		"var action: Action = fn(v: int) -> v + 1;"
		"print action.run(41);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW((void)compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, InterfaceAssignmentMismatchRejected)
{
	WriteFile(
		m_tempRoot / "samples" / "reader.aura",
		"module samples.reader;"
		"fn read(buf: [int]) : int { return 7; }");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.reader as reader_mod;"
		"interface Reader {"
		"  fn read(buf: [int]) : int;"
		"  fn close() : void;"
		"}"
		"var reader: Reader = reader_mod;");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"), std::runtime_error);
}

TEST_F(CompilerTest, ExportedFunctionAndVarCompileAcrossModules)
{
	WriteFile(
		m_tempRoot / "samples" / "api.aura",
		"module samples.api;"
		"export var version: int = 7;"
		"fn hidden() : int { return 35; }"
		"export fn answer() : int { return hidden() + version; };");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.api as api;"
		"print api.version;"
		"print api.answer();");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DEFINE_GLOBAL));
	});
}

TEST_F(CompilerTest, UnexportedModuleMemberRejected)
{
	WriteFile(
		m_tempRoot / "samples" / "api.aura",
		"module samples.api;"
		"var secret: int = 7;"
		"fn hidden() : int { return secret; }"
		"export fn answer() : int { return hidden(); };");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.api as api;"
		"print api.hidden();");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"), std::runtime_error);
}

TEST_F(CompilerTest, ExportByIdentifierMarksExistingDeclaration)
{
	WriteFile(
		m_tempRoot / "samples" / "api.aura",
		"module samples.api;"
		"fn answer() : int { return 42; }"
		"export answer;");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.api as api;"
		"print api.answer();");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"));
}

TEST_F(CompilerTest, StructMethodCallCompile)
{
	const auto root = ParseCode(
		"struct Counter {"
		"  value: int;"
		"  fn increment() : int {"
		"    value = value + 1;"
		"    return value;"
		"  }"
		"}"
		"var counter = Counter(41);"
		"print counter.increment();");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_GLOBAL));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_SET));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_CALL));
}

TEST_F(CompilerTest, StructSatisfiesInterfaceCompile)
{
	const auto root = ParseCode(
		"interface Counter { fn increment() : int; }"
		"struct Box implements Counter {"
		"  value: int;"
		"  fn increment() : int {"
		"    value = value + 1;"
		"    return value;"
		"  }"
		"}"
		"var counter: Counter = Box(41);"
		"print counter.increment();");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_GLOBAL));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_CALL));
}

TEST_F(CompilerTest, StructInterfaceMismatchRejected)
{
	const auto root = ParseCode(
		"interface Counter { fn increment() : int; }"
		"struct Box implements Counter {"
		"  value: int;"
		"  fn increment() : void {"
		"    return;"
		"  }"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticStructConstructorArityMismatch)
{
	const auto root = ParseCode(
		"struct Point { x: int; y: int; }"
		"var p = Point(1);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticStructFieldTypeMismatch)
{
	const auto root = ParseCode(
		"struct Point { x: int; y: int; }"
		"var p = Point(1, \"bad\");");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticStructFieldAssignmentTypeMismatch)
{
	const auto root = ParseCode(
		"struct Point { x: int; y: int; }"
		"var p = Point(1, 2);"
		"p.x = \"bad\";");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticNestedStructFieldAssignmentTypeMismatch)
{
	const auto root = ParseCode(
		"struct Inner { x: int; }"
		"struct Outer { inner: Inner; }"
		"var outer = Outer(Inner(1));"
		"outer.inner.x = \"bad\";");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticStructWholeFieldAssignmentTypeMismatch)
{
	const auto root = ParseCode(
		"struct Inner { x: int; }"
		"struct Other { x: int; }"
		"struct Outer { inner: Inner; }"
		"var outer = Outer(Inner(1));"
		"outer.inner = Other(2);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticEnumConstructorArityMismatch)
{
	const auto root = ParseCode(
		"enum Option { None | Some(int) }"
		"var value = Some();");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticContextRequirementMustBeAvailableAtCallSite)
{
	const auto missing = ParseCode(
		"fn useLogger() : int with { logger: int } { return logger; }"
		"fn main() : int { return useLogger(); }");
	ASSERT_NE(missing, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(missing.get()), std::runtime_error);

	const auto present = ParseCode(
		"fn useLogger() : int with { logger: int } { return logger; }"
		"fn main() : int { var logger: int = 7; return useLogger(); }");
	ASSERT_NE(present, nullptr);
	EXPECT_NO_THROW(compiler.Compile(present.get()));
}

TEST_F(CompilerTest, SemanticRejectsRuntimeOnlyOperationsInsideComptime)
{
	const auto root = ParseCode("fn main() : void { var x = comptime { print 1; 0; }; }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}
