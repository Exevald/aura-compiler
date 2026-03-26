#include "BytecodeGenerator.h"
#include "../ast/SemanticAnalyzer.h"

using namespace VM::Core;

VM::Execution::Chunk BytecodeGenerator::Compile(ASTNode* root)
{
	SemanticAnalyzer analyzer;
	analyzer.Analyze(root);

	m_symbols.Reset();
	m_currentFunction = std::make_shared<Function>();
	m_currentFunction->name = "top_level";

	if (root)
	{
		root->Accept(*this);
	}

	CurrentChunk().Write(OpCode::OP_RETURN);
	return *m_currentFunction->chunk;
}

VM::Execution::Chunk& BytecodeGenerator::CurrentChunk() const
{
	return *m_currentFunction->chunk;
}

size_t BytecodeGenerator::EmitJump(OpCode opcode) const
{
	CurrentChunk().Write(opcode);
	CurrentChunk().code.push_back(0xff);
	CurrentChunk().code.push_back(0xff);
	return CurrentChunk().code.size() - 2;
}

void BytecodeGenerator::PatchJump(size_t jumpAddr) const
{
	const auto jumpDist = static_cast<uint16_t>(CurrentChunk().code.size() - jumpAddr - 2);
	CurrentChunk().code[jumpAddr] = (jumpDist >> 8) & 0xff;
	CurrentChunk().code[jumpAddr + 1] = jumpDist & 0xff;
}

void BytecodeGenerator::Visit(IntegerLiteralNode& node)
{
	CurrentChunk().WriteConstant(node.value);
}

void BytecodeGenerator::Visit(FloatLiteralNode& node)
{
	CurrentChunk().WriteConstant(node.value);
}

void BytecodeGenerator::Visit(StringLiteralNode& node)
{
	auto strPtr = std::make_shared<const std::string>(node.value);
	CurrentChunk().WriteConstant(strPtr);
}

void BytecodeGenerator::Visit(IdentifierNode& node)
{
	if (const auto idx = m_symbols.Resolve(node.name))
	{
		CurrentChunk().Write(OpCode::OP_GET_LOCAL);
		CurrentChunk().code.push_back(*idx);
	}
	else
	{
		const uint8_t nIdx = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(node.name));
		CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
		CurrentChunk().code.push_back(nIdx);
	}
}

void BytecodeGenerator::Visit(BinaryExprNode& node)
{
	node.left->Accept(*this);
	node.right->Accept(*this);
	EmitBinaryOp(node.op);
}

void BytecodeGenerator::Visit(AssignmentNode& node)
{
	if (node.index)
	{
		if (const auto idx = m_symbols.Resolve(node.name))
		{
			CurrentChunk().Write(OpCode::OP_GET_LOCAL);
			CurrentChunk().code.push_back(*idx);
		}
		else
		{
			uint8_t nIdx = CurrentChunk().AddConstant(std::make_shared<const std::string>(node.name));
			CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
			CurrentChunk().code.push_back(nIdx);
		}
		node.index->Accept(*this);
		node.value->Accept(*this);
		CurrentChunk().Write(OpCode::OP_INDEX_SET);
	}
	else
	{
		node.value->Accept(*this);
		if (const auto idx = m_symbols.Resolve(node.name))
		{
			CurrentChunk().Write(OpCode::OP_SET_LOCAL);
			CurrentChunk().code.push_back(*idx);
		}
		else
		{
			uint8_t nIdx = CurrentChunk().AddConstant(std::make_shared<const std::string>(node.name));
			CurrentChunk().Write(OpCode::OP_SET_GLOBAL);
			CurrentChunk().code.push_back(nIdx);
		}
	}
}

void BytecodeGenerator::Visit(VarDeclNode& node)
{
	if (node.initializer)
	{
		node.initializer->Accept(*this);
	}
	else
	{
		CurrentChunk().WriteConstant(0LL);
	}
	const uint8_t idx = m_symbols.Define(node.name);
	CurrentChunk().Write(OpCode::OP_SET_LOCAL);
	CurrentChunk().code.push_back(idx);
}

void BytecodeGenerator::Visit(BlockNode& node)
{
	for (const auto& stmt : node.statements)
	{
		stmt->Accept(*this);
	}
}

void BytecodeGenerator::Visit(IfStatementNode& node)
{
	node.condition->Accept(*this);
	const size_t elseJump = EmitJump(OpCode::OP_JUMP_IF_FALSE);

	node.thenBlock->Accept(*this);

	if (node.elseBlock)
	{
		const size_t endJump = EmitJump(OpCode::OP_JUMP);
		PatchJump(elseJump);
		node.elseBlock->Accept(*this);
		PatchJump(endJump);
	}
	else
	{
		PatchJump(elseJump);
	}
}

void BytecodeGenerator::Visit(WhileStatementNode& node)
{
	const size_t loopStart = CurrentChunk().code.size();
	node.condition->Accept(*this);
	const size_t exitJump = EmitJump(OpCode::OP_JUMP_IF_FALSE);

	node.body->Accept(*this);

	const auto offset = static_cast<uint16_t>(CurrentChunk().code.size() - loopStart + 3);
	CurrentChunk().Write(OpCode::OP_LOOP);
	CurrentChunk().code.push_back((offset >> 8) & 0xff);
	CurrentChunk().code.push_back(offset & 0xff);

	PatchJump(exitJump);
}

void BytecodeGenerator::Visit(FunctionDeclNode& node)
{
	const auto prevFunc = m_currentFunction;
	m_currentFunction = std::make_shared<Function>();
	m_currentFunction->name = node.name;

	m_symbols.PushScope();
	for (auto& p : node.params)
	{
		m_symbols.Define(p.name);
		m_currentFunction->arity++;
	}

	node.body->Accept(*this);
	CurrentChunk().Write(OpCode::OP_RETURN);

	auto finished = m_currentFunction;
	m_symbols.PopScope();
	m_currentFunction = prevFunc;

	const uint8_t nIdx = CurrentChunk().AddConstant(std::make_shared<const std::string>(node.name));
	CurrentChunk().WriteConstant(finished);
	CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
	CurrentChunk().code.push_back(nIdx);
}

void BytecodeGenerator::Visit(CallNode& node)
{
	const uint8_t nIdx = CurrentChunk().AddConstant(std::make_shared<const std::string>(node.callee));
	CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
	CurrentChunk().code.push_back(nIdx);

	for (const auto& arg : node.args)
	{
		arg->Accept(*this);
	}

	CurrentChunk().Write(OpCode::OP_CALL);
	CurrentChunk().code.push_back(static_cast<uint8_t>(node.args.size()));
}

void BytecodeGenerator::Visit(ReturnNode& node)
{
	if (node.value)
	{
		node.value->Accept(*this);
	}
	else
	{
		CurrentChunk().WriteConstant(std::monostate{});
	}
	CurrentChunk().Write(OpCode::OP_RETURN);
}

void BytecodeGenerator::Visit(PrintNode& node)
{
	node.value->Accept(*this);
	CurrentChunk().Write(OpCode::OP_PRINT);
}

void BytecodeGenerator::Visit(ArrayLiteralNode& node)
{
	for (const auto& el : node.elements)
	{
		el->Accept(*this);
	}
	CurrentChunk().Write(OpCode::OP_BUILD_ARRAY);
	CurrentChunk().code.push_back(static_cast<uint8_t>(node.elements.size()));
}

void BytecodeGenerator::Visit(IndexNode& node)
{
	node.container->Accept(*this);
	node.index->Accept(*this);
	CurrentChunk().Write(OpCode::OP_INDEX_GET);
}

void BytecodeGenerator::Visit(IterNode& node)
{
	node.collection->Accept(*this);
	CurrentChunk().Write(OpCode::OP_MAKE_ITER);

	const size_t loopStart = CurrentChunk().code.size();
	const size_t exitJump = EmitJump(OpCode::OP_ITER_NEXT);

	const uint8_t varIdx = m_symbols.Define(node.varName);
	CurrentChunk().Write(OpCode::OP_SET_LOCAL);
	CurrentChunk().code.push_back(varIdx);

	node.body->Accept(*this);

	const auto offset = static_cast<uint16_t>(CurrentChunk().code.size() - loopStart + 3);
	CurrentChunk().Write(OpCode::OP_LOOP);
	CurrentChunk().code.push_back((offset >> 8) & 0xff);
	CurrentChunk().code.push_back(offset & 0xff);

	PatchJump(exitJump);
	CurrentChunk().Write(OpCode::OP_POP);
}

void BytecodeGenerator::Visit(ComptimeNode& node)
{
	if (node.body)
	{
		node.body->Accept(*this);
	}
}

void BytecodeGenerator::Visit(LeafNode& node)
{
}

void BytecodeGenerator::Visit(RawNode& node)
{
	for (auto& child : node.children)
	{
		if (child)
		{
			child->Accept(*this);
		}
	}
}

void BytecodeGenerator::EmitBinaryOp(const std::string& op) const
{
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
}