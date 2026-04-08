#include "src/Compiler.h"
#include "src/vm/execution/VirtualMachine.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

void PrintHelp()
{
	std::cout << "Aura Language Build Tool\n\n"
			  << "Usage:\n"
			  << "  aura <command> [arguments]\n\n"
			  << "Commands:\n"
			  << "  build   Compile source code into bytecode\n"
			  << "  run     Compile and execute immediately\n"
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
	fs::path normalizedInputPath = fs::weakly_canonical(fs::absolute(inputPath));
	fs::path p(normalizedInputPath);
	std::string outputExe = p.stem().string();

	try
	{
		std::ifstream sourceFile(normalizedInputPath);
		if (!sourceFile.is_open())
		{
			throw std::runtime_error("Could not open source file: " + normalizedInputPath.string());
		}
		std::stringstream buffer;
		buffer << sourceFile.rdbuf();
		std::string sourceCode = buffer.str();
		sourceFile.close();

		Lexer lexer(sourceCode);
		BytecodeGenerator bytecodeGenerator;
		std::string grammarFile = "grammar.txt";

		Compiler compiler(grammarFile, bytecodeGenerator, lexer);

		if (command == "build")
		{
			std::ofstream binary(outputExe + ".aurabc", std::ios::binary);

			std::cout << "Building " << normalizedInputPath << "..." << std::endl;
			if (compiler.CompileFile(normalizedInputPath, binary))
			{
				std::cout << "Success! Created " << outputExe << ".aurabc" << std::endl;
			}
		}
		else if (command == "run")
		{
			auto chunk = compiler.CompileFileToChunk(normalizedInputPath);

			VM::Execution::VirtualMachine vm;
			vm.Interpret(&chunk);
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
