#include "core/Value.h"
#include "execution/VirtualMachine.h"

#include <iostream>

using namespace VM::Core;
using namespace VM::Execution;

class TestRunner
{
public:
	static void RunAllTests()
	{
		std::cout << "=== Virtual Machine Tests ===\n\n";

		TestArithmetic();
		TestNegate();
		TestDivision();
		TestErrorHandling();

		std::cout << "\n=== All Tests Completed ===\n";
	}

private:
	static void TestArithmetic()
	{
		std::cout << "--- Test 1: Arithmetic (10+2)*5-1 ---\n";

		Chunk chunk;
		chunk.WriteConstant(10.0);
		chunk.WriteConstant(2.0);
		chunk.Write(OpCode::OP_ADD);
		chunk.WriteConstant(5.0);
		chunk.Write(OpCode::OP_MULTIPLY);
		chunk.WriteConstant(1.0);
		chunk.Write(OpCode::OP_SUBTRACT);
		chunk.Write(OpCode::OP_RETURN);

		VirtualMachine vm;
		vm.Interpret(&chunk);
		std::cout << "\n";
	}

	static void TestNegate()
	{
		std::cout << "--- Test 2: Negate (-5) ---\n";

		Chunk chunk;
		chunk.WriteConstant(5.0);
		chunk.Write(OpCode::OP_NEGATE);
		chunk.Write(OpCode::OP_RETURN);

		VirtualMachine vm;
		vm.Interpret(&chunk);
		std::cout << "\n";
	}

	static void TestDivision()
	{
		std::cout << "--- Test 3: Divide (100/4) ---\n";

		Chunk chunk;
		chunk.WriteConstant(100.0);
		chunk.WriteConstant(4.0);
		chunk.Write(OpCode::OP_DIVIDE);
		chunk.Write(OpCode::OP_RETURN);

		VirtualMachine vm;
		vm.Interpret(&chunk);
		std::cout << "\n";
	}

	static void TestErrorHandling()
	{
		std::cout << "--- Test 4: Error Handling (Div by Zero) ---\n";

		Chunk chunk;
		chunk.WriteConstant(10.0);
		chunk.WriteConstant(0.0);
		chunk.Write(OpCode::OP_DIVIDE);
		chunk.Write(OpCode::OP_RETURN);

		VirtualMachine vm;

		bool caught = false;
		try
		{
			vm.Interpret(&chunk);
		}
		catch (const std::exception& e)
		{
			std::cout << "Caught expected error: " << e.what() << "\n";
			caught = true;
		}

		if (!caught)
		{
			if (vm.GetContext().HasError())
			{
				std::cout << "Caught expected error: " << vm.GetContext().GetError() << "\n";
			}
			else
			{
				std::cout << "Warning: expected error not detected\n";
			}
		}
		std::cout << "\n";
	}
};

int main()
{
	try
	{
		TestRunner::RunAllTests();
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << "\n";
		return 1;
	}
}