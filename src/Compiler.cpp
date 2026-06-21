#include "Compiler.h"

#include "lexer/Lexer.h"
#include "parser/Parser.h"

#include <filesystem>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <sstream>
#include <type_traits>

namespace
{

template <typename T>
void WritePod(std::ostream& output, const T& value)
{
	static_assert(std::is_trivially_copyable_v<T>);
	output.write(reinterpret_cast<const char*>(&value), static_cast<std::streamsize>(sizeof(T)));
}

void WriteString(std::ostream& output, const std::string& value)
{
	const auto size = static_cast<uint32_t>(value.size());
	WritePod(output, size);
	output.write(value.data(), static_cast<std::streamsize>(value.size()));
}

void SerializeValue(const VM::Core::Value& value, std::ostream& output);

void SerializeFunction(const VM::Core::FunctionPtr& function, std::ostream& output)
{
	if (!function)
	{
		throw std::runtime_error("Cannot serialize null function constant");
	}

	WriteString(output, function->name);
	WritePod(output, static_cast<int32_t>(function->arity));
	WritePod(output, static_cast<int32_t>(function->minArity));
	WritePod(output, static_cast<uint8_t>(function->variadic ? 1 : 0));

	const auto captureCount = static_cast<uint32_t>(function->captureNames.size());
	WritePod(output, captureCount);
	for (const auto& captureName : function->captureNames)
	{
		WriteString(output, captureName);
	}

	if (!function->chunk)
	{
		throw std::runtime_error("Cannot serialize function without bytecode chunk");
	}

	const auto constantCount = static_cast<uint32_t>(function->chunk->constants.size());
	WritePod(output, constantCount);
	for (const auto& constant : function->chunk->constants)
	{
		SerializeValue(constant, output);
	}

	const auto codeSize = static_cast<uint32_t>(function->chunk->code.size());
	WritePod(output, codeSize);
	if (codeSize > 0)
	{
		output.write(reinterpret_cast<const char*>(function->chunk->code.data()),
			static_cast<std::streamsize>(function->chunk->code.size()));
	}
}

void SerializeValue(const VM::Core::Value& value, std::ostream& output)
{
	enum class ValueTag : uint8_t
	{
		Null = 0,
		Bool = 1,
		Int = 2,
		Double = 3,
		String = 4,
		Function = 5,
	};

	if (std::holds_alternative<std::monostate>(value))
	{
		WritePod(output, static_cast<uint8_t>(ValueTag::Null));
		return;
	}
	if (std::holds_alternative<bool>(value))
	{
		WritePod(output, static_cast<uint8_t>(ValueTag::Bool));
		WritePod(output, static_cast<uint8_t>(std::get<bool>(value) ? 1 : 0));
		return;
	}
	if (std::holds_alternative<int64_t>(value))
	{
		WritePod(output, static_cast<uint8_t>(ValueTag::Int));
		WritePod(output, std::get<int64_t>(value));
		return;
	}
	if (std::holds_alternative<double>(value))
	{
		WritePod(output, static_cast<uint8_t>(ValueTag::Double));
		WritePod(output, std::get<double>(value));
		return;
	}
	if (std::holds_alternative<VM::Core::StringPtr>(value))
	{
		WritePod(output, static_cast<uint8_t>(ValueTag::String));
		const auto& str = std::get<VM::Core::StringPtr>(value);
		WritePod(output, static_cast<uint8_t>(str ? 1 : 0));
		if (str)
		{
			WriteString(output, *str);
		}
		return;
	}
	if (std::holds_alternative<VM::Core::FunctionPtr>(value))
	{
		WritePod(output, static_cast<uint8_t>(ValueTag::Function));
		SerializeFunction(std::get<VM::Core::FunctionPtr>(value), output);
		return;
	}

	throw std::runtime_error("Unsupported constant type in bytecode serialization");
}

void SerializeChunk(const VM::Execution::Chunk& chunk, std::ostream& output)
{
	output.write("AURB", 4);
	WritePod(output, static_cast<uint8_t>(1));

	const auto constCount = static_cast<uint32_t>(chunk.constants.size());
	WritePod(output, constCount);
	for (const auto& constant : chunk.constants)
	{
		SerializeValue(constant, output);
	}

	const auto codeSize = static_cast<uint32_t>(chunk.code.size());
	WritePod(output, codeSize);
	if (codeSize > 0)
	{
		output.write(reinterpret_cast<const char*>(chunk.code.data()),
			static_cast<std::streamsize>(chunk.code.size()));
	}
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
