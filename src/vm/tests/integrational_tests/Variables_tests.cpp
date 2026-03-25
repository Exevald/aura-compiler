#include "../../core/values/ValueHelper.h"
#include "VirtualMachine.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <sstream>

using namespace VM::Core;
using namespace VM::Execution;
using enum OpCode;

class VariablesTest : public ::testing::Test
{
protected:
	VirtualMachine vm;
	Chunk chunk;

	std::string RunAndCapture()
	{
		const std::ostringstream oss;
		std::streambuf* old = std::cout.rdbuf(oss.rdbuf());

		if (vm.Interpret(&chunk))
		{
			if (vm.GetContext().StackSize() > 0)
			{
				std::cout << "Result: ";
				ValueHelper::PrintValue(vm.GetContext().PeekValue(0), std::cout);
			}
		}
		else
		{
			if (vm.GetContext().HasError())
			{
				std::cout << "Error: " << vm.GetContext().GetError();
			}
		}

		std::cout.rdbuf(old);
		return oss.str();
	}
};

TEST_F(VariablesTest, DefineAndGetGlobal)
{
	const uint8_t nameIdx = chunk.AddConstant(std::make_shared<const std::string>("x"));
	chunk.WriteConstant(42.0);
	chunk.Write(OP_DEFINE_GLOBAL);
	chunk.code.push_back(nameIdx);

	chunk.Write(OP_GET_GLOBAL);
	chunk.code.push_back(nameIdx);
	chunk.Write(OP_RETURN);

	std::string output = RunAndCapture();
	EXPECT_THAT(output, ::testing::HasSubstr("Result: 42"));
}

TEST_F(VariablesTest, SetExistingGlobal)
{
	const uint8_t nameIdx = chunk.AddConstant(std::make_shared<const std::string>("g"));

	chunk.WriteConstant(10.0);
	chunk.Write(OP_DEFINE_GLOBAL);
	chunk.code.push_back(nameIdx);

	chunk.WriteConstant(20.0);
	chunk.Write(OP_SET_GLOBAL);
	chunk.code.push_back(nameIdx);

	chunk.Write(OP_GET_GLOBAL);
	chunk.code.push_back(nameIdx);
	chunk.Write(OP_RETURN);

	const std::string output = RunAndCapture();
	EXPECT_THAT(output, ::testing::HasSubstr("20"));
}

TEST_F(VariablesTest, GetUndefinedGlobalRaisesError)
{
	const uint8_t nameIdx = chunk.AddConstant(std::make_shared<const std::string>("not_exists"));
	chunk.Write(OP_GET_GLOBAL);
	chunk.code.push_back(nameIdx);

	EXPECT_FALSE(vm.Interpret(&chunk));
	EXPECT_TRUE(vm.GetContext().HasError());
	EXPECT_THAT(vm.GetContext().GetError(), ::testing::HasSubstr("Undefined variable"));
}

TEST_F(VariablesTest, LocalVariablesReadWrite)
{
	chunk.WriteConstant(10.0);
	chunk.WriteConstant(20.0);

	chunk.WriteConstant(100.0);
	chunk.Write(OP_SET_LOCAL);
	chunk.code.push_back(0);

	chunk.Write(OP_GET_LOCAL);
	chunk.code.push_back(0);
	chunk.Write(OP_RETURN);

	const std::string output = RunAndCapture();
	EXPECT_THAT(output, ::testing::HasSubstr("100"));
}

TEST_F(VariablesTest, LocalArithmetic)
{
	chunk.WriteConstant(5.0);
	chunk.WriteConstant(10.0);

	chunk.Write(OP_GET_LOCAL);
	chunk.code.push_back(0);
	chunk.Write(OP_GET_LOCAL);
	chunk.code.push_back(1);

	chunk.Write(OP_ADD);
	chunk.Write(OP_RETURN);

	const std::string output = RunAndCapture();
	EXPECT_THAT(output, ::testing::HasSubstr("15"));
}