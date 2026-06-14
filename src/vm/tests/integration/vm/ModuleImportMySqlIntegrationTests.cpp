#include "../../support/CompilerVmIntegrationSupport.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

TEST(ModuleImportIntegrationTest, BuiltinMySqlModuleRejectsInvalidDsnBeforeConnecting)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.db.mysql as db;"
		"var conn = db.open(\"host=127.0.0.1;database=test\");"
		"print conn;");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("std.mysql DSN requires a user"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinMySqlFacadeModuleCompilesWithoutSourceFile)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.db.mysql as db;"
		"fn callback(conn) : int { return 7; };"
		"fn run() : void {"
		"  var conn = db.open(\"host=127.0.0.1;user=root;database=test\");"
		"  db.with_tx(conn, callback);"
		"};");

	EXPECT_NO_THROW((void)CompileProgram(root / "samples" / "main.aura"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinOutboxModuleCompilesWithoutSourceFile)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.db.mysql as db;"
		"import std.json as json;"
		"import std.mq.rabbitmq as mq;"
		"import std.outbox as outbox;"
		"fn enqueue(conn) : int {"
		"  return outbox.enqueue_json(conn, \"jobs\", \"jobs\", json.object([json.field(\"kind\", json.encode_string(\"email\"))]));"
		"};"
		"fn run() : void {"
		"  var conn = db.open(\"host=127.0.0.1;user=root;database=test\");"
		"  var rabbit = mq.open(\"amqp://guest:guest@127.0.0.1:5672/\");"
		"  outbox.ensure_schema(conn);"
		"  outbox.with_tx(conn, enqueue);"
		"  outbox.run_rabbitmq_relay(conn, rabbit, 10);"
		"};");

	EXPECT_NO_THROW((void)CompileProgram(root / "samples" / "main.aura"));
	std::filesystem::remove_all(root);
}
