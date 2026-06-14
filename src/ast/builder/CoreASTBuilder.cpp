#include "../common/ASTBuilderSupport.h"
#include "ASTBuilder.h"

#include <algorithm>
#include <functional>
#include <iostream>

using ASTBuilderDetail::ExtractQualifiedId;
using ASTBuilderDetail::ExtractType;

namespace
{

std::string UnescapeStringLiteral(std::string_view value)
{
	std::string unescaped;
	unescaped.reserve(value.size());

	for (size_t i = 0; i < value.size(); ++i)
	{
		if (value[i] != '\\' || i + 1 >= value.size())
		{
			unescaped.push_back(value[i]);
			continue;
		}

		switch (value[++i])
		{
		case 'n':
			unescaped.push_back('\n');
			break;
		case 'r':
			unescaped.push_back('\r');
			break;
		case 't':
			unescaped.push_back('\t');
			break;
		case '"':
			unescaped.push_back('"');
			break;
		case '\\':
			unescaped.push_back('\\');
			break;
		default:
			unescaped.push_back(value[i]);
			break;
		}
	}

	return unescaped;
}

} // namespace

ASTNodePtr ASTBuilder::Build(ASTNodePtr node)
{
	auto result = Simplify(std::move(node));
	if (!result)
	{
		return std::make_unique<BlockNode>();
	}

	if (dynamic_cast<BlockNode*>(result.get()))
	{
		return result;
	}

	auto rootBlock = std::make_unique<BlockNode>();
	rootBlock->statements.push_back(std::move(result));
	return rootBlock;
}

ASTNodePtr ASTBuilder::Simplify(ASTNodePtr node)
{
	if (!node)
	{
		return nullptr;
	}
	if (auto* leaf = dynamic_cast<LeafNode*>(node.get()))
	{
		return SimplifyLeaf(std::move(node), *leaf);
	}
	if (auto* raw = dynamic_cast<RawNode*>(node.get()))
	{
		return SimplifyRaw(std::move(node), *raw);
	}

	return node;
}

ASTNodePtr ASTBuilder::SimplifyLeaf(ASTNodePtr node, LeafNode& leaf)
{
	if (leaf.type == "integer_literal")
	{
		return std::make_unique<IntegerLiteralNode>(std::stoll(leaf.value));
	}
	if (leaf.type == "float_literal")
	{
		return std::make_unique<FloatLiteralNode>(std::stod(leaf.value));
	}
	if (leaf.type == "string_literal")
	{
		std::string value = leaf.value;
		return std::make_unique<StringLiteralNode>(
			value.size() >= 2
				? UnescapeStringLiteral(value.substr(1, value.size() - 2))
				: value);
	}
	if (leaf.type == "identifier")
	{
		return std::make_unique<IdentifierNode>(leaf.value);
	}
	return node;
}

ASTNodePtr ASTBuilder::SimplifyRaw(ASTNodePtr node, RawNode& raw)
{
	const std::string& rule = raw.ruleName;

	if (rule == "program"
		|| rule == "top_level_list"
		|| rule == "statement_list"
		|| rule == "block_stmt")
	{
		return SimplifyBlockChain(raw);
	}

	if (rule == "func_decl")
	{
		return SimplifyFuncDecl(raw);
	}
	if (rule == "export_decl")
	{
		return SimplifyExportDecl(raw);
	}
	if (rule == "struct_decl_no_semi")
	{
		return SimplifyStructDecl(raw);
	}
	if (rule == "enum_decl_no_semi")
	{
		return SimplifyEnumDecl(raw);
	}
	if (rule == "interface_decl_no_semi")
	{
		return SimplifyInterfaceDecl(raw);
	}
	if (rule == "effect_def_no_semi")
	{
		return SimplifyEffectDecl(raw);
	}
	if (rule == "actor_decl_no_semi")
	{
		return SimplifyActorDecl(raw);
	}
	if (rule == "arrow_func")
	{
		return SimplifyArrowFunc(raw);
	}
	if (rule == "var_decl_no_semi")
	{
		return SimplifyVarDeclNoSemi(raw);
	}
	if (rule == "const_decl_no_semi")
	{
		return SimplifyConstDeclNoSemi(raw);
	}
	if (rule == "type_alias_no_semi")
	{
		return SimplifyTypeAliasNoSemi(raw);
	}
	if (rule == "assignment_expr")
	{
		return SimplifyAssignmentExpr(raw);
	}
	if (rule == "module_decl")
	{
		return SimplifyModuleDecl(raw);
	}
	if (rule == "import_decl")
	{
		return SimplifyImportDecl(raw);
	}
	if (rule == "if_stmt" && raw.children.size() >= 5)
	{
		return SimplifyIfStmt(raw);
	}
	if (rule == "while_stmt" && raw.children.size() >= 5)
	{
		return SimplifyWhileStmt(raw);
	}
	if (rule == "iter_stmt")
	{
		return SimplifyIterStmt(raw);
	}
	if (rule == "array_lit")
	{
		return SimplifyArrayLit(raw);
	}
	if (rule == "map_lit")
	{
		return SimplifyMapLit(raw);
	}
	if (rule == "primary" && raw.children.size() >= 2)
	{
		if (auto res = SimplifyPrimaryComptime(raw))
		{
			return res;
		}
	}
	if (rule == "print_stmt")
	{
		return SimplifyPrintStmt(raw);
	}
	if (rule == "unsafe_stmt")
	{
		return SimplifyUnsafeStmt(raw);
	}
	if (rule == "transaction_stmt")
	{
		return SimplifyTransactionStmt(raw);
	}
	if (rule == "handle_stmt")
	{
		return SimplifyHandleStmt(raw);
	}
	if (rule == "unary")
	{
		return SimplifyUnary(raw);
	}
	if (rule == "identifier_expr")
	{
		return SimplifyIdentifierExpr(raw);
	}
	if (rule == "return_stmt")
	{
		return std::make_unique<ReturnNode>(
			raw.children.size() > 1 ? Simplify(std::move(raw.children[1])) : nullptr);
	}

	if (rule == "logic_or"
		|| rule == "logic_and"
		|| rule == "equality"
		|| rule == "relational"
		|| rule == "additive"
		|| rule == "multiplicative")
	{
		return SimplifyBinaryChain(raw);
	}

	if (raw.children.size() == 1)
	{
		return Simplify(std::move(raw.children[0]));
	}

	if (raw.children.size() == 2)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(raw.children[1].get()))
		{
			if (leaf->value == ";")
			{
				return Simplify(std::move(raw.children[0]));
			}
		}
	}

	for (auto& child : raw.children)
	{
		child = Simplify(std::move(child));
	}
	return node;
}
