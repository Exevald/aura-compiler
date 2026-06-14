#pragma once

#include "bytecode/BytecodeGenerator.h"
#include "linker/PackageLinker.h"
#include "vm/execution/chunk/Chunk.h"

#include <filesystem>
#include <string>
#include <vector>

class Compiler
{
public:
	explicit Compiler(
		const std::string& grammarFileName,
		BytecodeGenerator& bytecodeGenerator);

	bool Compile(const std::istream& input, std::ostream& output) const;
	[[nodiscard]] VM::Execution::Chunk CompileToChunk(const std::istream& input) const;
	bool CompileFile(const std::filesystem::path& inputPath, std::ostream& output) const;
	[[nodiscard]] VM::Execution::Chunk CompileFileToChunk(const std::filesystem::path& inputPath) const;

private:
	[[nodiscard]] ASTNodePtr ParseSourceToAst(const std::string& code) const;

	std::string m_grammarText;
	std::vector<std::filesystem::path> m_builtinModuleRoots;
	BytecodeGenerator& m_bytecodeGenerator;
	mutable PackageCache m_packageCache;
};
