#include "Compiler.h"
#include "../bytecode/BytecodeGenerator.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace
{

void SerializeChunk(const VM::Execution::Chunk& chunk, std::ostream& output)
{
	const auto constCount = static_cast<uint32_t>(chunk.constants.size());
	output.write(reinterpret_cast<const char*>(&constCount), sizeof(constCount));
	const auto codeSize = static_cast<uint32_t>(chunk.code.size());
	output.write(reinterpret_cast<const char*>(&codeSize), sizeof(codeSize));
	output.write(reinterpret_cast<const char*>(chunk.code.data()), static_cast<long>(chunk.code.size()));

	std::cout << "Successfully compiled " << codeSize << " bytes of bytecode." << std::endl;
}

std::string ReadSource(const std::istream& input)
{
	std::stringstream buffer;
	buffer << input.rdbuf();
	return buffer.str();
}

} // namespace

Compiler::Compiler(
	const std::string& grammarFileName,
	BytecodeGenerator& bytecodeGenerator,
	Lexer& lexer)
	: m_bytecodeGenerator(bytecodeGenerator)
	, m_lexer(lexer)
{
	std::ifstream input(grammarFileName);
	if (!input.is_open())
	{
		throw std::runtime_error("Failed to open grammar file: " + grammarFileName);
	}
	std::stringstream buffer;
	buffer << input.rdbuf();
	m_grammarText = buffer.str();
}

bool Compiler::Compile(std::istream& input, std::ostream& output) const
{
	std::string code = ReadSource(input);

	SLRParser parser(m_lexer, m_grammarText);

	if (!parser.Parse())
	{
		std::cerr << "Compilation failed: Syntax Error." << std::endl;
		return false;
	}

	const auto root = parser.GetRoot();
	if (!root)
	{
		std::cerr << "Compilation failed: AST Root is null." << std::endl;
		return false;
	}

	const VM::Execution::Chunk chunk = m_bytecodeGenerator.Compile(root.get());
	SerializeChunk(chunk, output);

	return true;
}

VM::Execution::Chunk Compiler::CompileToChunk(const std::istream& input) const
{
	std::string code = ReadSource(input);
	SLRParser parser(m_lexer, m_grammarText);

	if (!parser.Parse())
	{
		throw std::runtime_error("Syntax error during compilation");
	}

	const auto root = parser.GetRoot();
	return m_bytecodeGenerator.Compile(root.get());
}