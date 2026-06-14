#pragma once

#include "../../core/values/ValueHelper.h"
#include "VirtualMachine.h"

#include <gtest/gtest.h>
#include <sstream>

using namespace VM::Core;
using namespace VM::Execution;
using enum OpCode;

inline std::string CaptureBytecodeVmOutput(const std::function<void(VirtualMachine&, Chunk&)>& testFunc)
{
	Chunk chunk;
	VirtualMachine vm;

	std::ostringstream oss;
	std::streambuf* old = std::cout.rdbuf(oss.rdbuf());

	testFunc(vm, chunk);

	if (!vm.GetContext().HasError() && vm.GetContext().StackSize() > 0)
	{
		std::cout << "Result: ";
		ValueHelper::PrintValue(vm.GetContext().PeekValue(0), std::cout);
	}

	std::cout.rdbuf(old);
	return oss.str();
}

inline std::string CaptureVMOutput(const std::function<void(VirtualMachine&, Chunk&)>& testFunc)
{
	return CaptureBytecodeVmOutput(testFunc);
}