#include "../../ast/sematic/SemanticAnalyzer.h"
#include "../BytecodeGenerator.h"

#include <functional>
#include <memory>
#include <stdexcept>

using namespace VM::Core;

#include "../BytecodeGeneratorSupport.h"

using BytecodeGeneratorDetail::ScopeExit;
using BytecodeGeneratorDetail::ShouldPopStatementResult;

void BytecodeGenerator::Visit(BlockNode& node)
{
	uint8_t varsInBlock = 0;
	PushContextScope(CurrentContext());
	ScopeExit scopeExit([this, &varsInBlock]() {
		for (uint8_t i = 0; i < varsInBlock; ++i)
		{
			CurrentChunk().Write(OpCode::OP_POP);
		}
		PopContextScope(CurrentContext());
	});

	for (const auto& stmt : node.statements)
	{
		if (!stmt)
		{
			continue;
		}

		if (dynamic_cast<VarDeclNode*>(stmt.get()))
		{
			++varsInBlock;
		}

		stmt->Accept(*this);

		if (ShouldPopStatementResult(stmt.get()))
		{
			CurrentChunk().Write(OpCode::OP_POP);
		}
	}
}

void BytecodeGenerator::Visit(ExportDeclNode& node)
{
	if (node.declaration)
	{
		node.declaration->Accept(*this);
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
	const std::string exportedName = QualifyName(node.name);
	RegisterFunctionSignature(exportedName, node.params);
	CompileFunctionBody(
		exportedName,
		node.params,
		node.body.get(),
		[this, &exportedName]() {
			const uint8_t nameIndex = CurrentChunk().AddConstant(
				std::make_shared<const std::string>(exportedName));
			CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
			CurrentChunk().code.push_back(nameIndex);
		});
}

void BytecodeGenerator::Visit(FunctionExprNode& node)
{
	std::string name = node.name;
	if (name.empty())
	{
		name = "<lambda_" + std::to_string(++m_lambdaCounter) + ">";
	}

	CompileFunctionBody(name, node.params, node.body.get(), []() {});
}

void BytecodeGenerator::Visit(CallNode& node)
{
	AccessEmitter{ *this }.EmitCall(node);
}

void BytecodeGenerator::Visit(MemberAccessNode& node)
{
	AccessEmitter{ *this }.EmitMemberAccess(node);
}

void BytecodeGenerator::Visit(ModuleDeclNode& node)
{
	m_currentModule = node.qualifiedName;
}

void BytecodeGenerator::Visit(ImportDeclNode& node)
{
	m_metadata.importAliases[node.alias.empty()
			? DefaultImportAlias(node.qualifiedName)
			: node.alias] = node.qualifiedName;

	auto module = std::make_shared<Module>();
	module->name = node.qualifiedName;
	CurrentChunk().WriteConstant(module);

	const uint8_t localIndex = CurrentContext().symbols.Define(
		node.alias.empty() ? DefaultImportAlias(node.qualifiedName) : node.alias);
	CurrentChunk().Write(OpCode::OP_SET_LOCAL);
	CurrentChunk().code.push_back(localIndex);
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

void BytecodeGenerator::Visit(UnsafeNode& node)
{
	if (node.body)
	{
		node.body->Accept(*this);
	}
}

void BytecodeGenerator::Visit(TransactionNode& node)
{
	if (node.usesSharedRegion)
	{
		EmitGetVariable("__txn_mutex." + node.regionName);
	}
	else
	{
		EmitGetVariable(node.regionName);
	}
	CurrentChunk().Write(OpCode::OP_BEGIN_TXN);
	if (node.body)
	{
		node.body->Accept(*this);
	}
	CurrentChunk().Write(OpCode::OP_END_TXN);
}

void BytecodeGenerator::Visit(HandleNode& node)
{
	for (const auto& handler : node.handlers)
	{
		std::string handlerName = "<handler_"
			+ handler.effectName
			+ "_"
			+ std::to_string(++m_lambdaCounter)
			+ ">";
		std::vector<Parameter> params = handler.params;
		CompileFunctionBody(handlerName, params, handler.body.get(), []() {});

		const uint8_t opIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(handler.effectName));
		CurrentChunk().Write(OpCode::OP_CONSTANT);
		CurrentChunk().code.push_back(opIndex);
	}

	CurrentChunk().Write(OpCode::OP_BUILD_HANDLER);
	CurrentChunk().code.push_back(static_cast<uint8_t>(node.handlers.size()));
	CurrentChunk().Write(OpCode::OP_PUSH_HANDLER);
	if (node.expression)
	{
		node.expression->Accept(*this);
	}
	else
	{
		CurrentChunk().WriteConstant(std::monostate{});
	}
	CurrentChunk().Write(OpCode::OP_POP_HANDLER);
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
	if (InferEnumTypeName(node.container.get()))
	{
		const auto* literalIndex = dynamic_cast<const IntegerLiteralNode*>(node.index.get());
		if (!literalIndex || literalIndex->value < 0 || literalIndex->value > 255)
		{
			throw std::runtime_error("Enum argument access requires a non-negative integer literal index");
		}

		node.container->Accept(*this);
		CurrentChunk().Write(OpCode::OP_GET_ENUM_ARG);
		CurrentChunk().code.push_back(static_cast<uint8_t>(literalIndex->value));
		return;
	}

	node.container->Accept(*this);
	node.index->Accept(*this);
	CurrentChunk().Write(OpCode::OP_INDEX_GET);
}

void BytecodeGenerator::Visit(IterNode& node)
{
	node.collection->Accept(*this);
	CurrentChunk().Write(OpCode::OP_MAKE_ITER);

	for (const auto& adapter : node.adapters)
	{
		switch (adapter.kind)
		{
		case IterAdapterKind::Drop:
			adapter.argument->Accept(*this);
			CurrentChunk().Write(OpCode::OP_ITER_DROP);
			break;
		case IterAdapterKind::Take:
			adapter.argument->Accept(*this);
			CurrentChunk().Write(OpCode::OP_ITER_TAKE);
			break;
		case IterAdapterKind::Reverse:
			CurrentChunk().Write(OpCode::OP_ITER_REVERSE);
			break;
		case IterAdapterKind::Filter:
			adapter.argument->Accept(*this);
			CurrentChunk().Write(OpCode::OP_ITER_FILTER);
			break;
		case IterAdapterKind::Transform:
			adapter.argument->Accept(*this);
			CurrentChunk().Write(OpCode::OP_ITER_TRANSFORM);
			break;
		}
	}

	const size_t loopStart = CurrentChunk().code.size();
	const size_t exitJump = EmitJump(OpCode::OP_ITER_NEXT);

	const uint8_t varIdx = CurrentContext().symbols.Define(node.varName);
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
	if (node.type == "true")
	{
		CurrentChunk().WriteConstant(true);
		return;
	}
	if (node.type == "false")
	{
		CurrentChunk().WriteConstant(false);
		return;
	}
	if (node.type == "null")
	{
		CurrentChunk().WriteConstant(std::monostate{});
	}
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
