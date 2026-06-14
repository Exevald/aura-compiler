#include "PackageLinker.h"

#include <algorithm>
#include <fstream>
#include <ranges>
#include <sstream>

std::string PackageLinker::Trim(const std::string_view value)
{
	const auto begin = value.find_first_not_of(" \t\r\n");
	if (begin == std::string_view::npos)
	{
		return "";
	}

	const auto end = value.find_last_not_of(" \t\r\n");
	return std::string(value.substr(begin, end - begin + 1));
}

PackageLinker::ModuleRootContext PackageLinker::ParseModuleFile(const std::filesystem::path& moduleFilePath)
{
	std::ifstream input(moduleFilePath);
	if (!input.is_open())
	{
		throw std::runtime_error("Failed to open module file: " + moduleFilePath.string());
	}

	std::string line;
	std::string moduleName;
	while (std::getline(input, line))
	{
		const std::string trimmed = Trim(line);
		if (trimmed.empty())
		{
			continue;
		}

		if (moduleName.empty())
		{
			constexpr std::string_view prefix = "module ";
			if (!trimmed.starts_with(prefix))
			{
				throw std::runtime_error(
					"Invalid aura.mod in " + moduleFilePath.string()
					+ ": expected 'module <name>' on the first non-empty line");
			}

			moduleName = Trim(std::string_view(trimmed).substr(prefix.size()));
			if (moduleName.empty())
			{
				throw std::runtime_error(
					"Invalid aura.mod in " + moduleFilePath.string()
					+ ": module name must not be empty");
			}
			continue;
		}

		throw std::runtime_error(
			"Invalid aura.mod in " + moduleFilePath.string()
			+ ": unexpected extra content '" + trimmed + "'");
	}

	if (moduleName.empty())
	{
		throw std::runtime_error(
			"Invalid aura.mod in " + moduleFilePath.string()
			+ ": missing 'module <name>' declaration");
	}

	return { std::filesystem::weakly_canonical(moduleFilePath.parent_path()), moduleName };
}

std::filesystem::path PackageLinker::ResolveBuiltinModulePath(const std::string_view relativePath) const
{
	for (const auto& root : m_builtinModuleRoots)
	{
		const auto candidate = root / std::filesystem::path(relativePath);
		if (std::filesystem::exists(candidate))
		{
			return std::filesystem::weakly_canonical(candidate);
		}
	}

	throw std::runtime_error("Failed to resolve builtin stdlib module source: " + std::string(relativePath));
}

std::optional<PackageLinker::ModuleRootContext> PackageLinker::DiscoverModuleRootContext(
	const std::filesystem::path& sourcePath) const
{
	namespace fs = std::filesystem;

	fs::path current = fs::weakly_canonical(sourcePath).parent_path();
	while (!current.empty())
	{
		const fs::path moduleFile = current / "aura.mod";
		if (fs::exists(moduleFile) && fs::is_regular_file(moduleFile))
		{
			return ParseModuleFile(moduleFile);
		}

		if (current == current.parent_path())
		{
			break;
		}
		current = current.parent_path();
	}

	return std::nullopt;
}

std::filesystem::path PackageLinker::ResolveEntryPackagePath(const std::filesystem::path& sourcePath) const
{
	const auto normalizedPath = std::filesystem::weakly_canonical(sourcePath);
	if (!std::filesystem::exists(normalizedPath))
	{
		throw std::runtime_error("Could not open source file: " + sourcePath.string());
	}

	m_activeModuleRootContext = DiscoverModuleRootContext(normalizedPath);

	const auto packageDir = normalizedPath.parent_path();
	const auto sourceFiles = CollectPackageSourceFiles(packageDir);
	if (sourceFiles.empty())
	{
		throw std::runtime_error("No Aura source files found in package directory: " + packageDir.string());
	}

	std::ifstream entryInput(normalizedPath);
	const auto entryRoot = m_parseSource(ReadSource(entryInput));
	if (!entryRoot)
	{
		throw std::runtime_error("Syntax error while loading package source: " + normalizedPath.string());
	}

	const std::string entryModuleName = ExtractDeclaredModuleName(entryRoot.get());
	if (entryModuleName.empty())
	{
		return normalizedPath;
	}

	if (sourceFiles.size() == 1)
	{
		return entryModuleName == ExpectedPackageNameFromPath(packageDir)
			? packageDir
			: normalizedPath;
	}

	for (const auto& candidate : sourceFiles)
	{
		std::ifstream input(candidate);
		auto root = m_parseSource(ReadSource(input));
		if (!root || ExtractDeclaredModuleName(root.get()) != entryModuleName)
		{
			return normalizedPath;
		}
	}

	return packageDir;
}

std::filesystem::path PackageLinker::ResolvePackagePath(
	const std::string& qualifiedName,
	const std::filesystem::path& importerPackagePath) const
{
	namespace fs = std::filesystem;

	if (m_activeModuleRootContext)
	{
		const auto& moduleRootContext = *m_activeModuleRootContext;
		const std::string prefix = moduleRootContext.moduleName + ".";
		if (qualifiedName != moduleRootContext.moduleName
			&& !qualifiedName.starts_with(prefix))
		{
			throw std::runtime_error(
				"Failed to resolve package '" + qualifiedName + "' imported from "
				+ importerPackagePath.string()
				+ ": local module imports must stay within module '"
				+ moduleRootContext.moduleName + "'");
		}

		std::string suffix = qualifiedName == moduleRootContext.moduleName
			? ""
			: qualifiedName.substr(prefix.size());
		fs::path relativePath;
		std::stringstream input(suffix);
		std::string segment;
		while (std::getline(input, segment, '.'))
		{
			if (!segment.empty())
			{
				relativePath /= segment;
			}
		}

		const fs::path base = moduleRootContext.rootPath / relativePath;
		if (fs::exists(base) && fs::is_directory(base) && !CollectPackageSourceFiles(base).empty())
		{
			return base;
		}

		fs::path filePath = base;
		filePath += ".aura";
		if (fs::exists(filePath) && fs::is_regular_file(filePath))
		{
			return filePath;
		}

		throw std::runtime_error(
			"Failed to resolve package '" + qualifiedName + "' imported from "
			+ importerPackagePath.string()
			+ " within module root " + moduleRootContext.rootPath.string());
	}

	fs::path relativePath;
	std::stringstream input(qualifiedName);
	std::string segment;
	while (std::getline(input, segment, '.'))
	{
		relativePath /= segment;
	}

	fs::path current = importerPackagePath.parent_path();
	while (true)
	{
		const fs::path candidateDir = current / relativePath;
		if (fs::exists(candidateDir) && fs::is_directory(candidateDir)
			&& !CollectPackageSourceFiles(candidateDir).empty())
		{
			return candidateDir;
		}

		fs::path legacyFile = current / relativePath;
		legacyFile += ".aura";
		if (fs::exists(legacyFile) && fs::is_regular_file(legacyFile))
		{
			return legacyFile;
		}

		if (current == current.parent_path())
		{
			break;
		}
		current = current.parent_path();
	}

	for (const auto& [cachedPath, cachedPackage] : m_packageCache)
	{
		if (cachedPackage.declaredPackageName == qualifiedName)
		{
			return cachedPath;
		}
	}

	throw std::runtime_error(
		"Failed to resolve package '"
		+ qualifiedName + "' imported from " + importerPackagePath.string());
}

std::vector<std::filesystem::path> PackageLinker::CollectPackageSourceFiles(
	const std::filesystem::path& packagePath)
{
	std::vector<std::filesystem::path> sourceFiles;
	if (!std::filesystem::exists(packagePath))
	{
		return sourceFiles;
	}

	if (std::filesystem::is_regular_file(packagePath) && packagePath.extension() == ".aura")
	{
		sourceFiles.push_back(packagePath);
		return sourceFiles;
	}

	if (!std::filesystem::is_directory(packagePath))
	{
		return sourceFiles;
	}

	for (const auto& entry : std::filesystem::directory_iterator(packagePath))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".aura")
		{
			sourceFiles.push_back(entry.path());
		}
	}

	std::ranges::sort(sourceFiles);
	return sourceFiles;
}

std::string PackageLinker::ExpectedPackageNameFromModuleRoot(
	const std::filesystem::path& packagePath,
	const ModuleRootContext& moduleRootContext)
{
	namespace fs = std::filesystem;

	const fs::path normalizedPackagePath = fs::weakly_canonical(packagePath);
	fs::path relativePath;
	try
	{
		relativePath = fs::relative(normalizedPackagePath, moduleRootContext.rootPath);
	}
	catch (const std::exception&)
	{
		throw std::runtime_error(
			"Package path " + normalizedPackagePath.string()
			+ " is outside module root " + moduleRootContext.rootPath.string());
	}

	if (relativePath.empty() || relativePath.native().starts_with(".."))
	{
		throw std::runtime_error(
			"Package path " + normalizedPackagePath.string()
			+ " is outside module root " + moduleRootContext.rootPath.string());
	}

	if (fs::is_regular_file(normalizedPackagePath))
	{
		relativePath.replace_extension();
	}

	std::string result = moduleRootContext.moduleName;
	for (const auto& segment : relativePath)
	{
		const std::string part = segment.string();
		if (part.empty() || part == ".")
		{
			continue;
		}
		result += ".";
		result += part;
	}
	return result;
}

std::string PackageLinker::ExpectedPackageNameFromPath(const std::filesystem::path& packagePath) const
{
	if (m_activeModuleRootContext)
	{
		return ExpectedPackageNameFromModuleRoot(packagePath, *m_activeModuleRootContext);
	}

	std::filesystem::path namePath = packagePath;
	if (std::filesystem::is_regular_file(packagePath))
	{
		namePath = packagePath;
		namePath.replace_extension();
	}

	std::vector<std::string> parts;
	for (auto current = namePath; !current.empty(); current = current.parent_path())
	{
		if (current.filename().empty())
		{
			break;
		}

		parts.push_back(current.filename().string());
		const std::string& currentPart = parts.back();
		if (currentPart == "samples" || currentPart == "src" || currentPart == "tests"
			|| currentPart == "benchmarks" || currentPart == "stdlib")
		{
			break;
		}
	}

	std::ranges::reverse(parts);
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

void PackageLinker::ValidatePackageFileDeclaration(
	const std::filesystem::path& sourcePath,
	const std::filesystem::path& packagePath,
	const std::string& declaredModuleName,
	const std::optional<ModuleRootContext>& moduleRootContext,
	const bool importedPackage,
	const bool multiFilePackage) const
{
	const std::string expectedPackageName = moduleRootContext
		? ExpectedPackageNameFromModuleRoot(packagePath, *moduleRootContext)
		: ExpectedPackageNameFromPath(packagePath);

	if (declaredModuleName.empty())
	{
		if (importedPackage || multiFilePackage)
		{
			throw std::runtime_error(
				"Package source file is missing a module declaration: " + sourcePath.string());
		}
		return;
	}

	if (!expectedPackageName.empty() && declaredModuleName != expectedPackageName)
	{
		std::string message
			= "Module declaration mismatch in "
			+ sourcePath.string()
			+ ": declared '" + declaredModuleName
			+ "', expected '" + expectedPackageName + "'";
		if (moduleRootContext)
		{
			const auto relativeSource = std::filesystem::relative(
				std::filesystem::weakly_canonical(sourcePath),
				moduleRootContext->rootPath);
			message += " (aura.mod: " + (moduleRootContext->rootPath / "aura.mod").string()
				+ ", module: '" + moduleRootContext->moduleName
				+ "', relative path: " + relativeSource.string() + ")";
		}
		throw std::runtime_error(message);
	}
}
