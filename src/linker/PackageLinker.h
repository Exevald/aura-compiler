#pragma once

#include "../ast/AST.h"

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct CachedPackage
{
	std::vector<std::string> sources;
	std::vector<std::filesystem::path> sourceFiles;
	std::vector<std::filesystem::file_time_type> sourceFileWriteTimes;
	std::string declaredPackageName;
	std::vector<std::string> imports;
};

using PackageCache = std::unordered_map<std::string, CachedPackage>;

class PackageLinker
{
public:
	using ParseSourceFn = std::function<ASTNodePtr(const std::string&)>;

	PackageLinker(
		ParseSourceFn parseSource,
		PackageCache& packageCache,
		std::vector<std::filesystem::path> builtinModuleRoots);

	[[nodiscard]] ASTNodePtr LinkEntryFile(const std::filesystem::path& sourcePath);

private:
	struct ModuleRootContext
	{
		std::filesystem::path rootPath;
		std::string moduleName;
	};

	struct State
	{
		std::unordered_set<std::string> loading;
		std::unordered_set<std::string> loaded;
	};

	[[nodiscard]] ASTNodePtr LoadPackageTree(
		const std::filesystem::path& packagePath,
		State& state,
		bool importedPackage);
	[[nodiscard]] ASTNodePtr LoadBuiltinModuleTree(
		const std::string& qualifiedName,
		State& state);
	[[nodiscard]] CachedPackage GetOrLoadPackage(
		const std::filesystem::path& packagePath,
		bool importedPackage) const;
	[[nodiscard]] std::filesystem::path ResolveBuiltinModulePath(std::string_view relativePath) const;
	[[nodiscard]] std::filesystem::path ResolveEntryPackagePath(
		const std::filesystem::path& sourcePath) const;
	[[nodiscard]] std::optional<ModuleRootContext> DiscoverModuleRootContext(
		const std::filesystem::path& sourcePath) const;
	[[nodiscard]] std::filesystem::path ResolvePackagePath(
		const std::string& qualifiedName,
		const std::filesystem::path& importerPackagePath) const;
	[[nodiscard]] ASTNodePtr BuildPackageRoot(const std::vector<std::string>& sources) const;
	[[nodiscard]] static ModuleRootContext ParseModuleFile(const std::filesystem::path& moduleFilePath);
	[[nodiscard]] static std::string Trim(std::string_view value);
	[[nodiscard]] static std::string ExpectedPackageNameFromModuleRoot(
		const std::filesystem::path& packagePath,
		const ModuleRootContext& moduleRootContext);

	[[nodiscard]] static std::vector<std::filesystem::path> CollectPackageSourceFiles(
		const std::filesystem::path& packagePath);
	[[nodiscard]] static std::vector<std::string> CollectImports(ASTNode* root);
	[[nodiscard]] static std::string ExtractDeclaredModuleName(ASTNode* root);
	[[nodiscard]] std::string ExpectedPackageNameFromPath(const std::filesystem::path& packagePath) const;
	void ValidatePackageFileDeclaration(
		const std::filesystem::path& sourcePath,
		const std::filesystem::path& packagePath,
		const std::string& declaredModuleName,
		const std::optional<ModuleRootContext>& moduleRootContext,
		bool importedPackage,
		bool multiFilePackage) const;
	[[nodiscard]] static ASTNodePtr BuildCombinedRoot(
		std::vector<ASTNodePtr> importedTrees,
		ASTNodePtr currentTree);
	[[nodiscard]] static std::string ReadSource(const std::istream& input);

	ParseSourceFn m_parseSource;
	PackageCache& m_packageCache;
	std::vector<std::filesystem::path> m_builtinModuleRoots;
	mutable std::optional<ModuleRootContext> m_activeModuleRootContext;
};
