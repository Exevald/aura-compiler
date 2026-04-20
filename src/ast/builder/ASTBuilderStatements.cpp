#include "../common/ASTBuilderSupport.h"
#include "ASTBuilder.h"

#include <algorithm>
#include <functional>

using ASTBuilderDetail::ExtractQualifiedId;
using ASTBuilderDetail::ExtractType;

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
				std::string memberName;
				if (const auto* memberLeaf = dynamic_cast<LeafNode*>(trailer->children[1].get()))
				{
					memberName = memberLeaf->value;
				}
				else if (const auto* memberRaw = dynamic_cast<RawNode*>(trailer->children[1].get()))
				{
					if (!memberRaw->children.empty())
					{
						if (const auto* nestedLeaf = dynamic_cast<LeafNode*>(memberRaw->children[0].get()))
						{
							memberName = nestedLeaf->value;
						}
					}
				}
				expr = std::make_unique<MemberAccessNode>(
					std::move(expr),
					memberName);
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

	return std::make_unique<IterNode>(
		var,
		std::move(coll),
		std::move(adapters),
		std::move(body));
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
	if (const auto* leafNode = dynamic_cast<LeafNode*>(raw.children[0].get()))
	{
		if (leafNode->value == "comptime")
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
		if (const auto* childRaw = dynamic_cast<RawNode*>(child.get());
			childRaw && childRaw->ruleName == "block_stmt")
		{
			return std::make_unique<UnsafeNode>(Simplify(std::move(child)));
		}
	}
	return nullptr;
}

ASTNodePtr ASTBuilder::SimplifyTransactionStmt(RawNode& raw)
{
	bool usesSharedRegion = false;
	std::string regionName;
	ASTNodePtr body;

	for (auto& child : raw.children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(child.get()))
		{
			if (leaf->value == "shared")
			{
				usesSharedRegion = true;
			}
			else if (leaf->type == "identifier")
			{
				regionName = leaf->value;
			}
		}
		else if (auto* childRaw = dynamic_cast<RawNode*>(child.get()))
		{
			if (childRaw->ruleName == "region_expr")
			{
				for (const auto& regionChild : childRaw->children)
				{
					if (const auto* regionLeaf = dynamic_cast<const LeafNode*>(regionChild.get()))
					{
						if (regionLeaf->value == "shared")
						{
							usesSharedRegion = true;
						}
						else if (regionLeaf->type == "identifier")
						{
							regionName = regionLeaf->value;
						}
					}
				}
			}
			else if (childRaw->ruleName == "block_stmt")
			{
				body = Simplify(std::move(child));
			}
		}
	}

	return std::make_unique<TransactionNode>(usesSharedRegion, std::move(regionName), std::move(body));
}

ASTNodePtr ASTBuilder::SimplifyHandleStmt(RawNode& raw)
{
	ASTNodePtr expression;
	std::vector<EffectHandlerCase> handlers;

	for (auto& child : raw.children)
	{
		if (auto* childRaw = dynamic_cast<RawNode*>(child.get()))
		{
			if (childRaw->ruleName == "expression")
			{
				expression = Simplify(std::move(child));
			}
			else if (childRaw->ruleName == "handler_list")
			{
				FlattenHandlerCases(childRaw, handlers);
			}
		}
	}

	return std::make_unique<HandleNode>(std::move(expression), std::move(handlers));
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
			return std::make_unique<UnaryExprNode>(
				opLeaf->value,
				Simplify(std::move(raw.children[1])));
		}
	}

	for (auto& child : raw.children)
	{
		child = Simplify(std::move(child));
	}
	return nullptr;
}