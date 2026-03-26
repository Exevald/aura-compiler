#include "ASTBuilder.h"

#include <algorithm>

ASTNodePtr ASTBuilder::Build(ASTNodePtr node)
{
	auto result = Simplify(std::move(node));
	return result ? std::move(result) : std::make_unique<BlockNode>();
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

	if (rule == "func_decl")
	{
		return SimplifyFuncDecl(raw);
	}
	if (rule == "program"
		|| rule == "top_level_list"
		|| rule == "statement_list"
		|| rule == "block_stmt")
	{
		return SimplifyBlockChain(raw);
	}
	if (rule == "var_decl_no_semi")
	{
		return SimplifyVarDeclNoSemi(raw);
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
	if (rule == "identifier_expr")
	{
		return SimplifyIdentifierExpr(raw);
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
	if (raw.children.size() == 1)
	{
		return Simplify(std::move(raw.children[0]));
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
	std::vector<Parameter> params;
	ASTNodePtr body = nullptr;
	std::vector<ASTNodePtr> metadata;

	for (auto& child : raw.children)
	{
		if (auto* l = dynamic_cast<LeafNode*>(child.get()))
		{
			if (l->type == "identifier" && name.empty())
			{
				name = l->value;
			}
		}
		if (auto* r = dynamic_cast<RawNode*>(child.get()))
		{
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

	auto fn = std::make_unique<FunctionDeclNode>(name, std::move(params), std::move(body));
	fn->metadata = std::move(metadata);
	return fn;
}

ASTNodePtr ASTBuilder::SimplifyBlockChain(RawNode& raw)
{
	auto block = std::make_unique<BlockNode>();
	auto* current = &raw;

	while (current)
	{
		if (current->ruleName == "block_stmt")
		{
			if (current->children.size() >= 3)
			{
				current = dynamic_cast<RawNode*>(current->children[1].get());
			}
			else
			{
				break;
			}
			continue;
		}

		if (current->children.empty())
		{
			break;
		}

		if (auto item = Simplify(std::move(current->children[0])))
		{
			block->statements.push_back(std::move(item));
		}

		current = (current->children.size() > 1)
			? dynamic_cast<RawNode*>(current->children[1].get())
			: nullptr;
	}

	return block;
}

ASTNodePtr ASTBuilder::SimplifyVarDeclNoSemi(RawNode& raw)
{
	std::string name;
	ASTNodePtr init = nullptr;

	for (auto& c : raw.children)
	{
		if (auto* l = dynamic_cast<LeafNode*>(c.get()))
		{
			if (l->type == "identifier")
			{
				name = l->value;
			}
		}
		if (auto* r = dynamic_cast<RawNode*>(c.get()))
		{
			if (r->ruleName == "assign_expr_opt" && r->children.size() >= 2)
			{
				init = Simplify(std::move(r->children[1]));
			}
		}
	}

	return std::make_unique<VarDeclNode>(name, std::move(init));
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

ASTNodePtr ASTBuilder::SimplifyIdentifierExpr(RawNode& raw)
{
	std::string name = dynamic_cast<LeafNode*>(raw.children[0].get())->value;
	if (raw.children.size() < 2)
	{
		return std::make_unique<IdentifierNode>(name);
	}

	auto* trailers = dynamic_cast<RawNode*>(raw.children[1].get());
	if (!trailers || trailers->children.empty())
	{
		return std::make_unique<IdentifierNode>(name);
	}

	if (auto* firstT = dynamic_cast<RawNode*>(trailers->children[0].get());
		firstT && !firstT->children.empty())
	{
		std::string type = dynamic_cast<LeafNode*>(firstT->children[0].get())->value;
		if (type == "(")
		{
			std::vector<ASTNodePtr> args;
			if (firstT->children.size() >= 2)
			{
				FlattenArgs(dynamic_cast<RawNode*>(firstT->children[1].get()), args);
			}
			return std::make_unique<CallNode>(name, std::move(args));
		}
		if (type == "[")
		{
			return std::make_unique<IndexNode>(
				std::make_unique<IdentifierNode>(name),
				Simplify(std::move(firstT->children[1])));
		}
	}

	return std::make_unique<IdentifierNode>(name);
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

	for (auto& c : raw.children)
	{
		if (auto* l = dynamic_cast<LeafNode*>(c.get()))
		{
			if (l->type == "identifier")
			{
				var = l->value;
			}
		}
		if (auto* r = dynamic_cast<RawNode*>(c.get()))
		{
			if (r->ruleName == "expression")
			{
				coll = Simplify(std::move(c));
			}
			if (r->ruleName == "block_stmt")
			{
				body = Simplify(std::move(c));
			}
		}
	}

	return std::make_unique<IterNode>(var, std::move(coll), std::move(body));
}

ASTNodePtr ASTBuilder::SimplifyArrayLit(RawNode& raw)
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
	if (raw->ruleName == "param_list"
		|| raw->ruleName == "param_list_tail")
	{
		const int offset = (raw->ruleName == "param_list") ? 0 : 1;
		if (raw->children.size() > offset)
		{
			if (auto* p = dynamic_cast<RawNode*>(raw->children[offset].get()))
			{
				if (!p->children.empty())
				{
					target.push_back({ dynamic_cast<LeafNode*>(p->children[0].get())->value });
				}
			}
		}
		if (raw->children.size() > offset + 1)
		{
			FlattenParams(dynamic_cast<RawNode*>(raw->children.back().get()), target);
		}
	}
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