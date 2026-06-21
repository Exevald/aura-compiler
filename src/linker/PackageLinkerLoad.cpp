#include "../builtin/BuiltinModuleRegistry.h"
#include "PackageLinker.h"

#include <fstream>
#include <unordered_set>
#include <utility>

PackageLinker::PackageLinker(
	ParseSourceFn parseSource,
	PackageCache& packageCache,
	std::vector<std::filesystem::path> builtinModuleRoots)
	: m_parseSource(std::move(parseSource))
	, m_packageCache(packageCache)
	, m_builtinModuleRoots(std::move(builtinModuleRoots))
{
}

ASTNodePtr PackageLinker::LinkEntryFile(const std::filesystem::path& sourcePath)
{
	const auto previousModuleRootContext = m_activeModuleRootContext;
	struct RestoreContext
	{
		std::optional<ModuleRootContext>& slot;
		std::optional<ModuleRootContext> previous;
		~RestoreContext()
		{
			slot = std::move(previous);
		}
	} restore{ m_activeModuleRootContext, previousModuleRootContext };

	State state;
	return LoadPackageTree(ResolveEntryPackagePath(sourcePath), state, false);
}

ASTNodePtr PackageLinker::LoadPackageTree(
	const std::filesystem::path& packagePath,
	State& state,
	const bool importedPackage)
{
	const auto normalizedPath = std::filesystem::weakly_canonical(packagePath);
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
	const auto cachedPackage = GetOrLoadPackage(normalizedPath, importedPackage);

	std::vector<ASTNodePtr> importedTrees;
	importedTrees.reserve(cachedPackage.imports.size());
	for (const auto& importName : cachedPackage.imports)
	{
		if (const auto* builtin = Aura::Builtin::FindBuiltinModule(importName))
		{
			if (!builtin->publicImport)
			{
				throw std::runtime_error(
					"Module '" + cachedPackage.declaredPackageName
					+ "' cannot import internal builtin module '" + importName + "'");
			}
			if (builtin->auraSource.empty() && builtin->sourcePath.empty())
			{
				continue;
			}
			if (auto imported = LoadBuiltinModuleTree(importName, state))
			{
				importedTrees.push_back(std::move(imported));
			}
			continue;
		}

		if (auto imported = LoadPackageTree(
				ResolvePackagePath(importName, normalizedPath),
				state,
				true))
		{
			importedTrees.push_back(std::move(imported));
		}
	}

	auto tree = BuildPackageRoot(cachedPackage.sources);

	state.loading.erase(key);
	state.loaded.insert(key);
	return BuildCombinedRoot(std::move(importedTrees), std::move(tree));
}

ASTNodePtr PackageLinker::LoadBuiltinModuleTree(
	const std::string& qualifiedName,
	State& state)
{
	const std::string key = "builtin:" + qualifiedName;
	if (state.loaded.contains(key))
	{
		return nullptr;
	}
	if (state.loading.contains(key))
	{
		throw std::runtime_error("Cyclic builtin module import detected: " + qualifiedName);
	}

	const auto* builtin = Aura::Builtin::FindBuiltinModule(qualifiedName);
	if (!builtin || (builtin->auraSource.empty() && builtin->sourcePath.empty()))
	{
		return nullptr;
	}

	state.loading.insert(key);
	std::vector<std::string> sources;
	if (!builtin->sourcePath.empty())
	{
		const auto sourcePath = ResolveBuiltinModulePath(builtin->sourcePath);
		std::ifstream input(sourcePath);
		if (!input.is_open())
		{
			throw std::runtime_error("Failed to open builtin module file: " + sourcePath.string());
		}
		sources.push_back(ReadSource(input));
	}
	else
	{
		sources.push_back(std::string(builtin->auraSource));
	}

	auto root = BuildPackageRoot(sources);
	if (!root)
	{
		throw std::runtime_error("Syntax error while loading builtin module: " + qualifiedName);
	}

	const std::string declaredName = ExtractDeclaredModuleName(root.get());
	if (declaredName != qualifiedName)
	{
		throw std::runtime_error(
			"Builtin module declaration mismatch: declared '"
			+ declaredName + "', expected '" + qualifiedName + "'");
	}

	const auto imports = CollectImports(root.get());
	std::vector<ASTNodePtr> importedTrees;
	importedTrees.reserve(imports.size());
	for (const auto& importName : imports)
	{
		if (const auto* importedBuiltin = Aura::Builtin::FindBuiltinModule(importName))
		{
			if (!importedBuiltin->auraSource.empty() || !importedBuiltin->sourcePath.empty())
			{
				if (auto imported = LoadBuiltinModuleTree(importName, state))
				{
					importedTrees.push_back(std::move(imported));
				}
			}
			continue;
		}
		throw std::runtime_error(
			"Builtin stdlib module '" + qualifiedName
			+ "' cannot import non-builtin module '" + importName + "'");
	}

	state.loading.erase(key);
	state.loaded.insert(key);
	return BuildCombinedRoot(std::move(importedTrees), std::move(root));
}

CachedPackage PackageLinker::GetOrLoadPackage(
	const std::filesystem::path& packagePath,
	const bool importedPackage) const
{
	const auto normalizedPath = std::filesystem::weakly_canonical(packagePath);
	const std::string key = normalizedPath.string();
	const auto sourceFiles = CollectPackageSourceFiles(normalizedPath);

	if (const auto it = m_packageCache.find(key); it != m_packageCache.end())
	{
		const auto& cached = it->second;
		if (cached.sourceFiles == sourceFiles
			&& cached.sourceFileWriteTimes.size() == sourceFiles.size())
		{
			bool fresh = true;
			for (size_t i = 0; i < sourceFiles.size(); ++i)
			{
				try
				{
					if (std::filesystem::last_write_time(sourceFiles[i]) != cached.sourceFileWriteTimes[i])
					{
						fresh = false;
						break;
					}
				}
				catch (const std::exception&)
				{
					fresh = false;
					break;
				}
			}

			if (fresh)
			{
				return cached;
			}
		}
	}

	if (sourceFiles.empty())
	{
		throw std::runtime_error("No Aura source files found for package: " + normalizedPath.string());
	}

	CachedPackage package;
	std::unordered_set<std::string> uniqueImports;
	for (const auto& sourceFile : sourceFiles)
	{
		std::ifstream input(sourceFile);
		if (!input.is_open())
		{
			throw std::runtime_error("Failed to open package source file: " + sourceFile.string());
		}

		package.sources.push_back(ReadSource(input));
		auto root = m_parseSource(package.sources.back());
		if (!root)
		{
			throw std::runtime_error("Syntax error while loading package source: " + sourceFile.string());
		}

		const std::string declaredName = ExtractDeclaredModuleName(root.get());
		ValidatePackageFileDeclaration(
			sourceFile,
			normalizedPath,
			declaredName,
			m_activeModuleRootContext,
			importedPackage,
			sourceFiles.size() > 1);
		if (!declaredName.empty())
		{
			if (package.declaredPackageName.empty())
			{
				package.declaredPackageName = declaredName;
			}
			else if (package.declaredPackageName != declaredName)
			{
				throw std::runtime_error(
					"Package contains multiple module declarations: " + normalizedPath.string());
			}
		}

		for (const auto& importName : CollectImports(root.get()))
		{
			if (uniqueImports.insert(importName).second)
			{
				package.imports.push_back(importName);
			}
		}
	}

	package.sourceFiles = sourceFiles;
	package.sourceFileWriteTimes.reserve(sourceFiles.size());
	for (const auto& sourceFile : sourceFiles)
	{
		package.sourceFileWriteTimes.push_back(std::filesystem::last_write_time(sourceFile));
	}

	if (importedPackage && package.declaredPackageName.empty())
	{
		throw std::runtime_error(
			"Imported package is missing a module declaration: " + normalizedPath.string());
	}

	m_packageCache.emplace(key, package);
	return package;
}
