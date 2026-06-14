#include "CompilerTest.h"

TEST_F(CompilerTest, BuiltinMemoryModuleIsInternalOnly)
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
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"), std::runtime_error);
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
	Compiler& compiler = MakeCompiler(generator);

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
		"import std.map as maps;"
		"import std.text as text;"
		"var values = [3, 1, 2];"
		"arr.push(values, 4);"
		"arr.remove_at(values, 1);"
		"var sorted = arr.sort(values);"
		"var lookup: map[string]int = map[string]int{};"
		"lookup[\"jobs\"] = 3;"
		"print math.clamp(math.max(sorted[0], 0), 0, 10);"
		"print arr.pop(sorted);"
		"print lookup[\"jobs\"];"
		"print maps.has(lookup, \"jobs\");"
		"print text.concat(\"au\", \"ra\");"
		"print text.contains(\"aura\", text.to_string(42));");

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

TEST_F(CompilerTest, BuiltinMapLiteralAndHelpersCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.map as maps;"
		"var labels: map[int]string = map[int]string{1: \"open\", 2: \"closed\"};"
		"print labels[1];"
		"print maps.len(labels);"
		"print maps.delete(labels, 2);");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_MAP));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_INDEX_GET));
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
	Compiler& compiler = MakeCompiler(generator);

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
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_NO_THROW({
		auto chunk = compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura");
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_GET_MODULE_MEMBER));
		EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_CALL));
	});
}

TEST_F(CompilerTest, BuiltinIoReadFileCompilesWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.io as io;"
		"var sql = io.read_file(\"data/001.sql\");"
		"io.println(sql);");

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

TEST_F(CompilerTest, BuiltinInternalNativeModuleImportsAreRejected)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.task_native as native;"
		"print native.is_done(native.go_call(fn() -> { return; }));");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"), std::runtime_error);
}

TEST_F(CompilerTest, BuiltinLowLevelMySqlModuleIsInternalOnly)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.mysql as mysql;"
		"print mysql.open(\"host=127.0.0.1;user=root;database=test\");");

	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler& compiler = MakeCompiler(generator);

	EXPECT_THROW((void)compiler.CompileFileToChunk(m_tempRoot / "samples" / "main.aura"), std::runtime_error);
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
		"print sync.unlock(t1, m2);"
		"print sync.finish(t1);");

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

TEST_F(CompilerTest, BuiltinEnvAndTimeModulesCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.env as env;"
		"import std.time as time;"
		"var fallback = env.get_or(\"__AURA_MISSING_ENV__\", \"fallback\");"
		"var present = env.has(\"__AURA_MISSING_ENV__\");"
		"var started = time.now_millis();"
		"var tick = time.monotonic_nanos();"
		"time.sleep(0);"
		"print fallback;"
		"print present;"
		"print started;"
		"print tick;");

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

TEST_F(CompilerTest, BuiltinConfigAndBackoffModulesCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.config as config;"
		"import std.backoff as backoff;"
		"var policy = backoff.exponential(10, 2, 100);"
		"print config.get_int(\"PORT\", 8080);"
		"print config.get_bool(\"DEBUG\", false);"
		"print config.get_duration_ms(\"TIMEOUT_MS\", 250);"
		"print backoff.delay(policy, 3);");

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

TEST_F(CompilerTest, BuiltinUuidAndRabbitMqModulesCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.uuid as uuid;"
		"import std.mq.rabbitmq as mq;"
		"var request_id = uuid.new_v4();"
		"var conn = mq.open(\"amqp://guest:guest@127.0.0.1:5672/\");"
		"print request_id;"
		"print mq.error(conn);"
		"mq.close(conn);");

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

TEST_F(CompilerTest, BuiltinOutboxModuleCompilesWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.db.mysql as db;"
		"import std.json as json;"
		"import std.mq.rabbitmq as mq;"
		"import std.outbox as outbox;"
		"fn callback(conn) : int {"
		"  return outbox.enqueue_json(conn, \"jobs\", \"jobs\", json.object([json.field(\"kind\", json.encode_string(\"email\"))]));"
		"};"
		"var conn = db.open(\"host=127.0.0.1;user=root;database=test\");"
		"var rabbit = mq.open(\"amqp://guest:guest@127.0.0.1:5672/\");"
		"print outbox.ensure_schema(conn);"
		"print outbox.with_tx(conn, callback);"
		"print outbox.run_rabbitmq_relay(conn, rabbit, 10);"
		"mq.close(rabbit);"
		"db.close(conn);");

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

TEST_F(CompilerTest, BuiltinChannelAndCtxModulesCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.channel as ch;"
		"import std.option as option;"
		"import std.context as ctx;"
		"var c: ch.Channel<int> = ch.make(1);"
		"print ch.send(c, 42);"
		"var item = ch.recv(c);"
		"print item.tag;"
		"print item[0];"
		"print ch.close(c);"
		"var none_value = ch.recv(c);"
		"print none_value.tag;"
		"var root = ctx.background();"
		"var child = ctx.with_cancel(root);"
		"print ctx.is_cancelled(child);"
		"print ctx.cancel(root);");

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

TEST_F(CompilerTest, BuiltinConcurrentModulesCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.task as task;"
		"import std.log as log;"
		"import std.concurrent.waitgroup as wg;"
		"import std.concurrent.errorgroup as eg;"
		"import std.context as ctx;"
		"fn calc() : int { return 42; }"
		"fn publish() : void { return; }"
		"fn fail_fast() : void { log.Fatal(\"eg-fail\"); }"
		"var work = task.spawn(calc);"
		"var joined = task.join_result(work);"
		"print joined.tag;"
		"print joined[0];"
		"var group = wg.new();"
		"var task1 = wg.spawn(group, publish);"
		"wg.wait(group);"
		"var errors = eg.with_context(ctx.background());"
		"var task2 = eg.spawn(errors, fail_fast);"
		"print eg.count(errors);"
		"var err = eg.wait(errors);"
		"print err.tag;"
		"print err[0];"
		"print ctx.is_cancelled(eg.group_context(errors));");

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

TEST_F(CompilerTest, BuiltinJsonModuleCompilesWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.json as json;"
		"var body = json.object(["
		"  json.field(\"id\", json.encode_number(42)),"
		"  json.field(\"name\", json.encode_string(\"aura\")),"
		"  json.field(\"active\", json.encode_bool(true)),"
		"  json.field(\"tags\", json.array([json.encode_string(\"svc\"), json.encode_string(\"bench\")]))"
		"]);"
		"var compact = json.compact(json.object([json.field(\"ok\", json.array([json.encode_number(1), json.encode_number(2)]))]));"
		"print body;"
		"print compact;"
		"print json.get_int(body, \"id\");"
		"print json.get_string(body, \"name\");"
		"print json.get_bool(body, \"active\");"
		"print json.is_valid(body);");

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

TEST_F(CompilerTest, BuiltinHttpServerNativeFacadeCompilesWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.http.server as server;"
		"import std.json as json;"
		"fn route(request: [string]) : string {"
		"  if (server.route_equals(request, \"GET\", \"/health\")) {"
		"    return server.ok_json(json.object([json.field(\"ok\", json.encode_bool(true))]));"
		"  }"
		"  if (server.path_has_prefix(request, \"/items/\")) {"
		"    return server.created_json(json.object(["
		"      json.field(\"id\", json.encode_string(server.path_suffix_after_prefix(request, \"/items/\")))"
		"    ]));"
		"  }"
		"  return server.not_found(\"missing\");"
		"}"
		"print route([\"GET\", \"/items/42\", \"\"]);");

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

TEST_F(CompilerTest, BuiltinNetHttpAndTextModulesCompileWithoutSourceFile)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.net as net;"
		"import std.http.raw as http;"
		"import std.json as json;"
		"import std.text as text;"
		"var listener = net.listen(\"127.0.0.1\", 8080);"
		"var conn = net.accept(listener);"
		"net.set_nodelay(conn, true);"
		"net.set_read_timeout(conn, 1000);"
		"net.set_write_timeout(conn, 1000);"
		"var request = http.read_request(conn);"
		"var parts = text.split(request[1], \"/\");"
		"var response = http.ok_json(json.object([json.field(\"id\", json.encode_string(parts[2]))]));"
		"net.write(conn, response);"
		"net.close(conn);"
		"net.close(listener);");

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

TEST_F(CompilerTest, BuiltinHttpServerFacadeCompilesWithoutFilesystemSource)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.http.server as server;"
		"import std.context as ctx;"
		"import std.json as json;"
		"var route_fn = fn(request: [string]) -> {"
		"  return server.ok_json(json.object([json.field(\"item\", json.encode_string(server.segment(request, 2)))]));"
		"};"
		"fn route(request: [string]) : string {"
		"  var parts = server.path_segments(request);"
		"  return server.ok_json(json.object([json.field(\"item\", json.encode_string(parts[2]))]));"
		"}"
		"var parsed = [\"GET\", \"/items/42\", \"\"];"
		"print server.method(parsed);"
		"print server.path(parsed);"
		"print route(parsed);"
		"server.serve_once(\"127.0.0.1\", 8080, route_fn);"
		"server.serve_n(\"127.0.0.1\", 8081, 2, route_fn);"
		"server.listen_and_serve(\"127.0.0.1\", 8082, route_fn);"
		"server.listen_and_serve_with(\"127.0.0.1\", 8083, route_fn, ctx.background(), 250, 500);");

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

TEST_F(CompilerTest, BuiltinAuraFacadeModuleCompilesWithoutFilesystemSource)
{
	WriteFile(
		m_tempRoot / "samples" / "main.aura",
		"module samples.main;"
		"import std.service as svc;"
		"import std.context as ctx;"
		"var items = svc.store_new();"
		"var shutdown = svc.shutdown_context();"
		"print ctx.is_cancelled(shutdown);"
		"print svc.host_or(\"127.0.0.1\");"
		"print svc.port_or(8080);"
		"print svc.store_set(items, \"42\", \"{\\\"ok\\\":true}\");"
		"print svc.store_get(items, \"42\");"
		"print svc.store_len(items);"
		"print svc.store_delete(items, \"42\");");

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
