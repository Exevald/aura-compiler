#include "Compiler.h"
#include "ast/nodes/statements/BlockNode.h"
#include "ast/nodes/statements/ImportDeclNode.h"
#include "ast/nodes/statements/ModuleDeclNode.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <unordered_set>

namespace
{
std::string ReadSource(const std::istream& input);

bool IsBuiltinModuleQualifiedName(const std::string& qualifiedName)
{
	return qualifiedName == "std.core"
		|| qualifiedName == "std.io"
		|| qualifiedName == "std.log"
		|| qualifiedName == "std.math"
		|| qualifiedName == "std.array"
		|| qualifiedName == "std.text"
		|| qualifiedName == "std.memory"
		|| qualifiedName == "std.sync";
}

template <typename ParseFn, typename ModuleCache>
class ModuleGraphLoader
{
public:
	ModuleGraphLoader(ParseFn parseSource, ModuleCache& moduleCache)
		: m_parseSource(std::move(parseSource))
		, m_moduleCache(moduleCache)
	{
	}

	[[nodiscard]] ASTNodePtr LoadEntryProgramTree(
		const std::string& code,
		const std::filesystem::path& sourcePath)
	{
		State state;
		return LoadProgramTree(code, sourcePath, state);
	}

private:
	using CachedModule = typename ModuleCache::mapped_type;

	struct State
	{
		std::unordered_set<std::string> loading;
		std::unordered_set<std::string> loaded;
	};

	[[nodiscard]] ASTNodePtr LoadProgramTree(
		const std::string& code,
		const std::filesystem::path& sourcePath,
		State& state)
	{
		auto root = m_parseSource(code);
		if (!root)
		{
			return nullptr;
		}

		ValidateModuleDeclaration(sourcePath, ExtractDeclaredModuleName(root.get()), false);

		const auto imports = CollectImports(root.get());
		std::vector<ASTNodePtr> importedTrees;
		importedTrees.reserve(imports.size());
		for (const auto& importName : imports)
		{
			if (IsBuiltinModuleQualifiedName(importName))
			{
				continue;
			}

			auto imported = LoadModuleTree(ResolveModulePath(importName, sourcePath), state);
			if (imported)
			{
				importedTrees.push_back(std::move(imported));
			}
		}

		return BuildCombinedRoot(std::move(importedTrees), std::move(root));
	}

	[[nodiscard]] ASTNodePtr LoadModuleTree(
		const std::filesystem::path& modulePath,
		State& state)
	{
		const auto normalizedPath = std::filesystem::weakly_canonical(modulePath);
		const std::string key = normalizedPath.string();

		if (state.loaded.contains(key))
		{
			return nullptr;
		}
		if (state.loading.contains(key))
		{
			throw std::runtime_error("Cyclic module import detected: " + key);
		}

		state.loading.insert(key);
		const auto cachedModule = GetOrLoadModule(normalizedPath);
		ValidateModuleDeclaration(normalizedPath, cachedModule.declaredModuleName, true);
		auto tree = LoadProgramTree(cachedModule.source, normalizedPath, state);

		state.loading.erase(key);
		state.loaded.insert(key);
		return tree;
	}

	[[nodiscard]] CachedModule GetOrLoadModule(const std::filesystem::path& modulePath)
	{
		const auto normalizedPath = std::filesystem::weakly_canonical(modulePath);
		const std::string key = normalizedPath.string();

		if (const auto it = m_moduleCache.find(key); it != m_moduleCache.end())
		{
			return it->second;
		}

		std::ifstream input(normalizedPath);
		if (!input.is_open())
		{
			throw std::runtime_error("Failed to open module file: " + normalizedPath.string());
		}

		CachedModule module;
		module.source = ReadSource(input);
		auto root = m_parseSource(module.source);
		if (!root)
		{
			throw std::runtime_error("Syntax error while loading module: " + normalizedPath.string());
		}

		module.declaredModuleName = ExtractDeclaredModuleName(root.get());
		m_moduleCache.emplace(key, module);
		return module;
	}

	[[nodiscard]] std::filesystem::path ResolveModulePath(
		const std::string& qualifiedName,
		const std::filesystem::path& importerPath) const
	{
		namespace fs = std::filesystem;

		fs::path relativePath;
		std::stringstream input(qualifiedName);
		std::string segment;
		while (std::getline(input, segment, '.'))
		{
			relativePath /= segment;
		}
		relativePath += ".aura";

		fs::path current = importerPath.parent_path();
		while (true)
		{
			const fs::path candidate = current / relativePath;
			if (fs::exists(candidate))
			{
				return candidate;
			}

			if (current == current.parent_path())
			{
				break;
			}
			current = current.parent_path();
		}

		for (const auto& [cachedPath, cachedModule] : m_moduleCache)
		{
			if (cachedModule.declaredModuleName == qualifiedName)
			{
				return cachedPath;
			}
		}

		throw std::runtime_error(
			"Failed to resolve module '"
			+ qualifiedName
			+ "' imported from " + importerPath.string());
	}

	[[nodiscard]] static std::vector<std::string> CollectImports(ASTNode* root)
	{
		std::vector<std::string> imports;
		auto* block = dynamic_cast<BlockNode*>(root);
		if (!block)
		{
			return imports;
		}

		for (const auto& statement : block->statements)
		{
			if (auto* importDecl = dynamic_cast<ImportDeclNode*>(statement.get()))
			{
				imports.push_back(importDecl->qualifiedName);
			}
		}

		return imports;
	}

	[[nodiscard]] static std::string ExtractDeclaredModuleName(ASTNode* root)
	{
		auto* block = dynamic_cast<BlockNode*>(root);
		if (!block)
		{
			return {};
		}

		for (const auto& statement : block->statements)
		{
			if (auto* moduleDecl = dynamic_cast<ModuleDeclNode*>(statement.get()))
			{
				return moduleDecl->qualifiedName;
			}
		}

		return {};
	}

	[[nodiscard]] static std::string ExpectedModuleNameFromPath(const std::filesystem::path& modulePath)
	{
		std::filesystem::path withoutExtension = modulePath;
		withoutExtension.replace_extension();

		std::vector<std::string> parts;
		for (auto current = withoutExtension; !current.empty(); current = current.parent_path())
		{
			if (current.filename().empty())
			{
				break;
			}

			parts.push_back(current.filename().string());
			if (parts.size() >= 2)
			{
				const std::string& last = parts[parts.size() - 1];
				if (last == "samples" || last == "src" || last == "tests")
				{
					break;
				}
			}
		}

		std::reverse(parts.begin(), parts.end());
		std::string result;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			if (i > 0)
			{
				result += ".";
			}
			result += parts[i];
		}
		return result;
	}

	static void ValidateModuleDeclaration(
		const std::filesystem::path& modulePath,
		const std::string& declaredModuleName,
		const bool importedModule)
	{
		const std::string expectedModuleName = ExpectedModuleNameFromPath(modulePath);

		if (declaredModuleName.empty())
		{
			if (importedModule)
			{
				throw std::runtime_error(
					"Imported module file is missing a module declaration: " + modulePath.string());
			}
			return;
		}

		if (!expectedModuleName.empty() && declaredModuleName != expectedModuleName)
		{
			throw std::runtime_error(
				"Module declaration mismatch in "
				+ modulePath.string()
				+ ": declared '" + declaredModuleName
				+ "', expected '" + expectedModuleName + "'");
		}
	}

	[[nodiscard]] static ASTNodePtr BuildCombinedRoot(
		std::vector<ASTNodePtr> importedTrees,
		ASTNodePtr currentTree)
	{
		auto root = std::make_unique<BlockNode>();

		for (auto& importedTree : importedTrees)
		{
			if (importedTree)
			{
				root->statements.push_back(std::move(importedTree));
			}
		}

		if (currentTree)
		{
			root->statements.push_back(std::move(currentTree));
		}

		return root;
	}

	ParseFn m_parseSource;
	ModuleCache& m_moduleCache;
};

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
			if (const fs::path candidatePath = current / candidate; fs::exists(candidatePath))
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

} // namespace

Compiler::Compiler(
	const std::string& grammarFileName,
	BytecodeGenerator& bytecodeGenerator,
	const Lexer& lexer)
	: m_bytecodeGenerator(bytecodeGenerator)
{
	(void)lexer;
	const auto grammarPath = ResolveGrammarPath(grammarFileName);
	m_grammarText = LoadNormalizedGrammarFromFile(grammarPath);
}

bool Compiler::Compile(const std::istream& input, std::ostream& output) const
{
	std::string code = ReadSource(input);
	auto root = ParseSourceToAst(code);
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
	std::string code = ReadSource(input);
	const auto root = ParseSourceToAst(code);
	if (!root)
	{
		throw std::runtime_error("Syntax error during compilation");
	}
	return m_bytecodeGenerator.Compile(root.get());
}

bool Compiler::CompileFile(const std::filesystem::path& inputPath, std::ostream& output) const
{
	const auto normalizedInputPath = std::filesystem::weakly_canonical(inputPath);
	std::ifstream input(normalizedInputPath);
	if (!input.is_open())
	{
		std::cerr << "Compilation failed: Could not open source file." << std::endl;
		return false;
	}

	std::string code = ReadSource(input);
	ModuleGraphLoader loader(
		[this](const std::string& source) { return ParseSourceToAst(source); },
		m_moduleCache);
	const auto root = loader.LoadEntryProgramTree(code, normalizedInputPath);
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
	const auto normalizedInputPath = std::filesystem::weakly_canonical(inputPath);
	std::ifstream input(normalizedInputPath);
	if (!input.is_open())
	{
		throw std::runtime_error("Could not open source file: " + inputPath.string());
	}

	std::string code = ReadSource(input);
	ModuleGraphLoader loader(
		[this](const std::string& source) { return ParseSourceToAst(source); },
		m_moduleCache);
	const auto root = loader.LoadEntryProgramTree(code, normalizedInputPath);
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