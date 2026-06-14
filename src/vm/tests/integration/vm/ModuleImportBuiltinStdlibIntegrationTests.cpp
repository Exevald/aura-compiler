#include "../../support/CompilerVmIntegrationSupport.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <array>
#include <chrono>
#include <future>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace
{

int ReserveLoopbackPort()
{
	const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	EXPECT_GE(fd, 0);

	sockaddr_in addr{};
#ifdef __APPLE__
	addr.sin_len = sizeof(addr);
#endif
	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
	EXPECT_EQ(::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

	socklen_t size = sizeof(addr);
	EXPECT_EQ(::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &size), 0);
	const int port = ntohs(addr.sin_port);
	::close(fd);
	return port;
}

std::string HttpGetWithRetry(const int port, const std::string& path)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < deadline)
	{
		const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0)
		{
			break;
		}

#ifdef SO_NOSIGPIPE
		const int noSigPipe = 1;
		::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, sizeof(noSigPipe));
#endif

		sockaddr_in addr{};
#ifdef __APPLE__
		addr.sin_len = sizeof(addr);
#endif
		addr.sin_family = AF_INET;
		addr.sin_port = htons(static_cast<uint16_t>(port));
		::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
		if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
		{
			::close(fd);
			std::this_thread::sleep_for(std::chrono::milliseconds(25));
			continue;
		}

		const std::string request =
			"GET " + path + " HTTP/1.1\r\n"
			"Host: 127.0.0.1\r\n"
			"Connection: close\r\n\r\n";
		if (::send(fd, request.data(), request.size(), 0) <= 0)
		{
			::close(fd);
			return "";
		}

		std::string response;
		std::array<char, 4096> buffer{};
		while (true)
		{
			const ssize_t received = ::recv(fd, buffer.data(), buffer.size(), 0);
			if (received == 0)
			{
				break;
			}
			if (received < 0)
			{
				response.clear();
				break;
			}
			response.append(buffer.data(), static_cast<size_t>(received));
		}

		::close(fd);
		if (!response.empty())
		{
			return response;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}

	return "";
}

} // namespace

TEST(ModuleImportIntegrationTest, BuiltinJsonModuleExecutesThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.json as json;"
		"var payload = json.object(["
		"  json.field(\"id\", json.encode_number(42)),"
		"  json.field(\"name\", json.encode_string(\"aura\")),"
		"  json.field(\"ready\", json.encode_bool(true)),"
		"  json.field(\"tags\", json.array([json.encode_string(\"bench\"), json.encode_string(\"svc\")]))"
		"]);"
		"var compact = json.compact("
		"json.object([json.field(\"ok\", json.array([json.encode_number(1), json.encode_number(2), json.encode_number(3)]))]));"
		"print payload;"
		"print json.get_int(payload, \"id\");"
		"print json.get_string(payload, \"name\");"
		"print json.get_bool(payload, \"ready\");"
		"print compact;"
		"print json.is_valid(payload);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("{\"id\":42,\"name\":\"aura\",\"ready\":true,\"tags\":[\"bench\",\"svc\"]}"));
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	EXPECT_THAT(output, ::testing::HasSubstr("aura"));
	EXPECT_THAT(output, ::testing::HasSubstr("{\"ok\":[1,2,3]}"));
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinJsonHasKeyExecutesThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.json as json;"
		"var payload = json.object(["
		"  json.field(\"sub\", json.encode_string(\"user-1\")),"
		"  json.field(\"exp\", json.encode_number(123))"
		"]);"
		"print json.has_key(payload, \"sub\");"
		"print json.has_key(payload, \"missing\");");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	EXPECT_THAT(output, ::testing::HasSubstr("false"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinBase64UrlDecodeExecutesThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.base64 as base64;"
		"var decoded = base64.try_url_decode(\"eyJzdWIiOiJ1c2VyLTEifQ\");"
		"print decoded[0];"
		"print decoded[1];");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	EXPECT_THAT(output, ::testing::HasSubstr("{\"sub\":\"user-1\"}"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinCryptoModuleExecutesThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.crypto as crypto;"
		"var signature = crypto.hmac_sha256_base64url(\"secret\", \"header.payload\");"
		"print signature;"
		"print crypto.constant_time_equal(signature, signature);"
		"print crypto.constant_time_equal(signature, \"invalid\");");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	EXPECT_THAT(output, ::testing::HasSubstr("false"));
	EXPECT_FALSE(output.empty());
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinNetModuleRejectsInvalidAddressThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.net as net;"
		"net.listen(\"not-an-ip\", 8080);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("std.net.listen expects an IPv4 address"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinHttpServerFacadeExecutesWithoutFilesystemSource)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.http.server as server;"
		"import std.http.raw as raw;"
		"import std.json as json;"
		"fn route(request: [string]) : string {"
		"  return server.ok_json(json.object(["
		"    json.field(\"method\", json.encode_string(server.method(request))),"
		"    json.field(\"item\", json.encode_string(server.segment(request, 2))),"
		"    json.field(\"body\", json.encode_string(server.body(request)))"
		"  ]));"
		"}"
		"var request = [\"POST\", \"/items/42\", \"ping\"];"
		"print server.path(request);"
		"print route(request);"
		"print server.created_json(json.object([json.field(\"created\", json.encode_bool(true))]));"
		"print server.not_found(\"missing\");");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("/items/42"));
	EXPECT_THAT(output, ::testing::HasSubstr("{\"method\":\"POST\",\"item\":\"42\",\"body\":\"ping\"}"));
	EXPECT_THAT(output, ::testing::HasSubstr("HTTP/1.1 201 Created"));
	EXPECT_THAT(output, ::testing::HasSubstr("HTTP/1.1 404 Not Found"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinHttpServerFastHelpersExecuteWithoutFilesystemSource)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.http.server as server;"
		"import std.json as json;"
		"var request = [\"PUT\", \"/items/42\", json.object([json.field(\"id\", json.encode_number(42))])];"
		"print server.route_equals([\"GET\", \"/health\", \"\"], \"GET\", \"/health\");"
		"print server.path_has_prefix(request, \"/items/\");"
		"print server.path_suffix_after_prefix(request, \"/items/\");"
		"print server.created_json(json.object([json.field(\"ok\", json.encode_bool(true))]));");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	EXPECT_THAT(output, ::testing::HasSubstr("HTTP/1.1 201 Created"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinHttpServerHeaderHelpersExecuteWithoutFilesystemSource)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.http.server as server;"
		"import std.http.raw as raw;"
		"print server.response_with_headers(202, \"text/plain\", \"ok\", [\"Location: /next\", \"X-Test: 1\"]);"
		"print server.redirect(\"https://example.com/final\");"
		"print raw.response_with_headers(201, \"application/json\", \"{}\", [\"Location: /raw\"]);"
		"print raw.redirect(\"https://example.com/raw\");");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("HTTP/1.1 202 Accepted"));
	EXPECT_THAT(output, ::testing::HasSubstr("Location: /next"));
	EXPECT_THAT(output, ::testing::HasSubstr("X-Test: 1"));
	EXPECT_THAT(output, ::testing::HasSubstr("HTTP/1.1 302 Found"));
	EXPECT_THAT(output, ::testing::HasSubstr("Location: https://example.com/final"));
	EXPECT_THAT(output, ::testing::HasSubstr("Location: /raw"));
	EXPECT_THAT(output, ::testing::HasSubstr("Location: https://example.com/raw"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinHttpMiddlewareChainExecutesWithoutFilesystemSource)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.http.middleware as middleware;"
		"import std.http.server as server;"
		"import std.json as json;"
		"fn route(request: [string]) : string {"
		"  return server.ok_json(json.object([]));"
		"}"
		"var app = middleware.chain(route, [middleware.with_response_header(\"X-Test\", \"1\")]);"
		"print app([\"GET\", \"/healthz\", \"\"]);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("HTTP/1.1 200 OK"));
	EXPECT_THAT(output, ::testing::HasSubstr("X-Test: 1"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinHttpServerServesConcurrentRequests)
{
	const auto root = MakeTempRoot();
	const int port = ReserveLoopbackPort();

	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.http.server as server;"
		"import std.json as json;"
		"import std.service as service;"
		"import std.time as time;"
		"var seen = service.store_new();"
		"fn route(request: [string]) : string {"
		"  service.store_set(seen, server.path(request), \"1\");"
		"  var attempts = 0;"
		"  while (service.store_len(seen) < 2 && attempts < 20) {"
		"    time.sleep(25);"
		"    attempts = attempts + 1;"
		"  }"
		"  return server.ok_json(json.object(["
		"    json.field(\"path\", json.encode_string(server.path(request))),"
		"    json.field(\"ready\", json.encode_bool(service.store_len(seen) == 2))"
		"  ]));"
		"}"
		"server.serve_n(\"127.0.0.1\", " + std::to_string(port) + ", 2, route);");

	auto serverRun = std::async(std::launch::async, [root]() {
		return RunProgram(root / "samples" / "main.aura");
	});

	const auto started = std::chrono::steady_clock::now();
	auto left = std::async(std::launch::async, [port]() {
		return HttpGetWithRetry(port, "/left");
	});
	auto right = std::async(std::launch::async, [port]() {
		return HttpGetWithRetry(port, "/right");
	});

	const auto leftResponse = left.get();
	const auto rightResponse = right.get();
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - started);

	EXPECT_THAT(leftResponse, ::testing::HasSubstr("HTTP/1.1 200 OK"));
	EXPECT_THAT(leftResponse, ::testing::HasSubstr("{\"path\":\"/left\",\"ready\":true}"));
	EXPECT_THAT(rightResponse, ::testing::HasSubstr("HTTP/1.1 200 OK"));
	EXPECT_THAT(rightResponse, ::testing::HasSubstr("{\"path\":\"/right\",\"ready\":true}"));
	EXPECT_LT(elapsed.count(), 900);
	EXPECT_EQ(serverRun.wait_for(std::chrono::seconds(2)), std::future_status::ready);
	EXPECT_EQ(serverRun.get(), "");

	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinHttpServerInvokesImportedRouteWithModuleGlobals)
{
	const auto root = MakeTempRoot();
	const int port = ReserveLoopbackPort();

	WriteFile(
		root / "samples" / "api.aura",
		"module samples.api;"
		"import std.http.server as server;"
		"import std.json as json;"
		"import std.service as service;"
		"var readiness;"
		"fn init(store) { readiness = store; }"
		"fn route(request: [string]) : string {"
		"  return server.ok_json(json.object(["
		"    json.field(\"status\", json.encode_string(service.store_get(readiness, \"status\")))"
		"  ]));"
		"}"
		"export init;"
		"export route;");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import samples.api as api;"
		"import std.http.server as server;"
		"import std.service as service;"
		"var readiness = service.store_new();"
		"service.store_set(readiness, \"status\", \"ready\");"
		"api.init(readiness);"
		"server.serve_n(\"127.0.0.1\", " + std::to_string(port) + ", 1, api.route);");

	auto serverRun = std::async(std::launch::async, [root]() {
		return RunProgram(root / "samples" / "main.aura");
	});
	const auto response = HttpGetWithRetry(port, "/readyz");

	EXPECT_THAT(response, ::testing::HasSubstr("HTTP/1.1 200 OK"));
	EXPECT_THAT(response, ::testing::HasSubstr("{\"status\":\"ready\"}"));
	EXPECT_EQ(serverRun.wait_for(std::chrono::seconds(2)), std::future_status::ready);
	EXPECT_EQ(serverRun.get(), "");

	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinCoreModuleExecutesThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.core as std;"
		"var values = std.sort([5, 1, 3]);"
		"print values[0];"
		"print std.max(values[1], values[2]);"
		"print std.min(7, std.abs(-4));"
		"print std.len(\"aura\");");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("1"));
	EXPECT_THAT(output, ::testing::HasSubstr("5"));
	EXPECT_THAT(output, ::testing::HasSubstr("4"));
	EXPECT_THAT(output, ::testing::HasSubstr("4"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, StringEscapesDecodeThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"print \"line1\\nline2\";"
		"print \"tab:\\tvalue\";"
		"print \"quote:\\\"ok\\\"\";"
		R"(print "slash:\\";)");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("line1\nline2"));
	EXPECT_THAT(output, ::testing::HasSubstr("tab:\tvalue"));
	EXPECT_THAT(output, ::testing::HasSubstr("quote:\"ok\""));
	EXPECT_THAT(output, ::testing::HasSubstr("slash:\\"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinStdModulesExecuteThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.math as math;"
		"import std.array as arr;"
		"import std.map as maps;"
		"import std.text as text;"
		"var values = [8, 2];"
		"arr.push(values, 5);"
		"arr.remove_at(values, 1);"
		"var sorted = arr.sort(values);"
		"var lookup: map[string]int = map[string]int{};"
		"lookup[\"svc\"] = 7;"
		"print arr.len(sorted);"
		"print arr.pop(sorted);"
		"print lookup[\"svc\"];"
		"print maps.has(lookup, \"svc\");"
		"print maps.delete(lookup, \"svc\");"
		"print math.clamp(math.abs(-12), 0, 10);"
		"print text.concat(\"au\", \"ra\");"
		"print text.contains(\"aura\", \"ur\");"
		"print text.to_string(math.min(9, 4));");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("2"));
	EXPECT_THAT(output, ::testing::HasSubstr("8"));
	EXPECT_THAT(output, ::testing::HasSubstr("7"));
	EXPECT_THAT(output, ::testing::HasSubstr("10"));
	EXPECT_THAT(output, ::testing::HasSubstr("aura"));
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	EXPECT_THAT(output, ::testing::HasSubstr("4"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinMapLiteralAndHelpersExecuteThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.map as maps;"
		"var labels: map[int]string = map[int]string{1: \"open\", 2: \"closed\"};"
		"print labels[1];"
		"print maps.len(labels);"
		"print maps.has(labels, 2);"
		"print maps.delete(labels, 2);"
		"print maps.len(labels);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("open"));
	EXPECT_THAT(output, ::testing::HasSubstr("2"));
	EXPECT_THAT(output, ::testing::HasSubstr("1"));
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinIoAndLogModulesExecuteThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.io as io;"
		"import std.log as log;"
		"io.print(\"a\", 1, true);"
		"io.println(\"b\", 2);"
		"io.printf(\"%s=%d\", \"x\", 42);"
		"log.Info(\"ready\", 7);"
		"log.Warn(\"warn\", false);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("a 1 true"));
	EXPECT_THAT(output, ::testing::HasSubstr("b 2"));
	EXPECT_THAT(output, ::testing::HasSubstr("x=42"));
	EXPECT_THAT(output, ::testing::HasSubstr("[INFO] ready 7"));
	EXPECT_THAT(output, ::testing::HasSubstr("[WARN] warn false"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinIoReadFileExecutesThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	const auto dataFile = root / "samples" / "data.txt";
	WriteFile(dataFile, "task schema");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.io as io;"
		"print io.read_file(\"" + dataFile.string() + "\");");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("task schema"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinLogFatalFailsThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.log as log;"
		"log.Fatal(\"boom\", 7);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("[FATAL] boom 7"));
	EXPECT_THAT(output, ::testing::HasSubstr("Error: Fatal log invoked"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinMemoryModuleIsHiddenFromUserImports)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.memory as rt;"
		"print rt.active_allocations();");

	EXPECT_THROW((void)CompileProgram(root / "samples" / "main.aura"), std::runtime_error);
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinSyncModuleExecutesThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.sync as sync;"
		"var t = sync.spawn();"
		"var m = sync.mutex();"
		"print sync.lock(t, m);"
		"print sync.unlock(t, m);"
		"print sync.finish(t);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinEnvAndTimeModulesExecuteThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.env as env;"
		"import std.time as time;"
		"print env.get_or(\"__AURA_MISSING_ENV__\", \"fallback\");"
		"print env.has(\"__AURA_MISSING_ENV__\");"
		"time.sleep(0);"
		"print time.now_millis() > 0;"
		"print time.monotonic_nanos() > 0;");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("fallback"));
	EXPECT_THAT(output, ::testing::HasSubstr("false"));
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinUuidModuleExecutesThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.uuid as uuid;"
		"var value = uuid.new_v4();"
		"print value;");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("-"));
	EXPECT_THAT(output, ::testing::HasSubstr("4"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinRabbitMqModuleSurfacesConnectionErrors)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.mq.rabbitmq as mq;"
		"var conn = mq.open(\"amqp://guest:guest@127.0.0.1:1/\");"
		"print mq.error(conn) != \"\";"
		"print mq.close(conn);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	EXPECT_THAT(output, ::testing::HasSubstr("false"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinChannelAndCtxModulesExecuteThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.channel as ch;"
		"import std.context as ctx;"
		"var c: ch.Channel<int> = ch.make(1);"
		"print ch.send(c, 42);"
		"var item = ch.recv(c);"
		"print item.tag;"
		"print item[0];"
		"print ch.close(c);"
		"var none_value = ch.recv(c);"
		"print none_value.tag;"
		"var root_ctx = ctx.background();"
		"var child_ctx = ctx.with_cancel(root_ctx);"
		"print ctx.is_cancelled(child_ctx);"
		"print ctx.cancel(root_ctx);"
		"print ctx.is_cancelled(child_ctx);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	EXPECT_THAT(output, ::testing::HasSubstr("false"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinConcurrentModulesExecuteThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.channel as ch;"
		"import std.context as ctx;"
		"import std.concurrent.waitgroup as wg;"
		"import std.concurrent.errorgroup as eg;"
		"import std.log as log;"
		"import std.task as task;"
		"fn calc() : int { return 42; }"
		"fn publish() : void {"
		"  return;"
		"}"
		"fn fail_fast() : void {"
		"  log.Fatal(\"eg-fail\");"
		"}"
		"var work = task.spawn(calc);"
		"var joined = task.join_result(work);"
		"print joined.tag;"
		"print joined[0];"
		"var group = wg.new();"
		"var task1 = wg.spawn(group, publish);"
		"var task2 = wg.spawn(group, publish);"
		"wg.wait(group);"
		"print wg.count(group);"
		"var errors = eg.with_context(ctx.background());"
		"var boom = eg.spawn(errors, fail_fast);"
		"print eg.count(errors);"
		"var err = eg.wait(errors);"
		"print err.tag;"
		"print err[0];"
		"print ctx.is_cancelled(eg.group_context(errors));");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	EXPECT_THAT(output, ::testing::HasSubstr("eg-fail"));
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinAuraFacadeModuleExecutesWithoutFilesystemSource)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
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
		"print svc.store_delete(items, \"42\");"
		"print svc.store_len(items);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("127.0.0.1"));
	EXPECT_THAT(output, ::testing::HasSubstr("8080"));
	EXPECT_THAT(output, ::testing::HasSubstr("201"));
	EXPECT_THAT(output, ::testing::HasSubstr("{\"ok\":true}"));
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	EXPECT_THAT(output, ::testing::HasSubstr("false"));
	EXPECT_THAT(output, ::testing::HasSubstr("\n1\n"));
	EXPECT_THAT(output, ::testing::HasSubstr("\n0\n"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinLowLevelMySqlFacadeIsHiddenFromUserImports)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.mysql as mysql;"
		"print mysql.open(\"host=127.0.0.1;user=root;database=test\");");

	EXPECT_THROW((void)CompileProgram(root / "samples" / "main.aura"), std::runtime_error);
	std::filesystem::remove_all(root);
}
