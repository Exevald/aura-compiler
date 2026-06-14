#pragma once

#include "Compiler.h"
#include "VirtualMachine.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

inline std::filesystem::path MakeVmCompilerTempRoot()
{
	const auto root = std::filesystem::temp_directory_path()
		/ ("aura_vm_module_tests_" + std::to_string(::getpid()));
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	return root;
}

inline void WriteVmCompilerTestFile(const std::filesystem::path& path, const std::string& contents)
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream output(path);
	output << contents;
}

inline std::filesystem::path VmCompilerGrammarPath()
{
	for (auto dir = std::filesystem::current_path(); !dir.empty(); dir = dir.parent_path())
	{
		if (const auto candidate = dir / "grammar.md";
			std::filesystem::exists(candidate))
		{
			return candidate;
		}
		if (dir == dir.root_path())
		{
			break;
		}
	}
	return std::filesystem::current_path() / "grammar.md";
}

inline std::filesystem::path VmCompilerRepoRoot()
{
	return VmCompilerGrammarPath().parent_path();
}

inline std::string RunVmCompilerProgram(const std::filesystem::path& entryFile)
{
	BytecodeGenerator generator;
	const Compiler compiler(VmCompilerGrammarPath().string(), generator);

	const auto chunk = compiler.CompileFileToChunk(entryFile);
	VM::Execution::VirtualMachine vm;

	const std::ostringstream oss;
	std::streambuf* old = std::cout.rdbuf(oss.rdbuf());

	if (const bool ok = vm.Interpret(&chunk);
		!ok && vm.GetContext().HasError())
	{
		std::cout << "Error: " << vm.GetContext().GetError();
	}

	std::cout.rdbuf(old);
	return oss.str();
}

inline void CompileVmCompilerProgram(const std::filesystem::path& entryFile)
{
	BytecodeGenerator generator;
	Compiler compiler(VmCompilerGrammarPath().string(), generator);
	(void)compiler.CompileFileToChunk(entryFile);
}

inline std::filesystem::path MakeTempRoot()
{
	return MakeVmCompilerTempRoot();
}

inline void WriteFile(const std::filesystem::path& path, const std::string& contents)
{
	WriteVmCompilerTestFile(path, contents);
}

inline std::filesystem::path GrammarPath()
{
	return VmCompilerGrammarPath();
}

inline std::filesystem::path RepoRoot()
{
	return VmCompilerRepoRoot();
}

inline std::string RunProgram(const std::filesystem::path& entryFile)
{
	return RunVmCompilerProgram(entryFile);
}

inline void CompileProgram(const std::filesystem::path& entryFile)
{
	CompileVmCompilerProgram(entryFile);
}
