#include "BytecodeGenerator.h"

#include <memory>

namespace
{

ASTNode* FindChild(const InternalNode& node, const std::string& lhs)
{
	for (auto& child : node.children)
	{
		if (auto* i = dynamic_cast<InternalNode*>(child.get()))
		{
			if (i->ruleLhs == lhs)
			{
				return i;
			}
		}
	}
	return nullptr;
}

} // namespace

VM::Execution::Chunk BytecodeGenerator::Compile(ASTNode* root)
{
	m_symbols.Reset();
	m_functionStack.clear();

	m_currentFunction = std::make_shared<VM::Core::Function>();
	m_currentFunction->name = "top_level";

	if (root)
	{
		root->Accept(*this);
	}

	CurrentChunk().Write(VM::Core::OpCode::OP_RETURN);
	return *m_currentFunction->chunk;
}

void BytecodeGenerator::Visit(LeafNode& node)
{
	using namespace VM::Core;

	if (node.type == "integer_literal")
	{
		CurrentChunk().WriteConstant(std::stoll(node.value));
	}
	else if (node.type == "float_literal")
	{
		CurrentChunk().WriteConstant(std::stod(node.value));
	}
	else if (node.type == "identifier")
	{
		if (const auto idx = m_symbols.Resolve(node.value); idx.has_value())
		{
			CurrentChunk().Write(OpCode::OP_GET_LOCAL);
			CurrentChunk().code.push_back(idx.value());
		}
		else
		{
			auto stringPtr = std::make_shared<const std::string>(node.value);
			const uint8_t nameIdx = CurrentChunk().AddConstant(stringPtr);
			CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
			CurrentChunk().code.push_back(nameIdx);
		}
	}
	else if (node.type == "string_literal")
	{
		std::string content = node.value.substr(1, node.value.size() - 2);
		auto stringPtr = std::make_shared<const std::string>(content);
		CurrentChunk().WriteConstant(stringPtr);
	}
}

void BytecodeGenerator::Visit(InternalNode& node)
{
	using namespace VM::Core;

	if (node.ruleLhs == "assignment_expr")
	{
		HandleAssignment(node);
	}
	else if (node.ruleLhs == "iter_stmt")
	{
		HandleIter(node);
	}
	else if (node.ruleLhs == "array_lit")
	{
		uint8_t count = 0;
		for (auto& child : node.children)
		{
			if (auto* i = dynamic_cast<InternalNode*>(child.get()))
			{
				if (i->ruleLhs == "arg_list_opt")
				{
					count = CountAndEmitArgs(i);
				}
			}
		}
		CurrentChunk().Write(OpCode::OP_BUILD_ARRAY);
		CurrentChunk().code.push_back(count);
	}
	else if (node.ruleLhs == "additive"
		|| node.ruleLhs == "multiplicative"
		|| node.ruleLhs == "equality"
		|| node.ruleLhs == "relational")
	{
		if (!node.children.empty())
		{
			node.children[0]->Accept(*this);
		}
		if (node.children.size() > 1)
		{
			node.children[1]->Accept(*this);
		}
	}
	else if (node.ruleLhs == "additive_tail"
		|| node.ruleLhs == "multiplicative_tail"
		|| node.ruleLhs == "equality_tail"
		|| node.ruleLhs == "relational_tail")
	{
		if (!node.children.empty())
		{
			node.children[1]->Accept(*this);
			if (auto* op = dynamic_cast<LeafNode*>(node.children[0].get()))
			{
				EmitBinaryOp(op->value);
			}
			if (node.children.size() > 2)
			{
				node.children[2]->Accept(*this);
			}
		}
	}
	else if (node.ruleLhs == "identifier_expr")
	{
		node.children[0]->Accept(*this);
		for (size_t i = 1; i < node.children.size(); ++i)
		{
			node.children[i]->Accept(*this);
		}
	}
	else if (node.ruleLhs == "trailer")
	{
		if (const auto* first = dynamic_cast<LeafNode*>(node.children[0].get());
			first && first->value == "(")
		{
			uint8_t count = 0;
			for (auto& child : node.children)
			{
				if (auto* i = dynamic_cast<InternalNode*>(child.get()))
				{
					if (i->ruleLhs == "arg_list_opt")
					{
						count = CountAndEmitArgs(i);
					}
				}
			}
			CurrentChunk().Write(OpCode::OP_CALL);
			CurrentChunk().code.push_back(count);
		}
		else if (first && first->value == "[")
		{
			node.children[1]->Accept(*this);
			CurrentChunk().Write(OpCode::OP_INDEX_GET);
		}
	}
	else if (node.ruleLhs == "var_decl_no_semi")
	{
		HandleVarDecl(node);
	}
	else if (node.ruleLhs == "func_decl")
	{
		HandleFunctionDecl(node);
	}
	else if (node.ruleLhs == "return_stmt")
	{
		HandleReturn(node);
	}
	else if (node.ruleLhs == "if_stmt")
	{
		HandleIf(node);
	}
	else if (node.ruleLhs == "while_stmt")
	{
		HandleWhile(node);
	}
	else if (node.ruleLhs == "print_stmt")
	{
		node.children[1]->Accept(*this);
		CurrentChunk().Write(OpCode::OP_PRINT);
	}
	else
	{
		for (auto& child : node.children)
		{
			if (child)
			{
				child->Accept(*this);
			}
		}
	}
}

void BytecodeGenerator::HandleAssignment(const InternalNode& node)
{
	using namespace VM::Core;
	const auto* tail = dynamic_cast<InternalNode*>(FindChild(node, "assignment_tail"));
	if (!tail || tail->children.empty())
	{
		node.children[0]->Accept(*this);
		return;
	}

	ASTNode* rhs = tail->children[1].get();
	const auto* lhsExpr = dynamic_cast<InternalNode*>(node.children[0].get());

	std::string varName;
	const InternalNode* trailerList = nullptr;

	const auto* current = lhsExpr;
	while (current && current->ruleLhs != "identifier_expr")
	{
		if (!current->children.empty())
		{
			current = dynamic_cast<InternalNode*>(current->children[0].get());
		}
		else
		{
			break;
		}
	}

	if (current && current->ruleLhs == "identifier_expr")
	{
		varName = dynamic_cast<LeafNode*>(current->children[0].get())->value;
		if (current->children.size() > 2)
		{
			trailerList = dynamic_cast<InternalNode*>(current->children[2].get());
		}
	}

	bool isArraySet = false;
	if (trailerList && !trailerList->children.empty())
	{
		if (const auto* trailer = dynamic_cast<InternalNode*>(trailerList->children[0].get());
			trailer && dynamic_cast<LeafNode*>(trailer->children[0].get())->value == "[")
		{
			isArraySet = true;

			if (auto sIdx = m_symbols.Resolve(varName))
			{
				CurrentChunk().Write(OpCode::OP_GET_LOCAL);
				CurrentChunk().code.push_back(*sIdx);
			}
			else
			{
				const uint8_t nIdx = CurrentChunk().AddConstant(
					std::make_shared<const std::string>(varName));
				CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
				CurrentChunk().code.push_back(nIdx);
			}
			trailer->children[1]->Accept(*this);
			rhs->Accept(*this);

			CurrentChunk().Write(OpCode::OP_INDEX_SET);
		}
	}

	if (!isArraySet)
	{
		rhs->Accept(*this);
		if (const auto sIdx = m_symbols.Resolve(varName))
		{
			CurrentChunk().Write(OpCode::OP_SET_LOCAL);
			CurrentChunk().code.push_back(*sIdx);
		}
		else
		{
			const uint8_t nIdx = CurrentChunk().AddConstant(
				std::make_shared<const std::string>(varName));
			CurrentChunk().Write(OpCode::OP_SET_GLOBAL);
			CurrentChunk().code.push_back(nIdx);
		}
	}
}

void BytecodeGenerator::HandleVarDecl(InternalNode& node)
{
	using namespace VM::Core;
	std::string name;
	ASTNode* init = nullptr;
	for (auto& c : node.children)
	{
		if (const auto* l = dynamic_cast<LeafNode*>(c.get()))
		{
			if (l->type == "identifier")
			{
				name = l->value;
			}
		}
		if (const auto* i = dynamic_cast<InternalNode*>(c.get()))
		{
			if (i->ruleLhs == "assign_expr_opt" && !i->children.empty())
			{
				init = i->children[1].get();
			}
		}
	}
	if (init)
	{
		init->Accept(*this);
	}
	else
	{
		CurrentChunk().WriteConstant(0LL);
	}
	CurrentChunk().Write(OpCode::OP_SET_LOCAL);
	CurrentChunk().code.push_back(m_symbols.Define(name));
}

void BytecodeGenerator::EmitBinaryOp(const std::string& op) const
{
	using namespace VM::Core;
	if (op == "+")
	{
		CurrentChunk().Write(OpCode::OP_ADD);
	}
	else if (op == "-")
	{
		CurrentChunk().Write(OpCode::OP_SUBTRACT);
	}
	else if (op == "*")
	{
		CurrentChunk().Write(OpCode::OP_MULTIPLY);
	}
	else if (op == "/")
	{
		CurrentChunk().Write(OpCode::OP_DIVIDE);
	}
	else if (op == "==")
	{
		CurrentChunk().Write(OpCode::OP_EQUAL);
	}
	else if (op == "!=")
	{
		CurrentChunk().Write(OpCode::OP_NOT_EQUAL);
	}
	else if (op == "<")
	{
		CurrentChunk().Write(OpCode::OP_LESS);
	}
	else if (op == ">")
	{
		CurrentChunk().Write(OpCode::OP_GREATER);
	}
	else if (op == "<=")
	{
		CurrentChunk().Write(OpCode::OP_LESS_EQUAL);
	}
	else if (op == ">=")
	{
		CurrentChunk().Write(OpCode::OP_GREATER_EQUAL);
	}
	else if (op == "mod" || op == "%")
	{
		CurrentChunk().Write(OpCode::OP_MOD);
	}
	else if (op == "div")
	{
		CurrentChunk().Write(OpCode::OP_DIV);
	}
	else if (op == "and" || op == "&&")
	{
		CurrentChunk().Write(OpCode::OP_AND);
	}
	else if (op == "or" || op == "||")
	{
		CurrentChunk().Write(OpCode::OP_OR);
	}
}

void BytecodeGenerator::HandleIf(const InternalNode& node)
{
	using namespace VM::Core;
	ASTNode *cond = nullptr, *body = nullptr, *elseOpt = nullptr;

	for (auto& child : node.children)
	{
		if (auto* childNode = dynamic_cast<InternalNode*>(child.get()))
		{
			if (childNode->ruleLhs == "expression")
			{
				cond = childNode;
			}
			else if (childNode->ruleLhs == "block_stmt")
			{
				body = childNode;
			}
			else if (childNode->ruleLhs == "else_opt")
			{
				elseOpt = childNode;
			}
		}
	}

	if (cond)
	{
		cond->Accept(*this);
	}

	CurrentChunk().Write(OpCode::OP_JUMP_IF_FALSE);
	const size_t jumpToElseAddr = CurrentChunk().code.size();
	CurrentChunk().code.push_back(0xff);

	if (body)
	{
		body->Accept(*this);
	}

	if (const auto* elseInt = dynamic_cast<InternalNode*>(elseOpt);
		elseInt && elseInt->children.size() >= 2)
	{
		CurrentChunk().Write(OpCode::OP_JUMP);
		const size_t jumpToEndAddr = CurrentChunk().code.size();
		CurrentChunk().code.push_back(0xff);
		CurrentChunk().code[jumpToElseAddr]
			= static_cast<uint8_t>(CurrentChunk().code.size() - jumpToElseAddr - 1);

		elseInt->children[1]->Accept(*this);
		CurrentChunk().code[jumpToEndAddr]
			= static_cast<uint8_t>(CurrentChunk().code.size() - jumpToEndAddr - 1);
	}
	else
	{
		CurrentChunk().code[jumpToElseAddr]
			= static_cast<uint8_t>(CurrentChunk().code.size() - jumpToElseAddr - 1);
	}
}

void BytecodeGenerator::HandleWhile(const InternalNode& node)
{
	using namespace VM::Core;

	const size_t loopStart = CurrentChunk().code.size();

	ASTNode *cond = nullptr, *body = nullptr;
	for (auto& c : node.children)
	{
		if (auto* i = dynamic_cast<InternalNode*>(c.get()))
		{
			if (i->ruleLhs == "expression")
			{
				cond = i;
			}
			else if (i->ruleLhs == "block_stmt")
			{
				body = i;
			}
		}
	}

	if (cond)
	{
		cond->Accept(*this);
	}

	CurrentChunk().Write(OpCode::OP_JUMP_IF_FALSE);
	const size_t exitJumpAddr = CurrentChunk().code.size();
	CurrentChunk().code.push_back(0xff);

	if (body)
	{
		body->Accept(*this);
	}

	CurrentChunk().Write(OpCode::OP_LOOP);
	const size_t offsetToStart = CurrentChunk().code.size() - loopStart + 1;
	CurrentChunk().code.push_back(static_cast<uint8_t>(offsetToStart));
	CurrentChunk().code[exitJumpAddr] = static_cast<uint8_t>(CurrentChunk().code.size() - exitJumpAddr - 1);
}

VM::Execution::Chunk& BytecodeGenerator::CurrentChunk() const
{
	return *m_currentFunction->chunk;
}

void BytecodeGenerator::HandleFunctionDecl(InternalNode& node)
{
	using namespace VM::Core;
	std::string name;
	InternalNode *params = nullptr, *body = nullptr;
	for (auto& c : node.children)
	{
		if (const auto* l = dynamic_cast<LeafNode*>(c.get()))
			if (l->type == "identifier")
			{
				name = l->value;
			}
		if (auto* i = dynamic_cast<InternalNode*>(c.get()))
		{
			if (i->ruleLhs == "param_list_opt" && !i->children.empty())
			{
				params = dynamic_cast<InternalNode*>(i->children[0].get());
			}
			if (i->ruleLhs == "block_stmt")
			{
				body = i;
			}
		}
	}

	const auto prev = m_currentFunction;
	m_currentFunction = std::make_shared<Function>();
	m_currentFunction->name = name;

	m_symbols.PushScope();
	if (params)
	{
		std::function<void(InternalNode*)> collect = [&](InternalNode* l) {
			if (!l)
				return;
			const int pIdx = (l->ruleLhs == "param_list") ? 0 : 1;
			if (l->children.size() > pIdx)
			{
				if (auto* p = dynamic_cast<InternalNode*>(l->children[pIdx].get()))
				{
					m_symbols.Define(dynamic_cast<LeafNode*>(p->children[0].get())->value);
					m_currentFunction->arity++;
				}
			}
			if (l->children.size() > pIdx + 1)
				collect(dynamic_cast<InternalNode*>(l->children.back().get()));
		};
		collect(params);
	}

	if (body)
	{
		body->Accept(*this);
	}
	CurrentChunk().Write(OpCode::OP_RETURN);

	auto finished = m_currentFunction;
	m_symbols.PopScope();
	m_currentFunction = prev;

	uint8_t fIdx = CurrentChunk().AddConstant(finished);
	uint8_t nIdx = CurrentChunk().AddConstant(std::make_shared<const std::string>(name));
	CurrentChunk().Write(OpCode::OP_CONSTANT);
	CurrentChunk().code.push_back(fIdx);
	CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
	CurrentChunk().code.push_back(nIdx);
}

void BytecodeGenerator::HandleCall(const InternalNode& node) const
{
	using namespace VM::Core;
	std::string name = dynamic_cast<LeafNode*>(node.children[0].get())->value;

	constexpr uint8_t argCount = 0;
	auto stringPtr = std::make_shared<const std::string>(name);
	const uint8_t nameIdx = CurrentChunk().AddConstant(stringPtr);
	CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
	CurrentChunk().code.push_back(nameIdx);

	CurrentChunk().Write(OpCode::OP_CALL);
	CurrentChunk().code.push_back(argCount);
}

void BytecodeGenerator::HandleReturn(const InternalNode& node)
{
	bool hasExpr = false;
	for (auto& child : node.children)
	{
		if (auto* i = dynamic_cast<InternalNode*>(child.get()))
		{
			if (i->ruleLhs == "expression_opt" && !i->children.empty())
			{
				i->children[0]->Accept(*this);
				hasExpr = true;
				break;
			}
		}
	}
	if (!hasExpr)
	{
		CurrentChunk().WriteConstant(std::monostate{});
	}
	CurrentChunk().Write(VM::Core::OpCode::OP_RETURN);
}

uint8_t BytecodeGenerator::CountAndEmitArgs(ASTNode* node)
{
	if (!node)
		return 0;
	auto* i = dynamic_cast<InternalNode*>(node);
	if (!i)
		return 0;

	if (i->ruleLhs == "arg_list_opt")
		return i->children.empty() ? 0 : CountAndEmitArgs(i->children[0].get());

	if (i->ruleLhs == "arg_list")
	{
		i->children[0]->Accept(*this);
		return 1 + (i->children.size() > 1 ? CountAndEmitArgs(i->children[1].get()) : 0);
	}
	if (i->ruleLhs == "arg_list_tail" && i->children.size() >= 2)
	{
		i->children[1]->Accept(*this);
		return 1 + (i->children.size() > 2 ? CountAndEmitArgs(i->children[2].get()) : 0);
	}
	return 0;
}

void BytecodeGenerator::HandleIter(InternalNode& node)
{
	using namespace VM::Core;
	std::string varName;
	ASTNode *collection = nullptr, *body = nullptr;

	for (auto& c : node.children)
	{
		if (const auto* leaf = dynamic_cast<LeafNode*>(c.get()))
		{
			if (leaf->type == "identifier")
				varName = leaf->value;
		}
		if (auto* i = dynamic_cast<InternalNode*>(c.get()))
		{
			if (i->ruleLhs == "expression")
			{
				collection = i;
			}
			else if (i->ruleLhs == "block_stmt")
			{
				body = i;
			}
		}
	}
	collection->Accept(*this);
	CurrentChunk().Write(OpCode::OP_MAKE_ITER);

	const size_t loopStart = CurrentChunk().code.size();

	CurrentChunk().Write(OpCode::OP_ITER_NEXT);
	const size_t exitJumpAddr = CurrentChunk().code.size();
	CurrentChunk().code.push_back(0xff);

	const uint8_t varIdx = m_symbols.Define(varName);
	CurrentChunk().Write(OpCode::OP_SET_LOCAL);
	CurrentChunk().code.push_back(varIdx);

	body->Accept(*this);

	CurrentChunk().Write(OpCode::OP_LOOP);
	CurrentChunk().code.push_back(static_cast<uint8_t>(loopStart));

	CurrentChunk().code[exitJumpAddr] = static_cast<uint8_t>(CurrentChunk().code.size());

	CurrentChunk().Write(OpCode::OP_POP);
}