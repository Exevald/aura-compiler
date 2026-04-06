#include "ASTBuilder.h"

#include <algorithm>
#include <functional>

namespace
{

std::string ExtractType(const ASTNode* node)
{
	if (!node)
	{
		return "auto";
	}
	if (auto* leaf = dynamic_cast<const LeafNode*>(node))
	{
		return leaf->value;
	}
	auto* raw = dynamic_cast<const RawNode*>(node);
	if (!raw)
	{
		return "auto";
	}
	if (raw->ruleName == "type_guide_opt" && raw->children.size() >= 2)
	{
		return ExtractType(raw->children[1].get());
	}
	if (raw->ruleName == "dataType")
	{
		if (raw->children.empty())
		{
			return "auto";
		}

		const std::string left = ExtractType(raw->children[0].get());
		if (raw->children.size() < 2)
		{
			return left;
		}

		const auto* tail = dynamic_cast<const RawNode*>(raw->children[1].get());
		if (!tail || tail->children.empty())
		{
			return left;
		}

		if (tail->children.size() >= 2)
		{
			return left + "->" + ExtractType(tail->children[1].get());
		}
		return left;
	}
	if (raw->ruleName == "base_dataType" && !raw->children.empty())
	{
		if (auto* leaf = dynamic_cast<LeafNode*>(raw->children[0].get()))
		{
			if (leaf->value == "[")
			{
				return "[" + ExtractType(raw->children[1].get()) + "]";
			}
			if ((leaf->value == "ptr" || leaf->value == "ref") && raw->children.size() >= 3)
			{
				return leaf->value + "<" + ExtractType(raw->children[2].get()) + ">";
			}
			if (leaf->type == "identifier")
			{
				std::string result = leaf->value;
				if (raw->children.size() >= 2)
				{
					const std::string typeArgs = ExtractType(raw->children[1].get());
					if (!typeArgs.empty())
					{
						result += typeArgs;
					}
				}
				return result;
			}
			if (leaf->value == "(" && raw->children.size() >= 2)
			{
				return "(" + ExtractType(raw->children[1].get()) + ")";
			}
			return leaf->value;
		}
	}
	if (raw->ruleName == "type_args_opt")
	{
		if (raw->children.empty())
		{
			return {};
		}
		if (raw->children.size() >= 2)
		{
			return "<" + ExtractType(raw->children[1].get()) + ">";
		}
		return {};
	}
	if (raw->ruleName == "dataType_list" || raw->ruleName == "dataType_list_tail")
	{
		if (raw->children.empty())
		{
			return {};
		}
		const int offset = raw->ruleName == "dataType_list" ? 0 : 1;
		std::string result = ExtractType(raw->children[offset].get());
		if (raw->children.size() > offset + 1)
		{
			const std::string tail = ExtractType(raw->children.back().get());
			if (!tail.empty())
			{
				result += "," + tail;
			}
		}
		return result;
	}
	if (raw->ruleName == "type_constraint")
	{
		if (raw->children.empty())
		{
			return {};
		}
		std::string result = ExtractType(raw->children[0].get());
		if (raw->children.size() > 1)
		{
			const std::string tail = ExtractType(raw->children[1].get());
			if (!tail.empty())
			{
				result += "+" + tail;
			}
		}
		return result;
	}
	if (raw->ruleName == "type_constraint_tail")
	{
		if (raw->children.size() < 2)
		{
			return {};
		}
		std::string result = ExtractType(raw->children[1].get());
		if (raw->children.size() > 2)
		{
			const std::string tail = ExtractType(raw->children[2].get());
			if (!tail.empty())
			{
				result += "+" + tail;
			}
		}
		return result;
	}
	return "auto";
}

std::string ExtractQualifiedId(ASTNode* node)
{
	if (!node)
	{
		return {};
	}

	if (auto* leaf = dynamic_cast<LeafNode*>(node))
	{
		return leaf->value;
	}

	auto* raw = dynamic_cast<RawNode*>(node);
	if (!raw)
	{
		return {};
	}

	if (raw->ruleName == "qualified_id" && raw->children.size() >= 2)
	{
		std::string result = ExtractQualifiedId(raw->children[0].get());
		const std::string tail = ExtractQualifiedId(raw->children[1].get());
		if (!tail.empty())
		{
			result += tail;
		}
		return result;
	}

	if (raw->ruleName == "qualified_id_tail")
	{
		if (raw->children.empty())
		{
			return {};
		}

		std::string result;
		if (raw->children.size() >= 2)
		{
			result = "." + ExtractQualifiedId(raw->children[1].get());
		}
		if (raw->children.size() >= 3)
		{
			result += ExtractQualifiedId(raw->children[2].get());
		}
		return result;
	}

	for (auto& child : raw->children)
	{
		const std::string value = ExtractQualifiedId(child.get());
		if (!value.empty())
		{
			return value;
		}
	}

	return {};
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
			value.size() >= 2 ? value.substr(1, value.size() - 2) : value);
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

ASTNodePtr ASTBuilder::SimplifyFuncDecl(RawNode& raw)
{
	std::string name;
	std::vector<TypeParameterDecl> typeParams;
	std::vector<Parameter> params;
	ASTNodePtr body = nullptr;
	std::vector<ASTNodePtr> metadata;
	std::string retType = "void";

	for (auto& child : raw.children)
	{
		if (const auto* l = dynamic_cast<LeafNode*>(child.get()))
		{
			if (l->type == "identifier" && name.empty())
			{
				name = l->value;
			}
		}
		if (const auto* r = dynamic_cast<RawNode*>(child.get()))
		{
			if (r->ruleName == "type_guide_opt")
			{
				retType = ExtractType(r);
			}
			if (r->ruleName == "type_params_opt")
			{
				FlattenTypeParams(r, typeParams);
			}
			if (r->ruleName == "param_list_opt")
			{
				FlattenParams(r, params);
			}
			else if (r->ruleName == "block_stmt")
			{
				body = Simplify(std::move(child));
			}
			else
			{
				if (auto meta = Simplify(std::move(child)))
				{
					metadata.push_back(std::move(meta));
				}
			}
		}
	}

	auto fn = std::make_unique<FunctionDeclNode>(
		name,
		retType,
		std::move(typeParams),
		std::move(params),
		std::move(body));
	fn->metadata = std::move(metadata);
	return fn;
}

ASTNodePtr ASTBuilder::SimplifyArrowFunc(const RawNode& raw)
{
	std::vector<Parameter> params;
	ASTNodePtr body;

	for (auto& child : raw.children)
	{
		if (auto* r = dynamic_cast<RawNode*>(child.get()))
		{
			if (r->ruleName == "param_list_opt")
			{
				FlattenParams(r, params);
			}
			else if (r->ruleName == "arrow_body")
			{
				if (!r->children.empty())
				{
					body = Simplify(std::move(r->children[0]));
				}
			}
		}
	}

	if (body && !dynamic_cast<BlockNode*>(body.get()))
	{
		auto block = std::make_unique<BlockNode>();
		block->statements.push_back(std::make_unique<ReturnNode>(std::move(body)));
		body = std::move(block);
	}

	return std::make_unique<FunctionExprNode>("", "auto", std::move(params), std::move(body));
}

ASTNodePtr ASTBuilder::SimplifyExportDecl(RawNode& raw)
{
	for (auto& child : raw.children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if (leaf->type == "identifier")
			{
				return std::make_unique<ExportDeclNode>(leaf->value);
			}
			continue;
		}

		const auto* childRaw = dynamic_cast<RawNode*>(child.get());
		if (!childRaw || childRaw->ruleName != "exportable" || childRaw->children.empty())
		{
			continue;
		}

		if (const auto* exportLeaf = dynamic_cast<LeafNode*>(childRaw->children[0].get()))
		{
			if (exportLeaf->type == "identifier")
			{
				return std::make_unique<ExportDeclNode>(exportLeaf->value);
			}
		}

		if (auto exported = Simplify(std::move(child)))
		{
			return std::make_unique<ExportDeclNode>(std::move(exported));
		}
	}

	return nullptr;
}

ASTNodePtr ASTBuilder::SimplifyStructDecl(RawNode& raw)
{
	std::string name;
	std::vector<StructField> fields;
	std::vector<StructMethodDecl> methods;
	std::vector<std::string> implementedInterfaces;
	std::vector<ASTNodePtr> metadata;

	for (auto& child : raw.children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if (leaf->type == "identifier" && name.empty())
			{
				name = leaf->value;
			}
			continue;
		}

		auto* childRaw = dynamic_cast<RawNode*>(child.get());
		if (!childRaw)
		{
			continue;
		}

		if (childRaw->ruleName == "struct_member_list")
		{
			FlattenStructMembers(childRaw, fields, methods);
			continue;
		}

		if (childRaw->ruleName == "implements_opt")
		{
			FlattenQualifiedTypeList(childRaw, implementedInterfaces);
			continue;
		}

		if (auto simplified = Simplify(std::move(child)))
		{
			metadata.push_back(std::move(simplified));
		}
	}

	auto decl = std::make_unique<StructDeclNode>(
		name,
		std::move(fields),
		std::move(implementedInterfaces),
		std::move(methods));
	decl->metadata = std::move(metadata);
	return decl;
}

ASTNodePtr ASTBuilder::SimplifyEnumDecl(const RawNode& raw)
{
	std::string name;
	std::vector<EnumVariantDecl> variants;

	for (auto& child : raw.children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if (leaf->type == "identifier" && name.empty())
			{
				name = leaf->value;
			}
			continue;
		}

		auto* childRaw = dynamic_cast<RawNode*>(child.get());
		if (childRaw && childRaw->ruleName == "variant_list")
		{
			FlattenEnumVariants(childRaw, variants);
		}
	}

	return std::make_unique<EnumDeclNode>(name, std::move(variants));
}

ASTNodePtr ASTBuilder::SimplifyInterfaceDecl(const RawNode& raw)
{
	std::string name;
	std::vector<InterfaceMethodSig> methods;

	for (auto& child : raw.children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if (leaf->type == "identifier" && name.empty())
			{
				name = leaf->value;
			}
			continue;
		}

		auto* childRaw = dynamic_cast<RawNode*>(child.get());
		if (childRaw && childRaw->ruleName == "method_sig_list")
		{
			FlattenInterfaceMethods(childRaw, methods);
		}
	}

	return std::make_unique<InterfaceDeclNode>(name, std::move(methods));
}

ASTNodePtr ASTBuilder::SimplifyBlockChain(RawNode& raw)
{
	auto block = std::make_unique<BlockNode>();

	std::function<void(ASTNode*)> collect = [&](ASTNode* node) {
		if (!node)
		{
			return;
		}
		if (const auto* r = dynamic_cast<RawNode*>(node))
		{
			if (r->ruleName == "program"
				|| r->ruleName == "top_level_list"
				|| r->ruleName == "statement_list"
				|| r->ruleName == "top_level_node")
			{
				for (auto& child : r->children)
				{
					collect(child.get());
				}
			}
			else if (r->ruleName == "block_stmt")
			{
				if (r->children.size() >= 3)
				{
					collect(r->children[1].get());
				}
			}
			else if (r->ruleName == "statement" || r->ruleName == "declaration")
			{
				collect(r->children[0].get());
			}
			else
			{
				auto res = Simplify(std::unique_ptr<ASTNode>(node));
			}
		}
	};

	auto* current = &raw;
	while (current)
	{
		if (current->children.empty())
		{
			break;
		}

		auto item = Simplify(std::move(current->children[0]));
		if (item)
		{
			if (auto* nested = dynamic_cast<BlockNode*>(item.get()))
			{
				for (auto& s : nested->statements)
				{
					block->statements.push_back(std::move(s));
				}
			}
			else
			{
				block->statements.push_back(std::move(item));
			}
		}

		if (current->children.size() > 1)
		{
			current = dynamic_cast<RawNode*>(current->children[1].get());
		}
		else
		{
			current = nullptr;
		}
	}

	return block;
}

ASTNodePtr ASTBuilder::SimplifyVarDeclNoSemi(const RawNode& raw)
{
	std::string name;
	std::string type = "auto";
	ASTNodePtr init = nullptr;
	auto storageClass = VarDeclNode::StorageClass::Default;

	for (auto& c : raw.children)
	{
		if (const auto* l = dynamic_cast<LeafNode*>(c.get()))
		{
			if (l->value == "shared")
			{
				storageClass = VarDeclNode::StorageClass::Shared;
			}
			if (l->value == "thread_local")
			{
				storageClass = VarDeclNode::StorageClass::ThreadLocal;
			}
			if (l->type == "identifier")
			{
				name = l->value;
			}
		}
		if (auto* r = dynamic_cast<RawNode*>(c.get()))
		{
			if (r->ruleName == "type_guide_opt")
			{
				type = ExtractType(r);
			}
			if (r->ruleName == "assign_expr_opt" && r->children.size() >= 2)
			{
				init = Simplify(std::move(r->children[1]));
			}
		}
	}
	return std::make_unique<VarDeclNode>(name, type, std::move(init), false, storageClass);
}

ASTNodePtr ASTBuilder::SimplifyConstDeclNoSemi(RawNode& raw)
{
	std::string name;
	std::string type = "auto";
	ASTNodePtr init = nullptr;

	for (auto& c : raw.children)
	{
		if (const auto* l = dynamic_cast<LeafNode*>(c.get()))
		{
			if (l->type == "identifier")
			{
				name = l->value;
			}
		}
		if (auto* r = dynamic_cast<RawNode*>(c.get()))
		{
			if (r->ruleName == "type_guide_opt")
			{
				type = ExtractType(r);
			}
			if (r->ruleName == "expression")
			{
				init = Simplify(std::move(c));
			}
		}
	}

	return std::make_unique<VarDeclNode>(name, type, std::move(init), true);
}

ASTNodePtr ASTBuilder::SimplifyTypeAliasNoSemi(const RawNode& raw)
{
	std::string name;
	std::string aliasedType = "auto";
	std::vector<TypeParameterDecl> typeParams;

	for (auto& c : raw.children)
	{
		if (const auto* l = dynamic_cast<LeafNode*>(c.get()))
		{
			if (l->type == "identifier" && name.empty())
			{
				name = l->value;
			}
		}
		if (const auto* r = dynamic_cast<RawNode*>(c.get()))
		{
			if (r->ruleName == "type_params_opt")
			{
				FlattenTypeParams(r, typeParams);
			}
			if (r->ruleName == "dataType")
			{
				aliasedType = ExtractType(r);
			}
		}
	}

	return std::make_unique<TypeAliasNode>(name, aliasedType, std::move(typeParams));
}

void ASTBuilder::FlattenTypeParams(const RawNode* raw, std::vector<TypeParameterDecl>& target)
{
	if (!raw)
	{
		return;
	}

	if (raw->ruleName == "type_params_opt")
	{
		if (raw->children.size() >= 2)
		{
			FlattenTypeParams(dynamic_cast<const RawNode*>(raw->children[1].get()), target);
		}
		return;
	}

	if (raw->ruleName == "type_params")
	{
		if (!raw->children.empty())
		{
			FlattenTypeParams(dynamic_cast<const RawNode*>(raw->children[0].get()), target);
		}
		if (raw->children.size() > 1)
		{
			FlattenTypeParams(dynamic_cast<const RawNode*>(raw->children[1].get()), target);
		}
		return;
	}

	if (raw->ruleName == "type_params_tail")
	{
		if (raw->children.size() >= 2)
		{
			FlattenTypeParams(dynamic_cast<const RawNode*>(raw->children[1].get()), target);
		}
		if (raw->children.size() > 2)
		{
			FlattenTypeParams(dynamic_cast<const RawNode*>(raw->children[2].get()), target);
		}
		return;
	}

	if (raw->ruleName == "type_param")
	{
		TypeParameterDecl typeParam;
		for (const auto& child : raw->children)
		{
			if (const auto* leaf = dynamic_cast<const LeafNode*>(child.get()))
			{
				if (leaf->type == "identifier" && typeParam.name.empty())
				{
					typeParam.name = leaf->value;
				}
				continue;
			}

			const auto* childRaw = dynamic_cast<const RawNode*>(child.get());
			if (!childRaw || childRaw->ruleName != "type_constraint_opt" || childRaw->children.size() < 2)
			{
				continue;
			}

			std::string constraints = ExtractType(childRaw->children[1].get());
			size_t start = 0;
			while (start <= constraints.size())
			{
				const size_t plusPos = constraints.find('+', start);
				const std::string constraint = constraints.substr(start, plusPos - start);
				if (!constraint.empty())
				{
					typeParam.constraints.push_back(constraint);
				}
				if (plusPos == std::string::npos)
				{
					break;
				}
				start = plusPos + 1;
			}
		}
		if (!typeParam.name.empty())
		{
			target.push_back(std::move(typeParam));
		}
	}
}

ASTNodePtr ASTBuilder::SimplifyBinaryChain(RawNode& raw)
{
	auto left = Simplify(std::move(raw.children[0]));
	if (raw.children.size() < 2)
	{
		return left;
	}

	auto* tail = dynamic_cast<RawNode*>(raw.children[1].get());
	while (tail && !tail->children.empty())
	{
		std::string op = dynamic_cast<LeafNode*>(tail->children[0].get())->value;
		auto right = Simplify(std::move(tail->children[1]));
		left = std::make_unique<BinaryExprNode>(std::move(left), op, std::move(right));
		tail = (tail->children.size() > 2) ? dynamic_cast<RawNode*>(tail->children[2].get()) : nullptr;
	}

	return left;
}

ASTNodePtr ASTBuilder::SimplifyIdentifierExpr(const RawNode& raw)
{
	ASTNodePtr expr = std::make_unique<IdentifierNode>(
		dynamic_cast<LeafNode*>(raw.children[0].get())->value);

	if (raw.children.size() < 2)
	{
		return expr;
	}

	const auto* trailers = dynamic_cast<RawNode*>(raw.children[1].get());
	while (trailers && !trailers->children.empty())
	{
		if (auto* trailer = dynamic_cast<RawNode*>(trailers->children[0].get());
			trailer && !trailer->children.empty())
		{
			if (const std::string type = dynamic_cast<LeafNode*>(trailer->children[0].get())->value;
				type == ".")
			{
				expr = std::make_unique<MemberAccessNode>(
					std::move(expr),
					dynamic_cast<LeafNode*>(trailer->children[1].get())->value);
			}
			else if (type == "(")
			{
				std::vector<ASTNodePtr> args;
				if (trailer->children.size() >= 2)
				{
					FlattenArgs(dynamic_cast<RawNode*>(trailer->children[1].get()), args);
				}
				expr = std::make_unique<CallNode>(std::move(expr), std::move(args));
			}
			else if (type == "[")
			{
				expr = std::make_unique<IndexNode>(
					std::move(expr),
					Simplify(std::move(trailer->children[1])));
			}
		}

		trailers = (trailers->children.size() > 1)
			? dynamic_cast<RawNode*>(trailers->children[1].get())
			: nullptr;
	}

	return expr;
}

ASTNodePtr ASTBuilder::SimplifyAssignmentExpr(RawNode& raw)
{
	auto lhs = Simplify(std::move(raw.children[0]));
	if (raw.children.size() < 2)
	{
		return lhs;
	}

	auto* tail = dynamic_cast<RawNode*>(raw.children[1].get());
	if (!tail || tail->children.empty() || tail->children.size() < 2)
	{
		return lhs;
	}

	auto value = Simplify(std::move(tail->children[1]));
	if (auto* identifier = dynamic_cast<IdentifierNode*>(lhs.get()))
	{
		return std::make_unique<AssignmentNode>(identifier->name, std::move(value));
	}
	if (auto* index = dynamic_cast<IndexNode*>(lhs.get()))
	{
		if (auto* container = dynamic_cast<IdentifierNode*>(index->container.get()))
		{
			return std::make_unique<AssignmentNode>(
				container->name,
				std::move(value),
				std::move(index->index));
		}
	}
	if (auto* memberAccess = dynamic_cast<MemberAccessNode*>(lhs.get()))
	{
		return std::make_unique<AssignmentNode>(
			std::move(memberAccess->object),
			memberAccess->member,
			std::move(value));
	}
	if (auto* unary = dynamic_cast<UnaryExprNode*>(lhs.get()))
	{
		if (unary->op == "*")
		{
			return std::make_unique<AssignmentNode>(
				std::move(unary->operand), std::move(value));
		}
	}

	return lhs;
}

ASTNodePtr ASTBuilder::SimplifyModuleDecl(const RawNode& raw)
{
	for (auto& child : raw.children)
	{
		if (auto* r = dynamic_cast<RawNode*>(child.get()); r && r->ruleName == "qualified_id")
		{
			return std::make_unique<ModuleDeclNode>(ExtractQualifiedId(r));
		}
	}

	return std::make_unique<ModuleDeclNode>("");
}

ASTNodePtr ASTBuilder::SimplifyImportDecl(const RawNode& raw)
{
	std::string qualifiedName;
	std::string alias;

	for (auto& child : raw.children)
	{
		if (auto* r = dynamic_cast<RawNode*>(child.get()))
		{
			if (r->ruleName == "qualified_id")
			{
				qualifiedName = ExtractQualifiedId(r);
			}
			else if (r->ruleName == "import_as_opt" && r->children.size() >= 2)
			{
				alias = dynamic_cast<LeafNode*>(r->children[1].get())->value;
			}
		}
	}

	if (alias.empty())
	{
		const auto pos = qualifiedName.find_last_of('.');
		alias = (pos == std::string::npos) ? qualifiedName : qualifiedName.substr(pos + 1);
	}

	return std::make_unique<ImportDeclNode>(qualifiedName, alias);
}

ASTNodePtr ASTBuilder::SimplifyIfStmt(RawNode& raw)
{
	return std::make_unique<IfStatementNode>(
		Simplify(std::move(raw.children[2])),
		Simplify(std::move(raw.children[4])),
		(raw.children.size() > 5 ? Simplify(std::move(raw.children[5])) : nullptr));
}

ASTNodePtr ASTBuilder::SimplifyWhileStmt(RawNode& raw)
{
	return std::make_unique<WhileStatementNode>(
		Simplify(std::move(raw.children[2])),
		Simplify(std::move(raw.children[4])));
}

ASTNodePtr ASTBuilder::SimplifyIterStmt(RawNode& raw)
{
	std::string var;
	ASTNodePtr coll, body;
	std::vector<IterAdapter> adapters;

	for (auto& c : raw.children)
	{
		if (const auto* l = dynamic_cast<LeafNode*>(c.get()))
		{
			if (l->type == "identifier")
			{
				var = l->value;
			}
		}
		if (const auto* r = dynamic_cast<RawNode*>(c.get()))
		{
			if (r->ruleName == "expression")
			{
				coll = Simplify(std::move(c));
			}
			if (r->ruleName == "block_stmt")
			{
				body = Simplify(std::move(c));
			}
			if (r->ruleName == "adapter_chain_opt")
			{
				FlattenIterAdapters(r, adapters);
			}
		}
	}

	return std::make_unique<IterNode>(var, std::move(coll), std::move(adapters), std::move(body));
}

ASTNodePtr ASTBuilder::SimplifyArrayLit(const RawNode& raw)
{
	auto arr = std::make_unique<ArrayLiteralNode>();

	for (auto& child : raw.children)
	{
		if (auto* r = dynamic_cast<RawNode*>(child.get()))
		{
			if (r->ruleName == "arg_list_opt")
			{
				FlattenArgs(r, arr->elements);
			}
		}
	}

	return arr;
}

ASTNodePtr ASTBuilder::SimplifyPrimaryComptime(RawNode& raw)
{
	if (auto* l = dynamic_cast<LeafNode*>(raw.children[0].get()))
	{
		if (l->value == "comptime")
		{
			return std::make_unique<ComptimeNode>(Simplify(std::move(raw.children[1])));
		}
	}
	return nullptr;
}

ASTNodePtr ASTBuilder::SimplifyPrintStmt(RawNode& raw)
{
	return std::make_unique<PrintNode>(Simplify(std::move(raw.children[1])));
}

ASTNodePtr ASTBuilder::SimplifyUnsafeStmt(RawNode& raw)
{
	for (auto& child : raw.children)
	{
		if (auto* childRaw = dynamic_cast<RawNode*>(child.get()); childRaw && childRaw->ruleName == "block_stmt")
		{
			return std::make_unique<UnsafeNode>(Simplify(std::move(child)));
		}
	}
	return nullptr;
}

ASTNodePtr ASTBuilder::SimplifyUnary(RawNode& raw)
{
	if (raw.children.size() == 1)
	{
		return Simplify(std::move(raw.children[0]));
	}

	if (raw.children.size() == 2)
	{
		const auto* unaryOp = dynamic_cast<RawNode*>(raw.children[0].get());
		const auto* opLeaf = unaryOp && !unaryOp->children.empty()
			? dynamic_cast<LeafNode*>(unaryOp->children[0].get())
			: nullptr;
		if (opLeaf)
		{
			return std::make_unique<UnaryExprNode>(opLeaf->value, Simplify(std::move(raw.children[1])));
		}
	}

	for (auto& child : raw.children)
	{
		child = Simplify(std::move(child));
	}
	return nullptr;
}

void ASTBuilder::FlattenParams(const RawNode* raw, std::vector<Parameter>& target)
{
	if (!raw || raw->ruleName == "EPSILON")
	{
		return;
	}
	if (raw->ruleName == "param_list_opt" && !raw->children.empty())
	{
		FlattenParams(dynamic_cast<RawNode*>(raw->children[0].get()), target);
		return;
	}

	if (raw->ruleName == "param_list" || raw->ruleName == "param_list_tail")
	{
		const int offset = (raw->ruleName == "param_list") ? 0 : 1;
		if (raw->children.size() > offset)
		{
			if (auto* p = dynamic_cast<RawNode*>(raw->children[offset].get()))
			{
				const std::string pName = dynamic_cast<LeafNode*>(p->children[0].get())->value;
				std::string pType = "auto";
				if (p->children.size() > 1)
				{
					pType = ExtractType(dynamic_cast<RawNode*>(p->children[1].get()));
				}
				target.push_back({ pName, pType });
			}
		}
		if (raw->children.size() > offset + 1)
		{
			FlattenParams(dynamic_cast<RawNode*>(raw->children.back().get()), target);
		}
	}
}

void ASTBuilder::FlattenStructFields(const RawNode* raw, std::vector<StructField>& target)
{
	if (!raw || raw->ruleName == "EPSILON" || raw->children.empty())
	{
		return;
	}

	if (raw->ruleName == "field_decl_list")
	{
		FlattenStructFields(dynamic_cast<RawNode*>(raw->children[0].get()), target);
		if (raw->children.size() > 1)
		{
			FlattenStructFields(dynamic_cast<RawNode*>(raw->children[1].get()), target);
		}
		return;
	}

	if (raw->ruleName != "field_decl")
	{
		return;
	}

	std::string name;
	std::string typeName = "auto";
	for (const auto& child : raw->children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if (leaf->type == "identifier" && name.empty())
			{
				name = leaf->value;
			}
		}
		else if (const auto* childRaw = dynamic_cast<RawNode*>(child.get()))
		{
			if (childRaw->ruleName == "dataType")
			{
				typeName = ExtractType(childRaw);
			}
		}
	}

	target.push_back({ name, typeName });
}

StructMethodDecl ASTBuilder::SimplifyStructMethod(RawNode* raw)
{
	std::string name;
	std::vector<Parameter> params;
	ASTNodePtr body;
	std::vector<ASTNodePtr> metadata;
	std::string retType = "void";

	for (auto& child : raw->children)
	{
		if (const auto* l = dynamic_cast<LeafNode*>(child.get()))
		{
			if (l->type == "identifier" && name.empty())
			{
				name = l->value;
			}
		}
		if (auto* r = dynamic_cast<RawNode*>(child.get()))
		{
			if (r->ruleName == "type_guide_opt")
			{
				retType = ExtractType(r);
			}
			else if (r->ruleName == "param_list_opt")
			{
				FlattenParams(r, params);
			}
			else if (r->ruleName == "block_stmt")
			{
				body = Simplify(std::move(child));
			}
			else
			{
				if (auto meta = Simplify(std::move(child)))
				{
					metadata.push_back(std::move(meta));
				}
			}
		}
	}

	StructMethodDecl method(name, retType, std::move(params), std::move(body));
	method.metadata = std::move(metadata);
	return method;
}

void ASTBuilder::FlattenStructMembers(
	const RawNode* raw,
	std::vector<StructField>& fields,
	std::vector<StructMethodDecl>& methods)
{
	if (!raw || raw->ruleName == "EPSILON" || raw->children.empty())
	{
		return;
	}

	if (raw->ruleName == "struct_member_list")
	{
		FlattenStructMembers(dynamic_cast<RawNode*>(raw->children[0].get()), fields, methods);
		if (raw->children.size() > 1)
		{
			FlattenStructMembers(dynamic_cast<RawNode*>(raw->children[1].get()), fields, methods);
		}
		return;
	}

	if (raw->ruleName != "struct_member" || raw->children.empty())
	{
		if (raw->ruleName == "field_decl")
		{
			FlattenStructFields(raw, fields);
		}
		else if (raw->ruleName == "struct_method")
		{
			methods.push_back(SimplifyStructMethod(const_cast<RawNode*>(raw)));
		}
		return;
	}

	if (const auto* childRaw = dynamic_cast<RawNode*>(raw->children[0].get()))
	{
		if (childRaw->ruleName == "field_decl")
		{
			FlattenStructFields(childRaw, fields);
		}
		else if (childRaw->ruleName == "struct_method")
		{
			methods.push_back(SimplifyStructMethod(const_cast<RawNode*>(childRaw)));
		}
	}
}

void ASTBuilder::FlattenQualifiedTypeList(const RawNode* raw, std::vector<std::string>& target)
{
	if (!raw || raw->ruleName == "EPSILON" || raw->children.empty())
	{
		return;
	}

	if (raw->ruleName == "implements_opt")
	{
		if (raw->children.size() >= 2)
		{
			FlattenQualifiedTypeList(dynamic_cast<RawNode*>(raw->children[1].get()), target);
		}
		return;
	}

	if (raw->ruleName == "interface_type_list")
	{
		target.push_back(ExtractQualifiedId(raw->children[0].get()));
		if (raw->children.size() > 1)
		{
			FlattenQualifiedTypeList(dynamic_cast<RawNode*>(raw->children[1].get()), target);
		}
		return;
	}

	if (raw->ruleName == "interface_type_list_tail")
	{
		if (raw->children.size() > 1)
		{
			target.push_back(ExtractQualifiedId(raw->children[1].get()));
		}
		if (raw->children.size() > 2)
		{
			FlattenQualifiedTypeList(dynamic_cast<RawNode*>(raw->children[2].get()), target);
		}
	}
}

void ASTBuilder::FlattenEnumVariants(const RawNode* raw, std::vector<EnumVariantDecl>& target)
{
	if (!raw || raw->ruleName == "EPSILON" || raw->children.empty())
	{
		return;
	}

	if (raw->ruleName == "variant_list")
	{
		FlattenEnumVariants(dynamic_cast<RawNode*>(raw->children[0].get()), target);
		if (raw->children.size() > 1)
		{
			FlattenEnumVariants(dynamic_cast<RawNode*>(raw->children[1].get()), target);
		}
		return;
	}

	if (raw->ruleName == "variant_list_tail")
	{
		if (raw->children.size() > 1)
		{
			FlattenEnumVariants(dynamic_cast<RawNode*>(raw->children[1].get()), target);
		}
		if (raw->children.size() > 2)
		{
			FlattenEnumVariants(dynamic_cast<RawNode*>(raw->children[2].get()), target);
		}
		return;
	}

	if (raw->ruleName != "variant_decl")
	{
		return;
	}

	EnumVariantDecl variant;
	for (const auto& child : raw->children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if (leaf->type == "identifier" && variant.name.empty())
			{
				variant.name = leaf->value;
			}
		}
		else if (const auto* childRaw = dynamic_cast<RawNode*>(child.get()))
		{
			if (childRaw->ruleName == "variant_args_opt"
				&& childRaw->children.size() >= 2)
			{
				if (const auto* typeList = dynamic_cast<RawNode*>(childRaw->children[1].get()))
				{
					std::function<void(const RawNode*)> flattenTypeList = [&](const RawNode* typeNode) {
						if (!typeNode || typeNode->ruleName == "EPSILON" || typeNode->children.empty())
						{
							return;
						}
						if (typeNode->ruleName == "dataType_list" || typeNode->ruleName == "dataType_list_tail")
						{
							const int offset = (typeNode->ruleName == "dataType_list") ? 0 : 1;
							if (typeNode->children.size() > offset)
							{
								variant.argTypes.push_back(ExtractType(typeNode->children[offset].get()));
							}
							if (typeNode->children.size() > offset + 1)
							{
								flattenTypeList(dynamic_cast<RawNode*>(typeNode->children.back().get()));
							}
						}
					};
					flattenTypeList(typeList);
				}
			}
		}
	}

	target.push_back(std::move(variant));
}

void ASTBuilder::FlattenInterfaceMethods(const RawNode* raw, std::vector<InterfaceMethodSig>& target)
{
	if (!raw || raw->ruleName == "EPSILON" || raw->children.empty())
	{
		return;
	}

	if (raw->ruleName == "method_sig_list")
	{
		FlattenInterfaceMethods(dynamic_cast<RawNode*>(raw->children[0].get()), target);
		if (raw->children.size() > 1)
		{
			FlattenInterfaceMethods(dynamic_cast<RawNode*>(raw->children[1].get()), target);
		}
		return;
	}

	if (raw->ruleName != "method_sig")
	{
		return;
	}

	InterfaceMethodSig method;
	method.returnType = "void";

	for (const auto& child : raw->children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if (leaf->type == "identifier" && method.name.empty())
			{
				method.name = leaf->value;
			}
		}
		else if (const auto* childRaw = dynamic_cast<RawNode*>(child.get()))
		{
			if (childRaw->ruleName == "param_list_opt")
			{
				FlattenParams(childRaw, method.params);
			}
			else if (childRaw->ruleName == "type_guide_opt")
			{
				method.returnType = ExtractType(childRaw);
			}
		}
	}

	target.push_back(std::move(method));
}

void ASTBuilder::FlattenArgs(RawNode* raw, std::vector<ASTNodePtr>& target)
{
	if (!raw || raw->ruleName == "EPSILON")
	{
		return;
	}
	if (raw->ruleName == "arg_list_opt" && !raw->children.empty())
	{
		FlattenArgs(dynamic_cast<RawNode*>(raw->children[0].get()), target);
		return;
	}
	if (raw->ruleName == "arg_list" || raw->ruleName == "arg_list_tail")
	{
		const int offset = (raw->ruleName == "arg_list") ? 0 : 1;
		if (raw->children.size() > offset)
		{
			target.push_back(Simplify(std::move(raw->children[offset])));
		}
		if (raw->children.size() > offset + 1)
		{
			FlattenArgs(dynamic_cast<RawNode*>(raw->children.back().get()), target);
		}
	}
}

void ASTBuilder::FlattenIterAdapters(const RawNode* raw, std::vector<IterAdapter>& target)
{
	if (!raw || raw->ruleName == "EPSILON" || raw->children.empty())
	{
		return;
	}

	if (raw->ruleName == "adapter_chain_opt")
	{
		if (raw->children.size() >= 3)
		{
			FlattenIterAdapters(dynamic_cast<RawNode*>(raw->children[2].get()), target);
		}
		return;
	}

	if (raw->ruleName == "adapter_list")
	{
		FlattenIterAdapters(dynamic_cast<RawNode*>(raw->children[0].get()), target);
		if (raw->children.size() > 1)
		{
			FlattenIterAdapters(dynamic_cast<RawNode*>(raw->children[1].get()), target);
		}
		return;
	}

	if (raw->ruleName == "adapter_list_tail")
	{
		if (raw->children.size() > 1)
		{
			FlattenIterAdapters(dynamic_cast<RawNode*>(raw->children[1].get()), target);
		}
		if (raw->children.size() > 2)
		{
			FlattenIterAdapters(dynamic_cast<RawNode*>(raw->children[2].get()), target);
		}
		return;
	}

	if (raw->ruleName != "adapter")
	{
		return;
	}

	const auto* opLeaf = !raw->children.empty()
		? dynamic_cast<LeafNode*>(raw->children[0].get())
		: nullptr;
	if (!opLeaf)
	{
		return;
	}

	ASTNodePtr argument;
	for (auto& child : raw->children)
	{
		if (auto* childRaw = dynamic_cast<RawNode*>(child.get()); childRaw && childRaw->ruleName == "expression")
		{
			argument = Simplify(std::move(const_cast<ASTNodePtr&>(child)));
			break;
		}
	}

	if (opLeaf->value == "drop")
	{
		target.emplace_back(IterAdapterKind::Drop, std::move(argument));
	}
	else if (opLeaf->value == "take")
	{
		target.emplace_back(IterAdapterKind::Take, std::move(argument));
	}
	else if (opLeaf->value == "reverse")
	{
		target.emplace_back(IterAdapterKind::Reverse);
	}
	else if (opLeaf->value == "filter")
	{
		target.emplace_back(IterAdapterKind::Filter, std::move(argument));
	}
	else if (opLeaf->value == "transform")
	{
		target.emplace_back(IterAdapterKind::Transform, std::move(argument));
	}
}
