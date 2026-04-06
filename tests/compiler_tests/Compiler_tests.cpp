#include "../../src/Compiler.h"
#include "../../src/ast/AST.h"
#include "../../src/bytecode/BytecodeGenerator.h"
#include "../../src/lexer/Lexer.h"
#include "../../src/parser/Parser.h"
#include "../../src/vm/core/OpCode.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
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

void WriteFile(const std::filesystem::path& path, const std::string& contents)
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream output(path);
	output << contents;
}

bool ChunkContainsOpcode(const VM::Execution::Chunk& chunk, const OpCode target)
{
	for (const auto byte : chunk.code)
	{
		if (byte == static_cast<uint8_t>(target))
		{
			return true;
		}
	}

	for (const auto& constant : chunk.constants)
	{
		if (std::holds_alternative<FunctionPtr>(constant))
		{
			const auto& fn = std::get<FunctionPtr>(constant);
			if (fn && ChunkContainsOpcode(*fn->chunk, target))
			{
				return true;
			}
		}
	}

	return false;
}

} // namespace

class CompilerTest : public ::testing::Test
{
protected:
	std::filesystem::path m_tempRoot;

	void SetUp() override
	{
		m_tempRoot = std::filesystem::temp_directory_path()
			/ ("aura_compiler_tests_" + std::to_string(::getpid()));
		std::filesystem::remove_all(m_tempRoot);
		std::filesystem::create_directories(m_tempRoot);
	}

	void TearDown() override
	{
		std::filesystem::remove_all(m_tempRoot);
	}

	static ASTNodePtr ParseCode(const std::string& source)
	{
		if (CachedGrammarText().empty())
		{
			return nullptr;
		}
		Lexer lexer(source);
		SLRParser parser(lexer, CachedGrammarText());
		if (!parser.Parse())
		{
			return nullptr;
		}
		return parser.GetRoot();
	}
};

TEST_F(CompilerTest, BytecodeSimpleAddition)
{
	auto root = ParseCode("var res = 10 + 20;");
	ASSERT_NE(root, nullptr)
		<< "Parsing failed! Remember: expressions in Aura must be inside declarations or functions.";

	BytecodeGenerator compiler;
	auto chunk = compiler.Compile(root.get());

	ASSERT_GE(chunk.code.size(), 6);

	EXPECT_EQ(static_cast<OpCode>(chunk.code[0]), OpCode::OP_CONSTANT);
	EXPECT_EQ(static_cast<OpCode>(chunk.code[2]), OpCode::OP_CONSTANT);

	EXPECT_EQ(std::get<long long>(chunk.constants[chunk.code[1]]), 10);
	EXPECT_EQ(std::get<long long>(chunk.constants[chunk.code[3]]), 20);

	EXPECT_EQ(static_cast<OpCode>(chunk.code[4]), OpCode::OP_ADD);
	EXPECT_EQ(static_cast<OpCode>(chunk.code[5]), OpCode::OP_SET_LOCAL);
}

TEST_F(CompilerTest, BytecodeVariableDeclaration)
{
	const auto root = ParseCode("var myVar = 100;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	auto chunk = compiler.Compile(root.get());

	bool foundSetLocal = false;
	for (size_t i = 0; i < chunk.code.size(); ++i)
	{
		if (static_cast<OpCode>(chunk.code[i]) == OpCode::OP_SET_LOCAL)
		{
			foundSetLocal = true;
			ASSERT_LT(i + 1, chunk.code.size());
			EXPECT_EQ(chunk.code[i + 1], 0);
		}
	}
	EXPECT_TRUE(foundSetLocal);
}

TEST_F(CompilerTest, BytecodeMathComplexity)
{
	const auto root = ParseCode("var x = 2 + 3 * 4;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	auto chunk = compiler.Compile(root.get());

	const std::vector expectedSequence = {
		OpCode::OP_CONSTANT,
		OpCode::OP_CONSTANT,
		OpCode::OP_CONSTANT,
		OpCode::OP_MULTIPLY,
		OpCode::OP_ADD,
		OpCode::OP_SET_LOCAL,
		OpCode::OP_RETURN
	};

	size_t seqIdx = 0;
	for (size_t i = 0; i < chunk.code.size(); ++i)
	{
		if (const auto op = static_cast<OpCode>(chunk.code[i]);
			op == expectedSequence[seqIdx])
		{
			seqIdx++;
			if (OpCodeHasOperand(op))
			{
				i++;
			}
		}
		if (seqIdx == expectedSequence.size())
		{
			break;
		}
	}

	EXPECT_EQ(seqIdx, expectedSequence.size());
}

TEST_F(CompilerTest, BytecodeIfElse)
{
	const auto root = ParseCode(
		"var x = 10;"
		"if (x > 5) {"
		"  x = 1;"
		"} else {"
		"  x = 0;"
		"}");

	ASSERT_NE(root, nullptr)
		<< "Parsing failed! 'if' must be inside a function and followed by ';'.";

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	bool hasJumpIfFalse = false;
	bool hasJump = false;

	for (const auto byte : chunk.code)
	{
		if (byte == static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE))
		{
			hasJumpIfFalse = true;
		}
		if (byte == static_cast<uint8_t>(OpCode::OP_JUMP))
		{
			hasJump = true;
		}
	}

	EXPECT_TRUE(hasJumpIfFalse) << "Missing OP_JUMP_IF_FALSE";
	EXPECT_TRUE(hasJump) << "Missing OP_JUMP (for else branch)";
}

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

TEST_F(CompilerTest, BytecodeClosureCapturesOuterVariable)
{
	const auto root = ParseCode(
		"fn make_adder(n : int) : int {"
		"  var add = fn(x : int) -> x + n;"
		"  return add(2);"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_CLOSURE));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_UPVALUE));
}

TEST_F(CompilerTest, BytecodeIterAdapterChain)
{
	const auto root = ParseCode(
		"fn walk(list: [int]) {"
		"  iter (item of list with [drop(1), take(2), reverse, filter(fn(v: int) -> v > 0), transform(fn(v: int) -> v + 10)]) {"
		"    print item;"
		"  }"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ITER_DROP));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ITER_TAKE));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ITER_REVERSE));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ITER_FILTER));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ITER_TRANSFORM));
}

TEST_F(CompilerTest, BytecodeUnaryAndBooleanLiterals)
{
	const auto root = ParseCode("var x = -5; var flag = not false; var empty = null;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_NEGATE));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_NOT));
}

TEST_F(CompilerTest, BytecodePointerDereferenceGetAndSet)
{
	const auto root = ParseCode(
		"type Score = int;"
		"var ptr_val: ptr<int>;"
		"unsafe { *(ptr_val) = 5; print *(ptr_val); }");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DEREF_SET));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DEREF_GET));
}

TEST_F(CompilerTest, BytecodeAddressOfLocalGlobalAndMember)
{
	const auto root = ParseCode(
		"struct Point { x: int; }"
		"fn id(v: int) : int { return v; }"
		"fn demo() {"
		"  unsafe {"
		"    var x: int = 1;"
		"    var p: ptr<int> = &x;"
		"    var fp = &id;"
		"    var point = Point(2);"
		"    var mp: ptr<int> = &point.x;"
		"    var arr = [10, 20];"
		"    var ap: ptr<int> = &arr[1];"
		"    print *p;"
		"  }"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ADDR_LOCAL));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ADDR_GLOBAL));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ADDR_MEMBER));
}

TEST_F(CompilerTest, BytecodeAddressOfCapturedVariable)
{
	const auto root = ParseCode(
		"fn makeCounter() {"
		"  var x: int = 41;"
		"  var next = fn() -> {"
		"    unsafe {"
		"      var px: ptr<int> = &x;"
		"      *px = *px + 1;"
		"      return x;"
		"    }"
		"  };"
		"  return next;"
		"}"
		"var counter = makeCounter();"
		"print counter();");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ADDR_UPVALUE));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DEREF_GET));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DEREF_SET));
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

TEST_F(CompilerTest, BytecodeStructConstructorAndFieldAccess)
{
	const auto root = ParseCode(
		"struct Point { x: int; y: int; }"
		"var p: Point = Point(10, 20);"
		"print p.x;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_STRUCT));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_GET));
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

TEST_F(CompilerTest, BytecodeStructFieldAssignment)
{
	const auto root = ParseCode(
		"struct Point { x: int; y: int; }"
		"var p = Point(10, 20);"
		"p.x = 42;"
		"print p.x;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_SET));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_GET));
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

TEST_F(CompilerTest, BytecodeNestedStructFieldAccessAndAssignment)
{
	const auto root = ParseCode(
		"struct Inner { x: int; }"
		"struct Outer { inner: Inner; }"
		"var outer = Outer(Inner(10));"
		"outer.inner.x = 42;"
		"print outer.inner.x;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_STRUCT));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_SET));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_GET));
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

TEST_F(CompilerTest, BytecodeStructWholeFieldAssignment)
{
	const auto root = ParseCode(
		"struct Inner { x: int; }"
		"struct Outer { inner: Inner; }"
		"var outer = Outer(Inner(10));"
		"outer.inner = Inner(42);"
		"print outer.inner.x;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_STRUCT));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_SET));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_GET));
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
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
	});
}

TEST_F(CompilerTest, BuiltinRuntimeModuleCompilesWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.runtime as rt;"
		"print rt.active_allocations();"
		"print rt.is_send([1, 2, 3]);");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_CALL));
	});
}

TEST_F(CompilerTest, BuiltinRuntimeAllocAndFreeCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.runtime as rt;"
		"var mem = rt.alloc(8);"
		"rt.free(mem);"
		"rt.assert_no_leaks();");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_CALL));
	});
}

TEST_F(CompilerTest, BuiltinSyncModuleCompilesWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.sync as sync;"
		"var t1 = sync.spawn();"
		"var t2 = sync.spawn();"
		"var m1 = sync.mutex();"
		"var m2 = sync.mutex();"
		"print sync.lock(t1, m1);"
		"print sync.lock(t2, m2);"
		"print sync.lock(t1, m2);"
		"print sync.would_deadlock(t2, m1);");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_CALL));
	});
}

TEST_F(CompilerTest, BuiltinSyncModuleCompiletimeDeadlockRejected)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.sync as sync;"
		"var t1 = sync.spawn();"
		"var t2 = sync.spawn();"
		"var m1 = sync.mutex();"
		"var m2 = sync.mutex();"
		"sync.lock(t1, m1);"
		"sync.lock(t2, m2);"
		"sync.lock(t1, m2);"
		"sync.lock(t2, m1);");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"), std::runtime_error);
}

TEST_F(CompilerTest, BuiltinSyncModuleCompiletimeJoinCycleRejected)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.sync as sync;"
		"var t1 = sync.spawn();"
		"var t2 = sync.spawn();"
		"sync.join(t1, t2);"
		"sync.join(t2, t1);");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"), std::runtime_error);
}

TEST_F(CompilerTest, BuiltinSyncModuleCompiletimeUnlockNonOwnerRejected)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.sync as sync;"
		"var t1 = sync.spawn();"
		"var t2 = sync.spawn();"
		"var m = sync.mutex();"
		"sync.lock(t1, m);"
		"sync.unlock(t2, m);");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"), std::runtime_error);
}

TEST_F(CompilerTest, BuiltinSyncModuleCompiletimeDeadlockRejectedAcrossFunctions)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.sync as sync;"
		"fn takeFirst(thread, left, right) : void {"
		"  sync.lock(thread, left);"
		"  sync.lock(thread, right);"
		"}"
		"var t1 = sync.spawn();"
		"var t2 = sync.spawn();"
		"var m1 = sync.mutex();"
		"var m2 = sync.mutex();"
		"takeFirst(t1, m1, m2);"
		"takeFirst(t2, m2, m1);");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"), std::runtime_error);
}

TEST_F(CompilerTest, BuiltinSyncModuleCompiletimeDeadlockRejectedAcrossLambdaCalls)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.sync as sync;"
		"var lockPair = fn(thread, left, right) -> {"
		"  sync.lock(thread, left);"
		"  sync.lock(thread, right);"
		"};"
		"var t1 = sync.spawn();"
		"var t2 = sync.spawn();"
		"var m1 = sync.mutex();"
		"var m2 = sync.mutex();"
		"lockPair(t1, m1, m2);"
		"lockPair(t2, m2, m1);");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"), std::runtime_error);
}

TEST_F(CompilerTest, BuiltinSyncModuleCompiletimeDeadlockRejectedAcrossImportedModules)
{
	WriteFile(
		m_tempRoot / "samples" / "helpers.aura",
		"module samples.helpers;"
		"import std.sync as sync;"
		"export fn lock_pair(thread, left, right) : void {"
		"  sync.lock(thread, left);"
		"  sync.lock(thread, right);"
		"};");
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import samples.helpers as helpers;"
		"import std.sync as sync;"
		"var t1 = sync.spawn();"
		"var t2 = sync.spawn();"
		"var m1 = sync.mutex();"
		"var m2 = sync.mutex();"
		"helpers.lock_pair(t1, m1, m2);"
		"helpers.lock_pair(t2, m2, m1);");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"), std::runtime_error);
}

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
	Compiler compiler(GrammarPath().string(), generator, lexer);

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
	Compiler compiler(GrammarPath().string(), generator, lexer);

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
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_STRUCT));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_SET));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_MEMBER_GET));
	});
}

TEST_F(CompilerTest, BytecodeEnumConstructorAndTagAccess)
{
	const auto root = ParseCode(
		"enum Option { None | Some(int) }"
		"var value = Some(42);"
		"print value.tag;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_ENUM));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_ENUM_TAG));
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
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_ENUM));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_ENUM_TAG));
	});
}

TEST_F(CompilerTest, BytecodeEnumArgumentAccess)
{
	const auto root = ParseCode(
		"enum Option { None | Some(int) }"
		"var value = Some(42);"
		"print value[0];");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_ENUM));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_ENUM_ARG));
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
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_ENUM));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_ENUM_ARG));
	});
}

TEST_F(CompilerTest, ModuleDeclarationMismatchReportsClearError)
{
	WriteFile(
		m_tempRoot / "samples" / "wrong_name.aura",
		"module samples.not_wrong_name;"
		"print 1;");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler compiler(GrammarPath().string(), generator, lexer);

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

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler compiler(GrammarPath().string(), generator, lexer);

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
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_NO_THROW((void)compiler.CompileFileToChunk(mainPath));
	std::filesystem::remove(importedPath);
	EXPECT_NO_THROW((void)compiler.CompileFileToChunk(mainPath));
}
