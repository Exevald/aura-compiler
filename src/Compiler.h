#pragma once

#include "bytecode/BytecodeGenerator.h"
#include "lexer/Lexer.h"
#include "vm/execution/chunk/Chunk.h"

#include <filesystem>
#include <string>
#include <unordered_map>

class Compiler
{
public:
	explicit Compiler(
		const std::string& grammarFileName,
		BytecodeGenerator& bytecodeGenerator,
		const Lexer& lexer);

	bool Compile(const std::istream& input, std::ostream& output) const;
	[[nodiscard]] VM::Execution::Chunk CompileToChunk(const std::istream& input) const;
	bool CompileFile(const std::filesystem::path& inputPath, std::ostream& output) const;
	[[nodiscard]] VM::Execution::Chunk CompileFileToChunk(const std::filesystem::path& inputPath) const;

private:
	struct CachedModule
	{
		std::string source;
		std::string declaredModuleName;
	};

	[[nodiscard]] ASTNodePtr ParseSourceToAst(const std::string& code) const;

	std::string m_grammarText;
	BytecodeGenerator& m_bytecodeGenerator;
	mutable std::unordered_map<std::string, CachedModule> m_moduleCache;
};
