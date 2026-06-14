#include "CompilerTest.h"

TEST_F(CompilerTest, BuiltinMySqlModulesCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.db.mysql as db;"
		"fn callback(conn) : int { return 1; };"
		"fn run() : void {"
		"  var conn = db.open(\"host=127.0.0.1;user=root;database=test\");"
		"  var pool = db.open_pool(\"host=127.0.0.1;user=root;database=test\", 2);"
		"  var result = db.exec_stmt(conn, \"select ?\", [42]);"
		"  print db.affected_rows(result);"
		"  print db.error(pool);"
		"  db.with_tx(conn, callback);"
		"  db.close(pool);"
		"  db.close(conn);"
		"};");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_CALL));
	});
}
