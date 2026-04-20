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
	Compiler compiler(GrammarPath().string(), generator, lexer);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
	});
}

TEST_F(CompilerTest, BuiltinMemoryModuleCompilesWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.memory as rt;"
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

TEST_F(CompilerTest, BuiltinCoreModuleCompilesWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.core as std;"
		"var numbers = std.sort([3, 1, 2]);"
		"var hi = std.max(numbers[0], std.abs(-4));"
		"var size = std.len(\"aura\");"
		"print hi;"
		"print size;");

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

TEST_F(CompilerTest, BuiltinStdModulesCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.math as math;"
		"import std.array as arr;"
		"import std.text as text;"
		"var values = [3, 1, 2];"
		"arr.push(values, 4);"
		"var sorted = arr.sort(values);"
		"print math.clamp(math.max(sorted[0], 0), 0, 10);"
		"print arr.pop(sorted);"
		"print text.concat(\"au\", \"ra\");"
		"print text.contains(\"aura\", text.to_string(42));");

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

TEST_F(CompilerTest, BuiltinIoAndLogModulesCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.io as io;"
		"import std.log as log;"
		"io.print(\"a\", 1, true);"
		"io.println(\"b\", 2);"
		"io.printf(\"%s=%d\", \"x\", 42);"
		"log.Info(\"ready\", 7);"
		"log.Warn(\"warn\", false);");

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

TEST_F(CompilerTest, BuiltinIoReadAndCoreCastsCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.io as io;"
		"import std.core as core;"
		"var token = io.read();"
		"var line = io.readln();"
		"var number: int = core.to_int(token);"
		"var real: float = core.to_float(line);"
		"var flag: bool = core.to_bool(\"true\");"
		"io.println(core.to_string(number), real, flag);");

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

TEST_F(CompilerTest, BuiltinMemoryAllocAndFreeCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.memory as rt;"
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

TEST_F(CompilerTest, BuiltinMemoryAndSyncModulesCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.memory as mem;"
		"import std.sync as sync;"
		"var block = mem.alloc(8);"
		"var t = sync.spawn();"
		"var m = sync.mutex();"
		"sync.lock(t, m);"
		"mem.free(block);"
		"mem.assert_no_leaks();");

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
