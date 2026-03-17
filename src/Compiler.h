#pragma once

#include "../vm/execution/Chunk.h"
#include "BytecodeGenerator.h"
#include "lexer/Lexer.h"

#include <string>

class Compiler
{
public:
	explicit Compiler(
		const std::string& grammarFileName,
		BytecodeGenerator& bytecodeGenerator,
		Lexer& lexer);

	bool Compile(std::istream& input, std::ostream& output) const;
	[[nodiscard]] VM::Execution::Chunk CompileToChunk(const std::istream& input) const;

private:
	std::string m_grammarText;
	BytecodeGenerator& m_bytecodeGenerator;
	Lexer& m_lexer;
};