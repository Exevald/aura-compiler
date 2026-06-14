#include "../../support/CompilerVmIntegrationSupport.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <thread>

namespace
{

std::string RunShellCapture(const std::string& command)
{
	std::array<char, 512> buffer{};
	std::string output;

	FILE* pipe = ::popen(command.c_str(), "r");
	if (pipe == nullptr)
	{
		return output;
	}

	while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
	{
		output += buffer.data();
	}
	(void)::pclose(pipe);
	return output;
}

bool RunShellQuiet(const std::string& command)
{
	return std::system(command.c_str()) == 0;
}

struct MySqlDockerServer
{
	std::string containerName = "aura-mysql-it-" + std::to_string(::getpid());
	int port = 33306 + getpid() % 1000;
	bool running = false;

	~MySqlDockerServer()
	{
		if (running)
		{
			(void)RunShellQuiet("docker rm -f " + containerName + " >/dev/null 2>&1");
		}
	}

	std::string Dsn() const
	{
		return "host=127.0.0.1;port=" + std::to_string(port)
			+ ";user=aura;password=aura_pass;database=aura_test;charset=utf8mb4";
	}

	void StartOrSkip()
	{
		if (!RunShellQuiet("docker info >/dev/null 2>&1"))
		{
			GTEST_SKIP() << "docker is not available for live MySQL integration tests";
		}

		if (!RunShellQuiet("docker image inspect mysql:8.0.36 >/dev/null 2>&1"))
		{
			if (!RunShellQuiet("docker pull mysql:8.0.36 >/dev/null 2>&1"))
			{
				GTEST_SKIP() << "could not pull mysql:8.0.36 for live integration tests";
			}
		}

		(void)RunShellQuiet("docker rm -f " + containerName + " >/dev/null 2>&1");
		const std::string runCommand = "docker run -d --name " + containerName
			+ " -e MYSQL_ROOT_PASSWORD=aura_root"
			+ " -e MYSQL_DATABASE=aura_test"
			+ " -e MYSQL_USER=aura"
			+ " -e MYSQL_PASSWORD=aura_pass"
			+ " -p 127.0.0.1:" + std::to_string(port) + ":3306"
			+ " mysql:8.0.36 --default-authentication-plugin=mysql_native_password --mysqlx=0 >/dev/null";
		if (!RunShellQuiet(runCommand))
		{
			GTEST_SKIP() << "could not start mysql docker container";
		}

		running = true;
		for (int attempt = 0; attempt < 90; ++attempt)
		{
			if (RunShellQuiet(
					std::string("/opt/homebrew/opt/mysql-client/bin/mysqladmin --protocol=tcp")
					+ " -h127.0.0.1 -P" + std::to_string(port)
					+ " -uaura -paura_pass ping --silent >/dev/null 2>&1"))
			{
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}

		const auto logs = RunShellCapture("docker logs " + containerName + " 2>&1");
		FAIL() << "mysql container did not become ready\n"
			   << logs;
	}
};

} // namespace

TEST(ModuleImportEndToEndTest, BuiltinMySqlModuleExecutesAgainstLiveDatabase)
{
	MySqlDockerServer server;
	server.StartOrSkip();

	const auto root = MakeTempRoot();
	const auto dsn = server.Dsn();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.db.mysql as db;"
		"fn run() : void {"
		"  var conn = db.open(\""
			+ dsn + "\");"
					"  db.exec_stmt(conn, \"drop table if exists aura_items\", []);"
					"  db.exec_stmt(conn, \"create table aura_items(id bigint primary key, name varchar(255), score double, note varchar(255) null)\", []);"
					"  var insert_result = db.exec_stmt(conn, \"insert into aura_items(id, name, score, note) values(?, ?, ?, ?)\", [1, \"alpha\", 3.5, null]);"
					"  print db.affected_rows(insert_result);"
					"  var row = db.query_one(conn, \"select id, name, score, note from aura_items where id = ?\", [1]);"
					"  print db.get(row, \"id\");"
					"  print db.get(row, \"name\");"
					"  print db.get(row, \"score\");"
					"  print db.is_null(db.get(row, \"note\"));"
					"  db.close(conn);"
					"};"
					"run();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("1"));
	EXPECT_THAT(output, ::testing::HasSubstr("alpha"));
	EXPECT_THAT(output, ::testing::HasSubstr("3.5"));
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportEndToEndTest, BuiltinMySqlPoolTransactionsAndFacadeExecuteAgainstLiveDatabase)
{
	MySqlDockerServer server;
	server.StartOrSkip();

	const auto root = MakeTempRoot();
	const auto dsn = server.Dsn();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.db.mysql as db;"
		"fn count_rows(conn) : int {"
		"  var row = db.query_one(conn, \"select count(*) as total from aura_tx\", []);"
		"  return db.get(row, \"total\");"
		"};"
		"fn run() : void {"
		"  var conn = db.open(\""
			+ dsn + "\");"
					"  var pool = db.open_pool(\""
			+ dsn + "\", 2);"
					"  db.exec_stmt(conn, \"drop table if exists aura_tx\", []);"
					"  db.exec_stmt(conn, \"create table aura_tx(id bigint primary key, name varchar(255))\", []);"
					"  db.begin(conn);"
					"  var first = db.exec_stmt(conn, \"insert into aura_tx(id, name) values(?, ?)\", [1, \"committed\"]);"
					"  print db.affected_rows(first);"
					"  db.commit(conn);"
					"  db.begin(conn);"
					"  db.exec_stmt(conn, \"insert into aura_tx(id, name) values(?, ?)\", [2, \"rolled\"]);"
					"  db.rollback(conn);"
					"  var row = db.query_one(pool, \"select name from aura_tx where id = ?\", [1]);"
					"  print db.get(row, \"name\");"
					"  print count_rows(pool);"
					"  db.close(pool);"
					"  db.close(conn);"
					"};"
					"run();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("committed"));
	EXPECT_THAT(output, ::testing::Not(::testing::HasSubstr("rolled")));
	EXPECT_THAT(output, ::testing::HasSubstr("1"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportEndToEndTest, BuiltinOutboxEnqueuesRowsInsideTransactionAgainstLiveDatabase)
{
	MySqlDockerServer server;
	server.StartOrSkip();

	const auto root = MakeTempRoot();
	const auto dsn = server.Dsn();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.db.mysql as db;"
		"import std.json as json;"
		"import std.outbox as outbox;"
		"fn enqueue(conn) : int {"
		"  db.exec_stmt(conn, \"insert into aura_orders(id, status) values(?, ?)\", [1, \"created\"]);"
		"  return outbox.enqueue_json(conn, \"jobs\", \"jobs\", json.object([json.field(\"kind\", json.encode_string(\"order.created\"))]));"
		"};"
		"fn run() : void {"
		"  var conn = db.open(\"" + dsn + "\");"
		"  db.exec_stmt(conn, \"drop table if exists aura_orders\", []);"
		"  db.exec_stmt(conn, \"drop table if exists aura_outbox\", []);"
		"  db.exec_stmt(conn, \"create table aura_orders(id bigint primary key, status varchar(64) not null)\", []);"
		"  print outbox.ensure_schema(conn);"
		"  print outbox.with_tx(conn, enqueue) > 0;"
		"  var order_row = db.query_one(conn, \"select status from aura_orders where id = ?\", [1]);"
		"  var outbox_row = db.query_one(conn, \"select topic, status, attempts from aura_outbox where id = 1\", []);"
		"  print db.get(order_row, \"status\");"
		"  print db.get(outbox_row, \"topic\");"
		"  print db.get(outbox_row, \"status\");"
		"  print db.get(outbox_row, \"attempts\");"
		"  db.close(conn);"
		"};"
		"run();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	EXPECT_THAT(output, ::testing::HasSubstr("created"));
	EXPECT_THAT(output, ::testing::HasSubstr("jobs"));
	EXPECT_THAT(output, ::testing::HasSubstr("pending"));
	EXPECT_THAT(output, ::testing::HasSubstr("0"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportEndToEndTest, BuiltinOutboxMarksFailedRelayAttemptsAgainstLiveDatabase)
{
	MySqlDockerServer server;
	server.StartOrSkip();

	const auto root = MakeTempRoot();
	const auto dsn = server.Dsn();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.db.mysql as db;"
		"import std.json as json;"
		"import std.mq.rabbitmq as mq;"
		"import std.outbox as outbox;"
		"fn run() : void {"
		"  var conn = db.open(\"" + dsn + "\");"
		"  var rabbit = mq.open(\"amqp://guest:guest@127.0.0.1:1/\");"
		"  db.exec_stmt(conn, \"drop table if exists aura_outbox\", []);"
		"  print outbox.ensure_schema(conn);"
		"  print outbox.enqueue_json(conn, \"jobs\", \"jobs\", json.object([json.field(\"kind\", json.encode_string(\"email\"))])) > 0;"
		"  print outbox.run_rabbitmq_relay(conn, rabbit, 10);"
		"  var row = db.query_one(conn, \"select status, attempts, last_error from aura_outbox where id = 1\", []);"
		"  print db.get(row, \"status\");"
		"  print db.get(row, \"attempts\");"
		"  print db.get(row, \"last_error\");"
		"  mq.close(rabbit);"
		"  db.close(conn);"
		"};"
		"run();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	EXPECT_THAT(output, ::testing::HasSubstr("failed"));
	EXPECT_THAT(output, ::testing::HasSubstr("1"));
	EXPECT_THAT(output, ::testing::HasSubstr("std.mq.rabbitmq.publish cannot use a closed connection"));
	std::filesystem::remove_all(root);
}
