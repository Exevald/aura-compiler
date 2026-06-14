#include "CompilerTest.h"

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
	Compiler& compiler = MakeCompiler(generator);

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
	Compiler& compiler = MakeCompiler(generator);

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
	Compiler& compiler = MakeCompiler(generator);

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
	Compiler& compiler = MakeCompiler(generator);

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
	Compiler& compiler = MakeCompiler(generator);

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
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"), std::runtime_error);
}
