#include "src/Compiler.h"
#include "src/vm/execution/VirtualMachine.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

void PrintHelp()
{
	std::cout << "Aura language compiler\n\n"
			  << "Usage:\n"
			  << "  aura <command> [arguments]\n\n"
			  << "Commands:\n"
			  << "  check   Compile and validate without executing\n"
			  << "  run     Compile, link, and execute immediately\n"
			  << "  help    Show this message\n";
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		PrintHelp();
		return 1;
	}

	std::string command = argv[1];

	if (command == "help")
	{
		PrintHelp();
		return 0;
	}

	if (argc < 3)
	{
		std::cerr << "Error: No input file specified.\n";
		return 1;
	}

	std::string inputPath = argv[2];
	fs::path normalizedInputPath = fs::weakly_canonical(inputPath);
	for (int i = 3; i < argc; ++i)
	{
		std::cerr << "Error: Unknown option: " << argv[i] << '\n';
		return 1;
	}

	try
	{
		BytecodeGenerator bytecodeGenerator;
		const std::string grammarFile = "grammar.txt";
		const Compiler compiler(grammarFile, bytecodeGenerator);

		if (command == "check")
		{
			(void)compiler.CompileFileToChunk(normalizedInputPath);
		}
		else if (command == "run")
		{
			const auto chunk = compiler.CompileFileToChunk(normalizedInputPath);
			if (VM::Execution::VirtualMachine vm(std::make_shared<VM::Runtime::SharedRuntime>(), true); !vm.Interpret(&chunk))
			{
				if (vm.GetContext().HasError())
				{
					std::cerr << "Runtime Error: " << vm.GetContext().GetError() << std::endl;
				}
				return 1;
			}
		}
		else
		{
			std::cerr << "Unknown command: " << command << std::endl;
			return 1;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "Build Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
