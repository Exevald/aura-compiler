#include "PackageLinker.h"

#include "../ast/nodes/statements/BlockNode.h"
#include "../ast/nodes/statements/ImportDeclNode.h"
#include "../ast/nodes/statements/ModuleDeclNode.h"

#include <sstream>

ASTNodePtr PackageLinker::BuildPackageRoot(const std::vector<std::string>& sources) const
{
	auto root = std::make_unique<BlockNode>();
	for (const auto& source : sources)
	{
		auto fileRoot = m_parseSource(source);
		if (!fileRoot)
		{
			return nullptr;
		}

		if (auto* fileBlock = dynamic_cast<BlockNode*>(fileRoot.get()))
		{
			for (auto& statement : fileBlock->statements)
			{
				root->statements.push_back(std::move(statement));
			}
			continue;
		}

		root->statements.push_back(std::move(fileRoot));
	}
	return root;
}

std::vector<std::string> PackageLinker::CollectImports(ASTNode* root)
{
	std::vector<std::string> imports;
	const auto* block = dynamic_cast<BlockNode*>(root);
	if (!block)
	{
		return imports;
	}

	for (const auto& statement : block->statements)
	{
		if (const auto* importDecl = dynamic_cast<ImportDeclNode*>(statement.get()))
		{
			imports.push_back(importDecl->qualifiedName);
		}
	}

	return imports;
}

std::string PackageLinker::ExtractDeclaredModuleName(ASTNode* root)
{
	const auto* block = dynamic_cast<BlockNode*>(root);
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

ASTNodePtr PackageLinker::BuildCombinedRoot(
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

std::string PackageLinker::ReadSource(const std::istream& input)
{
	std::stringstream buffer;
	buffer << input.rdbuf();
	return buffer.str();
}