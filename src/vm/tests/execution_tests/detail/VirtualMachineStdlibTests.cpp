#include "../VirtualMachineTestSupport.h"

TEST_F(VirtualMachineTest, BuiltinCoreModuleMaxMinLenAndAbsReturnExpectedValues)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.core");
	WriteGetModuleMember(chunk, "max");
	chunk.WriteConstant(int64_t{ 4 });
	chunk.WriteConstant(int64_t{ 9 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "max_value");

	WriteGetGlobal(chunk, "std.core");
	WriteGetModuleMember(chunk, "min");
	chunk.WriteConstant(int64_t{ 4 });
	chunk.WriteConstant(int64_t{ 9 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "min_value");

	WriteGetGlobal(chunk, "std.core");
	WriteGetModuleMember(chunk, "len");
	chunk.WriteConstant(std::make_shared<const std::string>("aura"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	WriteDefineGlobal(chunk, "len_value");

	WriteGetGlobal(chunk, "std.core");
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

	WriteGetGlobal(chunk, "std.core");
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
	WriteGetGlobal(chunk, "std.io");
	WriteGetModuleMember(chunk, "print");
	chunk.WriteConstant(std::make_shared<const std::string>("hello"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_RETURN);

	EXPECT_THAT(RunAndCapture(chunk), ::testing::HasSubstr("hello"));
}

TEST_F(VirtualMachineTest, BuiltinMathArrayAndStringModulesWorkTogether)
{
	Chunk chunk;
	chunk.WriteConstant(int64_t{ 3 });
	chunk.WriteConstant(int64_t{ 1 });
	chunk.Write(OP_BUILD_ARRAY);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "arr");

	WriteGetGlobal(chunk, "std.array");
	WriteGetModuleMember(chunk, "push");
	WriteGetGlobal(chunk, "arr");
	chunk.WriteConstant(int64_t{ 2 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.array");
	WriteGetModuleMember(chunk, "sort");
	WriteGetGlobal(chunk, "arr");
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.math");
	WriteGetModuleMember(chunk, "clamp");
	chunk.WriteConstant(int64_t{ 20 });
	chunk.WriteConstant(int64_t{ 0 });
	chunk.WriteConstant(int64_t{ 10 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(3);
	WriteDefineGlobal(chunk, "clamped");

	WriteGetGlobal(chunk, "std.text");
	WriteGetModuleMember(chunk, "concat");
	chunk.WriteConstant(std::make_shared<const std::string>("au"));
	chunk.WriteConstant(std::make_shared<const std::string>("ra"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	WriteDefineGlobal(chunk, "text");

	WriteGetGlobal(chunk, "std.array");
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
	WriteGetGlobal(chunk, "std.io");
	WriteGetModuleMember(chunk, "print");
	chunk.WriteConstant(std::make_shared<const std::string>("a"));
	chunk.WriteConstant(int64_t{ 1 });
	chunk.WriteConstant(true);
	chunk.Write(OP_CALL);
	chunk.code.push_back(3);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.io");
	WriteGetModuleMember(chunk, "println");
	chunk.WriteConstant(std::make_shared<const std::string>("b"));
	chunk.WriteConstant(int64_t{ 2 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_POP);

	WriteGetGlobal(chunk, "std.io");
	WriteGetModuleMember(chunk, "printf");
	chunk.WriteConstant(std::make_shared<const std::string>("%s=%d"));
	chunk.WriteConstant(std::make_shared<const std::string>("x"));
	chunk.WriteConstant(int64_t{ 42 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(3);
	chunk.Write(OP_RETURN);

	const auto output = RunAndCapture(chunk);
	EXPECT_THAT(output, ::testing::HasSubstr("a 1 true"));
	EXPECT_THAT(output, ::testing::HasSubstr("b 2"));
	EXPECT_THAT(output, ::testing::HasSubstr("x=42"));
}

TEST_F(VirtualMachineTest, BuiltinCoreModuleCastsValues)
{
	Chunk chunk;
	WriteGetGlobal(chunk, "std.core");
	WriteGetModuleMember(chunk, "to_int");
	chunk.WriteConstant(std::make_shared<const std::string>("42"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	WriteDefineGlobal(chunk, "int_value");

	WriteGetGlobal(chunk, "std.core");
	WriteGetModuleMember(chunk, "to_float");
	chunk.WriteConstant(std::make_shared<const std::string>("3.5"));
	chunk.Write(OP_CALL);
	chunk.code.push_back(1);
	WriteDefineGlobal(chunk, "float_value");

	WriteGetGlobal(chunk, "std.core");
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
	WriteGetGlobal(chunk, "std.io");
	WriteGetModuleMember(chunk, "read");
	chunk.Write(OP_CALL);
	chunk.code.push_back(0);
	WriteDefineGlobal(chunk, "token");

	WriteGetGlobal(chunk, "std.io");
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
	WriteGetGlobal(chunk, "std.log");
	WriteGetModuleMember(chunk, "Fatal");
	chunk.WriteConstant(std::make_shared<const std::string>("boom"));
	chunk.WriteConstant(int64_t{ 7 });
	chunk.Write(OP_CALL);
	chunk.code.push_back(2);
	chunk.Write(OP_RETURN);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_THAT(std::string(vm.GetContext().GetError()), ::testing::HasSubstr("Fatal log invoked"));
}
