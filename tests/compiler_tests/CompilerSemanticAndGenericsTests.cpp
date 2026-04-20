#include "CompilerTest.h"

TEST_F(CompilerTest, SemanticUndefinedVariable)
{
	const auto root = ParseCode("var x = y + 1;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticArithmeticTypeMismatch)
{
	const auto root = ParseCode("var x = \"hello\" - 1;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticIndexingNonArray)
{
	const auto root = ParseCode("var x = 1; print x[0];");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticIndexingStringType)
{
	const auto root = ParseCode("var list = [1, 2]; var y = list[\"a\"];");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticExplicitTypeMismatch)
{
	const auto root = ParseCode("var x : int = \"hello\";");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticConstAssignmentRejected)
{
	const auto root = ParseCode("const x: int = 1; x = 2;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticTypeAliasResolvesExplicitType)
{
	const auto root = ParseCode(
		"type Score = int;"
		"const baseline: Score = 7;"
		"var current: Score = baseline;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, GenericTypeAliasResolvesExplicitType)
{
	const auto root = ParseCode(
		"type Box<T> = [T];"
		"var values: Box<int> = [1, 2, 3];");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, GenericFunctionInfersArgumentAndReturnType)
{
	const auto root = ParseCode(
		"fn identity<T>(value: T) : T { return value; }"
		"var answer: int = identity(42);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, GenericConstraintAllowsNumericOperations)
{
	const auto root = ParseCode(
		"fn doubleValue<T: int>(value: T) : int { return value + value; }"
		"print doubleValue(21);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ADD));
}

TEST_F(CompilerTest, GenericConstraintRejectsWrongArgumentType)
{
	const auto root = ParseCode(
		"fn doubleValue<T: int>(value: T) : int { return value + value; }"
		"print doubleValue(\"bad\");");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

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

TEST_F(CompilerTest, ModuleSatisfiesInterfaceCompile)
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
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
	});
}

TEST_F(CompilerTest, SingleMethodInterfaceFunctionDispatchCompile)
{
	const auto root = ParseCode(
		"interface Action { fn run(v: int) : int; }"
		"var action: Action = fn(v: int) -> v + 1;"
		"print action.run(41);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_CALL));
	EXPECT_FALSE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
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
	Compiler compiler(GrammarPath().string(), generator, lexer);

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
	Compiler compiler(GrammarPath().string(), generator, lexer);

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
	Compiler compiler(GrammarPath().string(), generator, lexer);

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
	Compiler compiler(GrammarPath().string(), generator, lexer);

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

TEST_F(CompilerTest, SemanticFunctionArityMismatch)
{
	const auto root = ParseCode(
		"fn add(a, b) { return a + b; }"
		"var x = add(1);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticArgumentTypeMismatch)
{
	const auto root = ParseCode(
		"fn square(n : int) { return n * n; }"
		"var res = square(\"not_a_number\");");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticRefArgumentAcceptsAssignableValue)
{
	const auto root = ParseCode(
		"fn inc(n: ref<int>) : void { *n = *n + 1; }"
		"var value: int = 1;"
		"inc(value);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticRefArgumentRejectsNonAssignableExpression)
{
	const auto root = ParseCode(
		"fn inc(n: ref<int>) : void { *n = *n + 1; }"
		"var value: int = 1;"
		"inc(value + 1);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticRefArgumentRejectsConstValue)
{
	const auto root = ParseCode(
		"fn inc(n: ref<int>) : void { *n = *n + 1; }"
		"const value: int = 1;"
		"inc(value);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticRefArgumentRejectsWrongType)
{
	const auto root = ParseCode(
		"fn inc(n: ref<int>) : void { *n = *n + 1; }"
		"var value: string = \"1\";"
		"inc(value);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticReturnTypeMismatch)
{
	const auto root = ParseCode(
		"fn get_num() : int { return \"oops\"; }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticValidTypeCoercion)
{
	const auto root = ParseCode(
		"fn accept_float(f : float) { return f; }"
		"var x = accept_float(42);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticCallNonFunction)
{
	const auto root = ParseCode(
		"var x = 10;"
		"x();");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticComplexRecursiveInference)
{
	const auto root = ParseCode(
		"fn get_val(n : int) : int { return n + 1; }"
		"var x : int = get_val(10);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW(compiler.Compile(root.get()));
}

TEST_F(CompilerTest, SemanticAddressOfConstRejected)
{
	const auto root = ParseCode("const x: int = 1; unsafe { var p: ptr<int> = &x; }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticPointerOpsRequireUnsafeBlock)
{
	const auto root = ParseCode(
		"var ptr_val: ptr<int>;"
		"print *ptr_val;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_THROW(compiler.Compile(root.get()), std::runtime_error);
}

TEST_F(CompilerTest, SemanticSupportsVmBinaryOperators)
{
	const auto root = ParseCode(
		"var a = 7 div 2;"
		"var b = 7 mod 2;"
		"var c = 2 <= 3;"
		"var d = 3 >= 2;"
		"var e = true and false;"
		"var f = true or false;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	EXPECT_NO_THROW({
		const auto chunk = compiler.Compile(root.get());
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DIV));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MOD));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_LESS_EQUAL));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GREATER_EQUAL));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_AND));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_OR));
	});
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
