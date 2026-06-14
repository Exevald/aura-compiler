#include "VirtualMachineTestSupport.h"

#include <sys/socket.h>
#include <unistd.h>

TEST_F(VirtualMachineTest, BuiltinCoreModuleMaxMinLenAndAbsReturnExpectedValues)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.core_native");
	WriteGetModuleMember(chunk, "max");
	chunk.WriteConstant(int64_t{ 4 });
	chunk.WriteConstant(int64_t{ 9 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "max_value");

	WriteGetGlobal(chunk, "std.core_native");
	WriteGetModuleMember(chunk, "min");
	chunk.WriteConstant(int64_t{ 4 });
	chunk.WriteConstant(int64_t{ 9 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "min_value");

	WriteGetGlobal(chunk, "std.core_native");
	WriteGetModuleMember(chunk, "len");
	chunk.WriteConstant(std::make_shared<const std::string>("aura"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	WriteDefineGlobal(chunk, "len_value");

	WriteGetGlobal(chunk, "std.core_native");
	WriteGetModuleMember(chunk, "abs");
	chunk.WriteConstant(int64_t{ -7 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: 7"));

	Value maxValue;
	ASSERT_TRUE(vm.GetContext().GetGlobal("max_value", maxValue));
	EXPECT_EQ(ValueHelper::As<int64_t>(maxValue), 9);

	Value minValue;
	ASSERT_TRUE(vm.GetContext().GetGlobal("min_value", minValue));
	EXPECT_EQ(ValueHelper::As<int64_t>(minValue), 4);

	Value lenValue;
	ASSERT_TRUE(vm.GetContext().GetGlobal("len_value", lenValue));
	EXPECT_EQ(ValueHelper::As<int64_t>(lenValue), 4);
}

TEST_F(VirtualMachineTest, BuiltinCoreModuleSortOrdersArrayInPlace)
{
	Chunk chunk;
	chunk.WriteConstant(int64_t{ 3 });
	chunk.WriteConstant(int64_t{ 1 });
	chunk.WriteConstant(int64_t{ 2 });
	chunk.Write(OP_BUILD_ARRAY);
	chunk.code.push_back(3);
	WriteDefineGlobal(chunk, "arr");

	WriteGetGlobal(chunk, "std.core_native");
	WriteGetModuleMember(chunk, "sort");
	WriteGetGlobal(chunk, "arr");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "arr");
	chunk.WriteConstant(int64_t{ 0 });
	chunk.Write(OP_INDEX_GET);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: 1"));
}

TEST_F(VirtualMachineTest, BuiltinIoModulePrintWritesValue)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.io_native");
	WriteGetModuleMember(chunk, "print");
	chunk.WriteConstant(std::make_shared<const std::string>("hello"));
	chunk.Write(OP_BUILD_ARRAY);
	chunk.code.push_back(1);
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("hello"));
}

TEST_F(VirtualMachineTest, BuiltinCoreAndTextModulesWorkTogether)
{
	Chunk chunk;
	chunk.WriteConstant(int64_t{ 3 });
	chunk.WriteConstant(int64_t{ 1 });
	chunk.Write(OP_BUILD_ARRAY);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "arr");

	WriteGetGlobal(chunk, "std.core_native");
	WriteGetModuleMember(chunk, "push");
	WriteGetGlobal(chunk, "arr");
	chunk.WriteConstant(int64_t{ 2 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.core_native");
	WriteGetModuleMember(chunk, "sort");
	WriteGetGlobal(chunk, "arr");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.core_native");
	WriteGetModuleMember(chunk, "clamp");
	chunk.WriteConstant(int64_t{ 20 });
	chunk.WriteConstant(int64_t{ 0 });
	chunk.WriteConstant(int64_t{ 10 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(3);
	WriteDefineGlobal(chunk, "clamped");

	WriteGetGlobal(chunk, "std.text_native");
	WriteGetModuleMember(chunk, "concat");
	chunk.WriteConstant(std::make_shared<const std::string>("au"));
	chunk.WriteConstant(std::make_shared<const std::string>("ra"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "text");

	WriteGetGlobal(chunk, "std.core_native");
	WriteGetModuleMember(chunk, "pop");
	WriteGetGlobal(chunk, "arr");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: 3"));

	Value clamped;
	ASSERT_TRUE(vm.GetContext().GetGlobal("clamped", clamped));
	EXPECT_EQ(ValueHelper::As<int64_t>(clamped), 10);

	Value text;
	ASSERT_TRUE(vm.GetContext().GetGlobal("text", text));
	EXPECT_EQ(ValueHelper::ToString(text), "aura");
}

TEST_F(VirtualMachineTest, BuiltinIoModuleSupportsVariadicPrintFunctions)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.io_native");
	WriteGetModuleMember(chunk, "print");
	chunk.WriteConstant(std::make_shared<const std::string>("a"));
	chunk.WriteConstant(int64_t{ 1 });
	chunk.WriteConstant(true);
	chunk.Write(OP_BUILD_ARRAY);
	chunk.code.push_back(3);
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.io_native");
	WriteGetModuleMember(chunk, "println");
	chunk.WriteConstant(std::make_shared<const std::string>("b"));
	chunk.WriteConstant(int64_t{ 2 });
	chunk.Write(OP_BUILD_ARRAY);
	chunk.code.push_back(2);
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.io_native");
	WriteGetModuleMember(chunk, "printf");
	chunk.WriteConstant(std::make_shared<const std::string>("%s=%d"));
	chunk.WriteConstant(std::make_shared<const std::string>("x"));
	chunk.WriteConstant(int64_t{ 42 });
	chunk.Write(OP_BUILD_ARRAY);
	chunk.code.push_back(2);
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_RETURN);

	const auto output = RunAndCapture(chunk);
	EXPECT_THAT(output, ::testing::HasSubstr("a 1 true"));
	EXPECT_THAT(output, ::testing::HasSubstr("b 2"));
	EXPECT_THAT(output, ::testing::HasSubstr("x=42"));
}

TEST_F(VirtualMachineTest, BuiltinCoreModuleCastsValues)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.core_native");
	WriteGetModuleMember(chunk, "to_int");
	chunk.WriteConstant(std::make_shared<const std::string>("42"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	WriteDefineGlobal(chunk, "int_value");

	WriteGetGlobal(chunk, "std.core_native");
	WriteGetModuleMember(chunk, "to_float");
	chunk.WriteConstant(std::make_shared<const std::string>("3.5"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	WriteDefineGlobal(chunk, "float_value");

	WriteGetGlobal(chunk, "std.core_native");
	WriteGetModuleMember(chunk, "to_bool");
	chunk.WriteConstant(std::make_shared<const std::string>("true"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: true"));

	Value intValue;
	ASSERT_TRUE(vm.GetContext().GetGlobal("int_value", intValue));
	EXPECT_EQ(ValueHelper::As<int64_t>(intValue), 42);

	Value floatValue;
	ASSERT_TRUE(vm.GetContext().GetGlobal("float_value", floatValue));
	EXPECT_EQ(ValueHelper::As<double>(floatValue), 3.5);
}

TEST_F(VirtualMachineTest, BuiltinIoModuleReadAndReadlnConsumeInput)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.io_native");
	WriteGetModuleMember(chunk, "read");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "token");

	WriteGetGlobal(chunk, "std.io_native");
	WriteGetModuleMember(chunk, "readln");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	chunk.Write(OP_RETURN);

	std::istringstream input("hello rest of line");
	auto* old = std::cin.rdbuf(input.rdbuf());
	const auto output = RunAndCapture(chunk);
	std::cin.rdbuf(old);

	EXPECT_THAT(output, ::testing::HasSubstr("Result:  rest of line"));

	Value token;
	ASSERT_TRUE(vm.GetContext().GetGlobal("token", token));
	EXPECT_EQ(ValueHelper::ToString(token), "hello");
}

TEST_F(VirtualMachineTest, BuiltinLogModuleFatalRaisesRuntimeError)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.log_native");
	WriteGetModuleMember(chunk, "Fatal");
	chunk.WriteConstant(std::make_shared<const std::string>("boom"));
	chunk.WriteConstant(int64_t{ 7 });
	chunk.Write(OP_BUILD_ARRAY);
	chunk.code.push_back(2);
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_THAT(std::string(vm.GetContext().GetError()), ::testing::HasSubstr("Fatal log invoked"));
}

TEST_F(VirtualMachineTest, BuiltinTextModuleRouteHelpersWorkTogether)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.text_native");
	WriteGetModuleMember(chunk, "split");
	chunk.WriteConstant(std::make_shared<const std::string>("/items/42"));
	chunk.WriteConstant(std::make_shared<const std::string>("/"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "parts");

	WriteGetGlobal(chunk, "std.text_native");
	WriteGetModuleMember(chunk, "starts_with");
	chunk.WriteConstant(std::make_shared<const std::string>("/items/42"));
	chunk.WriteConstant(std::make_shared<const std::string>("/items"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "has_prefix");

	WriteGetGlobal(chunk, "std.text_native");
	WriteGetModuleMember(chunk, "index_of");
	chunk.WriteConstant(std::make_shared<const std::string>("/items/42"));
	chunk.WriteConstant(std::make_shared<const std::string>("/"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "first_slash");

	WriteGetGlobal(chunk, "std.text_native");
	WriteGetModuleMember(chunk, "slice");
	chunk.WriteConstant(std::make_shared<const std::string>("/items/42"));
	chunk.WriteConstant(int64_t{ 1 });
	chunk.WriteConstant(int64_t{ 5 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(3);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: items"));

	Value parts;
	ASSERT_TRUE(vm.GetContext().GetGlobal("parts", parts));
	ASSERT_TRUE(std::holds_alternative<VM::Core::ArrayPtr>(parts));
	const auto& elements = std::get<VM::Core::ArrayPtr>(parts)->elements;
	ASSERT_EQ(elements.size(), 3u);
	EXPECT_EQ(ValueHelper::ToString(elements[0]), "");
	EXPECT_EQ(ValueHelper::ToString(elements[1]), "items");
	EXPECT_EQ(ValueHelper::ToString(elements[2]), "42");

	Value hasPrefix;
	ASSERT_TRUE(vm.GetContext().GetGlobal("has_prefix", hasPrefix));
	EXPECT_TRUE(ValueHelper::As<bool>(hasPrefix));

	Value firstSlash;
	ASSERT_TRUE(vm.GetContext().GetGlobal("first_slash", firstSlash));
	EXPECT_EQ(ValueHelper::As<int64_t>(firstSlash), 0);
}

TEST_F(VirtualMachineTest, BuiltinHttpRawModuleParsesRequests)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.http.raw_native");
	WriteGetModuleMember(chunk, "parse_request");
	chunk.WriteConstant(std::make_shared<const std::string>(
		"POST /items/42 HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\nping"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	WriteDefineGlobal(chunk, "request");

	WriteGetGlobal(chunk, "request");
	chunk.WriteConstant(int64_t{ 0 });
	chunk.Write(OP_INDEX_GET);
	WriteDefineGlobal(chunk, "method");

	WriteGetGlobal(chunk, "request");
	chunk.WriteConstant(int64_t{ 1 });
	chunk.Write(OP_INDEX_GET);
	WriteDefineGlobal(chunk, "path");

	WriteGetGlobal(chunk, "request");
	chunk.WriteConstant(int64_t{ 2 });
	chunk.Write(OP_INDEX_GET);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: ping"));

	Value method;
	ASSERT_TRUE(vm.GetContext().GetGlobal("method", method));
	EXPECT_EQ(ValueHelper::ToString(method), "POST");

	Value path;
	ASSERT_TRUE(vm.GetContext().GetGlobal("path", path));
	EXPECT_EQ(ValueHelper::ToString(path), "/items/42");
}

TEST_F(VirtualMachineTest, BuiltinHttpRawModuleReadsRequestsFromSocketConnection)
{
	int sockets[2] = { -1, -1 };
	ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);

	auto connection = std::make_shared<ConnectionHandle>();
	connection->fd = sockets[0];
	vm.GetContext().DefineGlobal("conn", connection);

	const std::string requestText = "GET /health HTTP/1.1\r\n"
									"Host: localhost:18080\r\n"
									"User-Agent: curl/8.7.1\r\n"
									"Accept: */*\r\n"
									"\r\n";
	ASSERT_EQ(::send(sockets[1], requestText.data(), requestText.size(), 0), static_cast<ssize_t>(requestText.size()));

	Chunk chunk;
	WriteGetGlobal(chunk, "std.http.raw_native");
	WriteGetModuleMember(chunk, "read_request");
	WriteGetGlobal(chunk, "conn");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	WriteDefineGlobal(chunk, "request");

	WriteGetGlobal(chunk, "request");
	chunk.WriteConstant(int64_t{ 0 });
	chunk.Write(OP_INDEX_GET);
	WriteDefineGlobal(chunk, "method");

	WriteGetGlobal(chunk, "request");
	chunk.WriteConstant(int64_t{ 1 });
	chunk.Write(OP_INDEX_GET);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: /health"));

	Value method;
	ASSERT_TRUE(vm.GetContext().GetGlobal("method", method));
	EXPECT_EQ(ValueHelper::ToString(method), "GET");

	close(sockets[1]);
}

TEST_F(VirtualMachineTest, BuiltinHttpRawModuleTryReadRequestReturnsEmptyTupleOnClosedConnection)
{
	int sockets[2] = { -1, -1 };
	ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);

	auto connection = std::make_shared<ConnectionHandle>();
	connection->fd = sockets[0];
	vm.GetContext().DefineGlobal("conn", connection);
	::close(sockets[1]);

	Chunk chunk;
	WriteGetGlobal(chunk, "std.http.raw_native");
	WriteGetModuleMember(chunk, "try_read_request");
	WriteGetGlobal(chunk, "conn");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	WriteDefineGlobal(chunk, "request");

	WriteGetGlobal(chunk, "std.core_native");
	WriteGetModuleMember(chunk, "len");
	WriteGetGlobal(chunk, "request");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("Result: 0"));
}
