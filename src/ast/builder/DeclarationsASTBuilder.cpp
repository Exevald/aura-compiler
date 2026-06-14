#include "../common/ASTBuilderSupport.h"
#include "ASTBuilder.h"

#include <algorithm>
#include <functional>

using ASTBuilderDetail::ExtractQualifiedId;
using ASTBuilderDetail::ExtractType;

ASTNodePtr ASTBuilder::SimplifyFuncDecl(RawNode& raw)
{
	std::string name;
	bool isComptime = false;
	std::vector<TypeParameterDecl> typeParams;
	std::vector<Parameter> params;
	std::vector<ContextBinding> contextRequirements;
	std::vector<std::string> raisedEffects;
	std::vector<std::unique_ptr<ContractNode>> contracts;
	ASTNodePtr body = nullptr;
	std::string retType = "auto";

	for (auto& child : raw.children)
	{
		if (const auto* l = dynamic_cast<LeafNode*>(child.get()))
		{
			if (l->value == "comptime")
			{
				isComptime = true;
			}
			if ((l->type == "identifier" || l->value == "print") && name.empty())
			{
				name = l->value;
			}
		}
		if (const auto* r = dynamic_cast<RawNode*>(child.get()))
		{
			if (r->ruleName == "function_name" && !r->children.empty() && name.empty())
			{
				if (const auto* nameLeaf = dynamic_cast<LeafNode*>(r->children[0].get()))
				{
					name = nameLeaf->value;
				}
			}
			if (r->ruleName == "type_guide_opt" && r->children.size() >= 2)
			{
				retType = ExtractType(r);
			}
			if (r->ruleName == "type_params_opt")
			{
				FlattenTypeParams(r, typeParams);
			}
			if (r->ruleName == "param_list_opt")
			{
				FlattenParams(const_cast<RawNode*>(r), params);
			}
			else if (r->ruleName == "context_req_opt")
			{
				FlattenContextBindings(r, contextRequirements);
			}
			else if (r->ruleName == "effect_spec_opt")
			{
				FlattenRaisedEffects(r, raisedEffects);
			}
			else if (r->ruleName == "contract_list")
			{
				FlattenContracts(r, contracts);
			}
			else if (r->ruleName == "block_stmt")
			{
				body = Simplify(std::move(child));
			}
		}
	}

	auto fn = std::make_unique<FunctionDeclNode>(
		name,
		retType,
		isComptime,
		std::move(typeParams),
		std::move(params),
		std::move(contextRequirements),
		std::move(raisedEffects),
		std::move(contracts),
		std::move(body));
	return fn;
}

ASTNodePtr ASTBuilder::SimplifyArrowFunc(const RawNode& raw)
{
	std::vector<Parameter> params;
	std::vector<std::string> raisedEffects;
	ASTNodePtr body;

	for (auto& child : raw.children)
	{
		if (auto* r = dynamic_cast<RawNode*>(child.get()))
		{
			if (r->ruleName == "param_list_opt")
			{
				FlattenParams(r, params);
			}
			else if (r->ruleName == "effect_spec_opt")
			{
				FlattenRaisedEffects(r, raisedEffects);
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

	return std::make_unique<FunctionExprNode>(
		"",
		"auto",
		std::move(params),
		std::move(raisedEffects),
		std::move(body));
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

ASTNodePtr ASTBuilder::SimplifyStructDecl(const RawNode& raw)
{
	std::string name;
	std::vector<TypeParameterDecl> typeParams;
	std::vector<StructField> fields;
	std::vector<StructMethodDecl> methods;
	std::vector<std::string> implementedInterfaces;
	std::vector<std::unique_ptr<ContractNode>> contracts;

	for (auto& child : raw.children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if ((leaf->type == "identifier" || leaf->value == "print") && name.empty())
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
		if (childRaw->ruleName == "type_params_opt")
		{
			FlattenTypeParams(childRaw, typeParams);
			continue;
		}

		if (childRaw->ruleName == "implements_opt")
		{
			FlattenQualifiedTypeList(childRaw, implementedInterfaces);
			continue;
		}
		if (childRaw->ruleName == "contract_list")
		{
			FlattenContracts(childRaw, contracts);
		}
	}

	auto decl = std::make_unique<StructDeclNode>(
		name,
		std::move(typeParams),
		std::move(fields),
		std::move(implementedInterfaces),
		std::move(methods),
		std::move(contracts));
	return decl;
}

ASTNodePtr ASTBuilder::SimplifyEnumDecl(const RawNode& raw)
{
	std::string name;
	std::vector<TypeParameterDecl> typeParams;
	std::vector<EnumVariantDecl> variants;

	for (auto& child : raw.children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if ((leaf->type == "identifier" || leaf->value == "print") && name.empty())
			{
				name = leaf->value;
			}
			continue;
		}

		auto* childRaw = dynamic_cast<RawNode*>(child.get());
		if (childRaw && childRaw->ruleName == "type_params_opt")
		{
			FlattenTypeParams(childRaw, typeParams);
		}
		else if (childRaw && childRaw->ruleName == "variant_list")
		{
			FlattenEnumVariants(childRaw, variants);
		}
	}

	return std::make_unique<EnumDeclNode>(name, std::move(typeParams), std::move(variants));
}

ASTNodePtr ASTBuilder::SimplifyInterfaceDecl(const RawNode& raw)
{
	std::string name;
	std::vector<TypeParameterDecl> typeParams;
	std::vector<InterfaceMethodSig> methods;

	for (auto& child : raw.children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if ((leaf->type == "identifier" || leaf->value == "print") && name.empty())
			{
				name = leaf->value;
			}
			continue;
		}

		if (const auto* childRaw = dynamic_cast<RawNode*>(child.get());
			childRaw && childRaw->ruleName == "type_params_opt")
		{
			FlattenTypeParams(childRaw, typeParams);
		}
		else if (childRaw && childRaw->ruleName == "method_sig_list")
		{
			FlattenInterfaceMethods(const_cast<RawNode*>(childRaw), methods);
		}
	}

	return std::make_unique<InterfaceDeclNode>(name, std::move(typeParams), std::move(methods));
}

ASTNodePtr ASTBuilder::SimplifyEffectDecl(const RawNode& raw)
{
	std::string name;
	std::vector<InterfaceMethodSig> operations;

	for (auto& child : raw.children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if ((leaf->type == "identifier" || leaf->value == "print") && name.empty())
			{
				name = leaf->value;
			}
			continue;
		}

		if (const auto* childRaw = dynamic_cast<RawNode*>(child.get());
			childRaw && childRaw->ruleName == "effect_body_opt" && childRaw->children.size() >= 2)
		{
			FlattenInterfaceMethods(dynamic_cast<RawNode*>(childRaw->children[1].get()), operations);
		}
	}

	return std::make_unique<EffectDeclNode>(name, std::move(operations));
}

ASTNodePtr ASTBuilder::SimplifyActorDecl(const RawNode& raw)
{
	std::string name;
	std::vector<TypeParameterDecl> typeParams;
	std::vector<ActorFieldDecl> fields;
	std::vector<ActorMethodDecl> methods;

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

		if (childRaw->ruleName == "type_params_opt")
		{
			FlattenTypeParams(childRaw, typeParams);
		}
		else if (childRaw->ruleName == "actor_body")
		{
			FlattenActorBody(childRaw, fields, methods);
		}
	}

	return std::make_unique<ActorDeclNode>(
		name,
		std::move(typeParams),
		std::move(fields),
		std::move(methods));
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
			block->statements.push_back(std::move(item));
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

void ASTBuilder::FlattenContextBindings(const RawNode* raw, std::vector<ContextBinding>& target)
{
	if (!raw)
	{
		return;
	}

	if (raw->ruleName == "context_req_opt")
	{
		if (raw->children.size() >= 3)
		{
			FlattenContextBindings(dynamic_cast<const RawNode*>(raw->children[2].get()), target);
		}
		return;
	}

	if (raw->ruleName == "context_binding_list")
	{
		if (!raw->children.empty())
		{
			FlattenContextBindings(dynamic_cast<const RawNode*>(raw->children[0].get()), target);
		}
		if (raw->children.size() > 1)
		{
			FlattenContextBindings(dynamic_cast<const RawNode*>(raw->children[1].get()), target);
		}
		return;
	}

	if (raw->ruleName == "context_binding_list_tail")
	{
		if (raw->children.size() >= 2)
		{
			FlattenContextBindings(dynamic_cast<const RawNode*>(raw->children[1].get()), target);
		}
		if (raw->children.size() > 2)
		{
			FlattenContextBindings(dynamic_cast<const RawNode*>(raw->children[2].get()), target);
		}
		return;
	}

	if (raw->ruleName != "context_binding")
	{
		return;
	}

	ContextBinding binding;
	for (const auto& child : raw->children)
	{
		if (const auto* leaf = dynamic_cast<const LeafNode*>(child.get()))
		{
			if (leaf->type == "identifier" && binding.name.empty())
			{
				binding.name = leaf->value;
			}
		}
		else if (const auto* childRaw = dynamic_cast<const RawNode*>(child.get()))
		{
			if (childRaw->ruleName == "dataType")
			{
				binding.typeName = ExtractType(childRaw);
			}
		}
	}

	if (!binding.name.empty())
	{
		target.push_back(std::move(binding));
	}
}

void ASTBuilder::FlattenRaisedEffects(const RawNode* raw, std::vector<std::string>& target)
{
	if (!raw)
	{
		return;
	}

	if (raw->ruleName == "effect_spec_opt")
	{
		if (raw->children.size() >= 3)
		{
			FlattenRaisedEffects(dynamic_cast<const RawNode*>(raw->children[2].get()), target);
		}
		return;
	}

	if (raw->ruleName == "effect_type_list")
	{
		if (!raw->children.empty())
		{
			FlattenRaisedEffects(dynamic_cast<const RawNode*>(raw->children[0].get()), target);
		}
		if (raw->children.size() > 1)
		{
			FlattenRaisedEffects(dynamic_cast<const RawNode*>(raw->children[1].get()), target);
		}
		return;
	}

	if (raw->ruleName == "effect_type_list_tail")
	{
		if (raw->children.size() >= 2)
		{
			FlattenRaisedEffects(dynamic_cast<const RawNode*>(raw->children[1].get()), target);
		}
		if (raw->children.size() > 2)
		{
			FlattenRaisedEffects(dynamic_cast<const RawNode*>(raw->children[2].get()), target);
		}
		return;
	}

	if (raw->ruleName != "effect_type")
	{
		return;
	}

	for (const auto& child : raw->children)
	{
		if (const auto* leaf = dynamic_cast<const LeafNode*>(child.get()))
		{
			if (leaf->type == "identifier")
			{
				target.push_back(leaf->value);
				break;
			}
		}
	}
}
