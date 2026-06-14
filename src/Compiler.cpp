#include "Compiler.h"

#include "lexer/Lexer.h"
#include "parser/Parser.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
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

std::string NormalizeGrammar(const std::string& text)
{
	std::stringstream input(text);
	std::stringstream output;
	std::string line;

	while (std::getline(input, line))
	{
		if (line.rfind("```", 0) == 0)
		{
			continue;
		}
		output << line << '\n';
	}

	return output.str();
}

std::filesystem::path ResolveGrammarPath(const std::string& requestedPath)
{
	namespace fs = std::filesystem;

	const fs::path requested(requestedPath);
	if (fs::exists(requested))
	{
		return requested;
	}

	std::vector candidates = { requestedPath };
	if (requested.filename() == "grammar.txt")
	{
		candidates.emplace_back("grammar.md");
	}
	else if (requested.filename() == "grammar.md")
	{
		candidates.emplace_back("grammar.txt");
	}

	fs::path current = fs::current_path();
	while (true)
	{
		for (const auto& candidate : candidates)
		{
			if (const fs::path candidatePath = current / candidate;
				fs::exists(candidatePath))
			{
				return candidatePath;
			}
		}

		if (current == current.parent_path())
		{
			break;
		}
		current = current.parent_path();
	}

	return requested;
}

std::string LoadNormalizedGrammarFromFile(const std::filesystem::path& grammarPath)
{
	static std::mutex cacheMutex;
	static std::unordered_map<std::string, std::string> cache;

	const std::string key = std::filesystem::weakly_canonical(grammarPath).string();
	std::scoped_lock lock(cacheMutex);
	if (const auto it = cache.find(key); it != cache.end())
	{
		return it->second;
	}

	std::ifstream input(grammarPath);
	if (!input.is_open())
	{
		throw std::runtime_error("Failed to open grammar file: " + grammarPath.string());
	}

	std::stringstream buffer;
	buffer << input.rdbuf();
	auto normalized = NormalizeGrammar(buffer.str());
	cache.emplace(key, normalized);
	return normalized;
}

std::vector<std::filesystem::path> ResolveBuiltinModuleRoots(const std::filesystem::path& grammarPath)
{
	std::vector<std::filesystem::path> roots;

	const auto grammarDir = std::filesystem::weakly_canonical(grammarPath).parent_path();
	roots.push_back(grammarDir);

	for (auto dir = std::filesystem::current_path(); !dir.empty(); dir = dir.parent_path())
	{
		if (dir == grammarDir)
		{
			continue;
		}
		if (std::filesystem::exists(dir / "grammar.md"))
		{
			roots.push_back(dir);
		}
		if (dir == dir.root_path())
		{
			break;
		}
	}

	return roots;
}

} // namespace

Compiler::Compiler(
	const std::string& grammarFileName,
	BytecodeGenerator& bytecodeGenerator)
	: m_bytecodeGenerator(bytecodeGenerator)
{
	const auto grammarPath = ResolveGrammarPath(grammarFileName);
	m_grammarText = LoadNormalizedGrammarFromFile(grammarPath);
	m_builtinModuleRoots = ResolveBuiltinModuleRoots(grammarPath);
}

bool Compiler::Compile(const std::istream& input, std::ostream& output) const
{
	const std::string code = ReadSource(input);
	const auto root = ParseSourceToAst(code);
	if (!root)
	{
		std::cerr << "Compilation failed: Syntax Error." << std::endl;
		return false;
	}

	const VM::Execution::Chunk chunk = m_bytecodeGenerator.Compile(root.get());
	SerializeChunk(chunk, output);
	return true;
}

VM::Execution::Chunk Compiler::CompileToChunk(const std::istream& input) const
{
	const std::string code = ReadSource(input);
	const auto root = ParseSourceToAst(code);
	if (!root)
	{
		throw std::runtime_error("Syntax error during compilation");
	}
	return m_bytecodeGenerator.Compile(root.get());
}

bool Compiler::CompileFile(const std::filesystem::path& inputPath, std::ostream& output) const
{
	PackageLinker linker(
		[this](const std::string& source) { return ParseSourceToAst(source); },
		m_packageCache,
		m_builtinModuleRoots);
	const auto root = linker.LinkEntryFile(inputPath);
	if (!root)
	{
		std::cerr << "Compilation failed: Syntax Error." << std::endl;
		return false;
	}

	const VM::Execution::Chunk chunk = m_bytecodeGenerator.Compile(root.get());
	SerializeChunk(chunk, output);
	return true;
}

VM::Execution::Chunk Compiler::CompileFileToChunk(const std::filesystem::path& inputPath) const
{
	PackageLinker linker(
		[this](const std::string& source) { return ParseSourceToAst(source); },
		m_packageCache,
		m_builtinModuleRoots);
	const auto root = linker.LinkEntryFile(inputPath);
	if (!root)
	{
		throw std::runtime_error("Syntax error during compilation");
	}
	return m_bytecodeGenerator.Compile(root.get());
}

ASTNodePtr Compiler::ParseSourceToAst(const std::string& code) const
{
	Lexer lexer(code);
	SLRParser parser(lexer, m_grammarText);
	if (!parser.Parse())
	{
		return nullptr;
	}
	return parser.GetRoot();
}
