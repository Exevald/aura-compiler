#include "../common/ASTBuilderSupport.h"
#include "ASTBuilder.h"

#include <algorithm>
#include <functional>

using ASTBuilderDetail::ExtractQualifiedId;
using ASTBuilderDetail::ExtractType;

void ASTBuilder::FlattenParams(RawNode* raw, std::vector<Parameter>& target)
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
				if (p->ruleName == "parameter" && !p->children.empty())
				{
					p = dynamic_cast<RawNode*>(p->children[0].get());
				}
				if (!p)
				{
					return;
				}

				const bool isVariadic = p->ruleName == "variadic_param";
				const size_t nameIndex = isVariadic ? 1 : 0;
				const size_t typeIndex = isVariadic ? 3 : 1;
				const auto* nameLeaf = p->children.size() > nameIndex
					? dynamic_cast<LeafNode*>(p->children[nameIndex].get())
					: nullptr;
				if (!nameLeaf)
				{
					return;
				}
				const std::string pName = nameLeaf->value;
				std::string pType = isVariadic ? ExtractType(dynamic_cast<RawNode*>(p->children[typeIndex].get())) : "auto";
				std::shared_ptr<ASTNode> defaultValue = nullptr;
				if (!isVariadic && p->children.size() > 1)
				{
					pType = ExtractType(dynamic_cast<RawNode*>(p->children[1].get()));
				}
				if (!isVariadic && p->children.size() > 2)
				{
					if (auto* assignRaw = dynamic_cast<RawNode*>(p->children[2].get());
						assignRaw && assignRaw->ruleName == "assign_expr_opt" && assignRaw->children.size() >= 2)
					{
						auto simplifiedDefault = Simplify(std::move(assignRaw->children[1]));
						defaultValue = std::shared_ptr<ASTNode>(std::move(simplifiedDefault));
					}
				}
				target.emplace_back(pName, pType, std::move(defaultValue), isVariadic);
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
	std::vector<ContextBinding> contextRequirements;
	std::vector<std::string> raisedEffects;
	std::vector<std::unique_ptr<ContractNode>> contracts;
	ASTNodePtr body;
	std::string retType = "void";

	for (auto& child : raw->children)
	{
		if (const auto* l = dynamic_cast<LeafNode*>(child.get()))
		{
			if ((l->type == "identifier" || l->value == "print") && name.empty())
			{
				name = l->value;
			}
		}
		if (auto* r = dynamic_cast<RawNode*>(child.get()))
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
			else if (r->ruleName == "param_list_opt")
			{
				FlattenParams(r, params);
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

	StructMethodDecl method(
		name,
		retType,
		std::move(params),
		std::move(contextRequirements),
		std::move(raisedEffects),
		std::move(contracts),
		std::move(body));
	return method;
}

ActorMethodDecl ASTBuilder::SimplifyActorMethod(RawNode* raw)
{
	ActorMethodDecl::Kind kind = ActorMethodDecl::Kind::Message;
	std::string name;
	std::vector<Parameter> params;
	std::vector<ContextBinding> contextRequirements;
	std::vector<std::string> raisedEffects;
	std::vector<std::unique_ptr<ContractNode>> contracts;
	ASTNodePtr body;
	std::string retType = "void";

	for (auto& child : raw->children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if (leaf->value == "query")
			{
				kind = ActorMethodDecl::Kind::Query;
			}
			else if ((leaf->type == "identifier" || leaf->value == "print") && name.empty())
			{
				name = leaf->value;
			}
		}
		else if (auto* r = dynamic_cast<RawNode*>(child.get()))
		{
			if (r->ruleName == "msg_or_query")
			{
				for (const auto& kindChild : r->children)
				{
					if (const auto* kindLeaf = dynamic_cast<const LeafNode*>(kindChild.get());
						kindLeaf && kindLeaf->value == "query")
					{
						kind = ActorMethodDecl::Kind::Query;
					}
				}
			}
			else if (r->ruleName == "function_name" && !r->children.empty() && name.empty())
			{
				if (const auto* nameLeaf = dynamic_cast<LeafNode*>(r->children[0].get()))
				{
					name = nameLeaf->value;
				}
			}
			else if (r->ruleName == "type_guide_opt" && r->children.size() >= 2)
			{
				retType = ExtractType(r);
			}
			else if (r->ruleName == "param_list_opt")
			{
				FlattenParams(r, params);
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

	ActorMethodDecl method(
		kind,
		name,
		retType,
		std::move(params),
		std::move(contextRequirements),
		std::move(raisedEffects),
		std::move(contracts),
		std::move(body));
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
		FlattenStructMembers(
			dynamic_cast<RawNode*>(raw->children[0].get()),
			fields,
			methods);
		if (raw->children.size() > 1)
		{
			FlattenStructMembers(
				dynamic_cast<RawNode*>(raw->children[1].get()),
				fields,
				methods);
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

void ASTBuilder::FlattenActorBody(
	const RawNode* raw,
	std::vector<ActorFieldDecl>& fields,
	std::vector<ActorMethodDecl>& methods)
{
	if (!raw || raw->ruleName == "EPSILON" || raw->children.empty())
	{
		return;
	}

	if (raw->ruleName == "actor_body"
		|| raw->ruleName == "actor_field_list"
		|| raw->ruleName == "actor_method_list")
	{
		for (const auto& child : raw->children)
		{
			FlattenActorBody(dynamic_cast<RawNode*>(child.get()), fields, methods);
		}
		return;
	}

	if (raw->ruleName == "actor_field")
	{
		ActorFieldDecl field;
		for (const auto& child : raw->children)
		{
			if (const auto* leaf = dynamic_cast<const LeafNode*>(child.get()))
			{
				if (leaf->type == "identifier" && field.name.empty())
				{
					field.name = leaf->value;
				}
			}
			else if (const auto* childRaw = dynamic_cast<const RawNode*>(child.get()))
			{
				if (childRaw->ruleName == "dataType")
				{
					field.typeName = ExtractType(childRaw);
				}
				else if (childRaw->ruleName == "assign_expr_opt" && childRaw->children.size() >= 2)
				{
					field.initializer = Simplify(std::move(const_cast<RawNode*>(childRaw)->children[1]));
				}
			}
		}
		fields.push_back(std::move(field));
		return;
	}

	if (raw->ruleName == "actor_method")
	{
		methods.push_back(SimplifyActorMethod(const_cast<RawNode*>(raw)));
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
		target.push_back(ExtractType(raw->children[0].get()));
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
			target.push_back(ExtractType(raw->children[1].get()));
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
						if (typeNode->ruleName == "dataType_list"
							|| typeNode->ruleName == "dataType_list_tail")
						{
							const int offset = (typeNode->ruleName == "dataType_list") ? 0 : 1;
							if (typeNode->children.size() > offset)
							{
								variant.argTypes.push_back(
									ExtractType(typeNode->children[offset].get()));
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

void ASTBuilder::FlattenContracts(
	const RawNode* raw,
	std::vector<std::unique_ptr<ContractNode>>& target)
{
	if (!raw || raw->ruleName == "EPSILON" || raw->children.empty())
	{
		return;
	}

	if (raw->ruleName == "contract_list")
	{
		FlattenContracts(dynamic_cast<const RawNode*>(raw->children[0].get()), target);
		if (raw->children.size() > 1)
		{
			FlattenContracts(dynamic_cast<const RawNode*>(raw->children[1].get()), target);
		}
		return;
	}

	if (raw->ruleName != "contract" || raw->children.size() < 3)
	{
		return;
	}

	const auto* kindLeaf = dynamic_cast<const LeafNode*>(raw->children[0].get());
	if (!kindLeaf)
	{
		return;
	}

	ContractKind kind = ContractKind::Requires;
	if (kindLeaf->value == "ensures")
	{
		kind = ContractKind::Ensures;
	}
	else if (kindLeaf->value == "invariant")
	{
		kind = ContractKind::Invariant;
	}

	target.push_back(std::make_unique<ContractNode>(
		kind,
		Simplify(std::move(const_cast<RawNode*>(raw)->children[2]))));
}

void ASTBuilder::FlattenInterfaceMethods(RawNode* raw, std::vector<InterfaceMethodSig>& target)
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
			if ((leaf->type == "identifier" || leaf->value == "print") && method.name.empty())
			{
				method.name = leaf->value;
			}
		}
		else if (const auto* childRaw = dynamic_cast<RawNode*>(child.get()))
		{
			if (childRaw->ruleName == "function_name"
				&& !childRaw->children.empty()
				&& method.name.empty())
			{
				if (const auto* nameLeaf = dynamic_cast<LeafNode*>(childRaw->children[0].get()))
				{
					method.name = nameLeaf->value;
				}
			}
			if (childRaw->ruleName == "param_list_opt")
			{
				FlattenParams(const_cast<RawNode*>(childRaw), method.params);
			}
			else if (childRaw->ruleName == "type_guide_opt" && childRaw->children.size() >= 2)
			{
				method.returnType = ExtractType(childRaw);
			}
			else if (childRaw->ruleName == "effect_spec_opt")
			{
				FlattenRaisedEffects(childRaw, method.raisedEffects);
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
		if (const auto* childRaw = dynamic_cast<RawNode*>(child.get());
			childRaw && childRaw->ruleName == "expression")
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

void ASTBuilder::FlattenHandlerCases(RawNode* raw, std::vector<EffectHandlerCase>& target)
{
	if (!raw || raw->ruleName == "EPSILON" || raw->children.empty())
	{
		return;
	}

	if (raw->ruleName == "handler_list")
	{
		for (auto& child : raw->children)
		{
			FlattenHandlerCases(dynamic_cast<RawNode*>(child.get()), target);
		}
		return;
	}

	if (raw->ruleName != "handler")
	{
		return;
	}

	std::string effectName;
	std::vector<Parameter> params;
	ASTNodePtr body;

	for (auto& child : raw->children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if (leaf->type == "identifier" && effectName.empty())
			{
				effectName = leaf->value;
			}
		}
		else if (auto* childRaw = dynamic_cast<RawNode*>(child.get()))
		{
			if (childRaw->ruleName == "param_list_opt")
			{
				FlattenParams(childRaw, params);
			}
			else if (childRaw->ruleName == "block_stmt")
			{
				body = Simplify(std::move(child));
			}
		}
	}

	target.emplace_back(effectName, std::move(params), std::move(body));
}
