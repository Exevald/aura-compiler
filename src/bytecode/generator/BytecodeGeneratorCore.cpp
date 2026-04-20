#include "../../ast/sematic/SemanticAnalyzer.h"
#include "../BytecodeGenerator.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>

using namespace VM::Core;

using namespace VM::Core;

namespace
{
class ScopeExit
{
public:
	explicit ScopeExit(std::function<void()> fn)
		: m_fn(std::move(fn))
	{
	}

	ScopeExit(const ScopeExit&) = delete;
	ScopeExit& operator=(const ScopeExit&) = delete;

	~ScopeExit()
	{
		if (m_fn)
		{
			m_fn();
		}
	}

private:
	std::function<void()> m_fn;
};

bool ShouldPopStatementResult(const ASTNode* node)
{
	return dynamic_cast<const BinaryExprNode*>(node)
		|| dynamic_cast<const AssignmentNode*>(node)
		|| dynamic_cast<const CallNode*>(node)
		|| dynamic_cast<const IndexNode*>(node)
		|| dynamic_cast<const IdentifierNode*>(node)
		|| dynamic_cast<const UnaryExprNode*>(node)
		|| dynamic_cast<const IntegerLiteralNode*>(node)
		|| dynamic_cast<const FloatLiteralNode*>(node)
		|| dynamic_cast<const StringLiteralNode*>(node)
		|| dynamic_cast<const FunctionExprNode*>(node)
		|| dynamic_cast<const MemberAccessNode*>(node)
		|| dynamic_cast<const ArrayLiteralNode*>(node);
}

} // namespace

VM::Execution::Chunk BytecodeGenerator::Compile(ASTNode* root)
{
	SemanticAnalyzer analyzer;
	analyzer.Analyze(root);

	m_contexts.clear();
	m_metadata.Clear();
	m_currentModule.clear();
	m_lambdaCounter = 0;

	FunctionContext context;
	context.function = std::make_shared<Function>();
	context.function->name = "top_level";
	context.symbols.Reset();
	context.structVarScopes.emplace_back();
	context.enumVarScopes.emplace_back();
	context.actorVarScopes.emplace_back();
	context.interfaceVarScopes.emplace_back();
	m_contexts.push_back(std::move(context));

	if (root)
	{
		root->Accept(*this);
	}

	CurrentChunk().Write(OpCode::OP_RETURN);
	return *CurrentContext().function->chunk;
}

VM::Execution::Chunk& BytecodeGenerator::CurrentChunk() const
{
	return *CurrentContext().function->chunk;
}

BytecodeGenerator::FunctionContext& BytecodeGenerator::CurrentContext()
{
	return m_contexts.back();
}

const BytecodeGenerator::FunctionContext& BytecodeGenerator::CurrentContext() const
{
	return m_contexts.back();
}

void BytecodeGenerator::InitializeFunctionContext(FunctionContext& context, const std::string& name)
{
	context.function = std::make_shared<Function>();
	context.function->name = name;
	context.symbols.Reset();
	PushContextScope(context);
}

void BytecodeGenerator::PushContextScope(FunctionContext& context)
{
	context.symbols.PushScope();
	context.structVarScopes.emplace_back();
	context.enumVarScopes.emplace_back();
	context.actorVarScopes.emplace_back();
	context.interfaceVarScopes.emplace_back();
}

void BytecodeGenerator::PopContextScope(FunctionContext& context)
{
	context.symbols.PopScope();
	context.structVarScopes.pop_back();
	context.enumVarScopes.pop_back();
	context.actorVarScopes.pop_back();
	context.interfaceVarScopes.pop_back();
}

size_t BytecodeGenerator::EmitJump(const OpCode opcode) const
{
	CurrentChunk().Write(opcode);
	CurrentChunk().code.push_back(0xff);
	CurrentChunk().code.push_back(0xff);
	return CurrentChunk().code.size() - 2;
}

void BytecodeGenerator::PatchJump(const size_t jumpAddr) const
{
	const auto jumpDist = static_cast<uint16_t>(CurrentChunk().code.size() - jumpAddr - 2);
	CurrentChunk().code[jumpAddr] = (jumpDist >> 8) & 0xff;
	CurrentChunk().code[jumpAddr + 1] = jumpDist & 0xff;
}

std::string BytecodeGenerator::QualifyName(const std::string& name) const
{
	if (name.find('.') != std::string::npos)
	{
		return name;
	}
	if (const auto it = m_metadata.importAliases.find(name); it != m_metadata.importAliases.end())
	{
		return it->second;
	}
	if (!m_currentModule.empty())
	{
		return m_currentModule + "." + name;
	}
	return name;
}

std::optional<uint8_t> BytecodeGenerator::ResolveLocal(const std::string& name) const
{
	return CurrentContext().symbols.Resolve(name);
}

std::optional<uint8_t> BytecodeGenerator::ResolveUpvalue(size_t contextIndex, const std::string& name)
{
	if (contextIndex == 0)
	{
		return std::nullopt;
	}

	auto& parent = m_contexts[contextIndex - 1];
	if (const auto local = parent.symbols.Resolve(name))
	{
		auto& current = m_contexts[contextIndex];
		if (const auto existing = current.upvalueLookup.find(name); existing != current.upvalueLookup.end())
		{
			return existing->second;
		}

		const uint8_t index = static_cast<uint8_t>(current.upvalues.size());
		current.upvalues.push_back({ name, true, *local });
		current.upvalueLookup[name] = index;
		current.function->captureNames.push_back(name);
		return index;
	}

	if (const auto parentUpvalue = ResolveUpvalue(contextIndex - 1, name))
	{
		auto& current = m_contexts[contextIndex];
		if (const auto existing = current.upvalueLookup.find(name); existing != current.upvalueLookup.end())
		{
			return existing->second;
		}

		const uint8_t index = static_cast<uint8_t>(current.upvalues.size());
		current.upvalues.push_back({ name, false, *parentUpvalue });
		current.upvalueLookup[name] = index;
		current.function->captureNames.push_back(name);
		return index;
	}

	return std::nullopt;
}

void BytecodeGenerator::EmitGetVariable(const std::string& name)
{
	if (const auto local = ResolveLocal(name))
	{
		CurrentChunk().Write(OpCode::OP_GET_LOCAL);
		CurrentChunk().code.push_back(*local);
		return;
	}

	if (m_contexts.size() > 1)
	{
		if (const auto upvalue = ResolveUpvalue(m_contexts.size() - 1, name))
		{
			CurrentChunk().Write(OpCode::OP_GET_UPVALUE);
			CurrentChunk().code.push_back(*upvalue);
			return;
		}
	}

	if (const auto selfFieldIndex = ResolveSelfFieldIndex(name))
	{
		CurrentChunk().Write(OpCode::OP_GET_LOCAL);
		CurrentChunk().code.push_back(0);
		CurrentChunk().Write(OpCode::OP_MEMBER_GET);
		CurrentChunk().code.push_back(*selfFieldIndex);
		return;
	}

	const uint8_t nameIndex = CurrentChunk().AddConstant(
		std::make_shared<const std::string>(QualifyName(name)));
	CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
	CurrentChunk().code.push_back(nameIndex);
}

void BytecodeGenerator::EmitSetVariable(const std::string& name)
{
	if (const auto local = ResolveLocal(name))
	{
		CurrentChunk().Write(OpCode::OP_SET_LOCAL);
		CurrentChunk().code.push_back(*local);
		return;
	}

	if (m_contexts.size() > 1)
	{
		if (const auto upvalue = ResolveUpvalue(m_contexts.size() - 1, name))
		{
			CurrentChunk().Write(OpCode::OP_SET_UPVALUE);
			CurrentChunk().code.push_back(*upvalue);
			return;
		}
	}

	if (const auto selfFieldIndex = ResolveSelfFieldIndex(name))
	{
		CurrentChunk().Write(OpCode::OP_GET_LOCAL);
		CurrentChunk().code.push_back(0);
		CurrentChunk().Write(OpCode::OP_MEMBER_SET);
		CurrentChunk().code.push_back(*selfFieldIndex);
		return;
	}

	const uint8_t nameIndex = CurrentChunk().AddConstant(
		std::make_shared<const std::string>(QualifyName(name)));
	CurrentChunk().Write(OpCode::OP_SET_GLOBAL);
	CurrentChunk().code.push_back(nameIndex);
}

std::optional<uint8_t> BytecodeGenerator::ResolveSelfFieldIndex(const std::string& member) const
{
	if (CurrentContext().methodSelfStructType)
	{
		const auto layoutIt = m_metadata.structLayouts.find(*CurrentContext().methodSelfStructType);
		if (layoutIt != m_metadata.structLayouts.end())
		{
			for (size_t index = 0; index < layoutIt->second.size(); ++index)
			{
				if (layoutIt->second[index] == member)
				{
					return static_cast<uint8_t>(index);
				}
			}
		}
	}

	if (!CurrentContext().actorSelfType)
	{
		return std::nullopt;
	}

	const auto layoutIt = m_metadata.actorLayouts.find(*CurrentContext().actorSelfType);
	if (layoutIt == m_metadata.actorLayouts.end())
	{
		return std::nullopt;
	}

	for (size_t index = 0; index < layoutIt->second.size(); ++index)
	{
		if (layoutIt->second[index] == member)
		{
			return static_cast<uint8_t>(index);
		}
	}

	return std::nullopt;
}

void BytecodeGenerator::EmitAddressOf(ASTNode* operand)
{
	if (const auto* identifier = dynamic_cast<IdentifierNode*>(operand))
	{
		if (const auto local = ResolveLocal(identifier->name))
		{
			CurrentChunk().Write(OpCode::OP_ADDR_LOCAL);
			CurrentChunk().code.push_back(*local);
			return;
		}
		if (m_contexts.size() > 1 && ResolveUpvalue(m_contexts.size() - 1, identifier->name))
		{
			const auto upvalue = ResolveUpvalue(
				m_contexts.size() - 1, identifier->name);
			CurrentChunk().Write(OpCode::OP_ADDR_UPVALUE);
			CurrentChunk().code.push_back(*upvalue);
			return;
		}

		const uint8_t nameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(QualifyName(identifier->name)));
		CurrentChunk().Write(OpCode::OP_ADDR_GLOBAL);
		CurrentChunk().code.push_back(nameIndex);
		return;
	}

	if (auto* memberAccess = dynamic_cast<MemberAccessNode*>(operand))
	{
		const auto fieldIndex = ResolveStructFieldIndex(memberAccess->object.get(), memberAccess->member);
		if (!fieldIndex)
		{
			throw std::runtime_error("Address-of member requires a struct field");
		}

		memberAccess->object->Accept(*this);
		CurrentChunk().WriteConstant(*fieldIndex);
		CurrentChunk().Write(OpCode::OP_ADDR_MEMBER);
		return;
	}

	if (const auto* indexNode = dynamic_cast<IndexNode*>(operand))
	{
		indexNode->container->Accept(*this);
		indexNode->index->Accept(*this);
		CurrentChunk().Write(OpCode::OP_ADDR_MEMBER);
		return;
	}

	throw std::runtime_error("Address-of requires an assignable operand");
}

void BytecodeGenerator::EmitCallArgument(ASTNode* arg, const bool byRef)
{
	if (byRef)
	{
		EmitAddressOf(arg);
		return;
	}
	arg->Accept(*this);
}

void BytecodeGenerator::RegisterFunctionSignature(const std::string& name, const std::vector<Parameter>& params)
{
	auto& signature = m_metadata.functionSignatures[name];
	signature.refParams.clear();
	signature.refParams.reserve(params.size());
	for (const auto& param : params)
	{
		signature.refParams.push_back(param.typeName.rfind("ref<", 0) == 0);
	}
}

std::vector<bool> BytecodeGenerator::ResolveFunctionRefParams(const ASTNode* callee) const
{
	std::string name;
	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(callee))
	{
		name = QualifyName(identifier->name);
	}
	else if (const auto* memberAccess = dynamic_cast<const MemberAccessNode*>(callee))
	{
		if (const auto* moduleIdentifier = dynamic_cast<const IdentifierNode*>(memberAccess->object.get()))
		{
			if (const auto aliasIt = m_metadata.importAliases.find(moduleIdentifier->name);
				aliasIt != m_metadata.importAliases.end())
			{
				name = aliasIt->second + "." + memberAccess->member;
			}
		}
	}

	if (name.empty())
	{
		return {};
	}
	if (const auto it = m_metadata.functionSignatures.find(name); it != m_metadata.functionSignatures.end())
	{
		return it->second.refParams;
	}
	return {};
}

void BytecodeGenerator::EmitFunctionObject(
	const std::shared_ptr<Function>& function,
	const std::vector<UpvalueInfo>& upvalues) const
{
	const uint8_t functionIndex = CurrentChunk().AddConstant(function);
	for (const auto& upvalue : upvalues)
	{
		if (upvalue.sourceIsLocal)
		{
			CurrentChunk().Write(OpCode::OP_GET_LOCAL);
		}
		else
		{
			CurrentChunk().Write(OpCode::OP_GET_UPVALUE);
		}
		CurrentChunk().code.push_back(upvalue.sourceIndex);
	}
	CurrentChunk().Write(OpCode::OP_CLOSURE);
	CurrentChunk().code.push_back(functionIndex);
}

void BytecodeGenerator::CompileFunctionBody(
	const std::string& name,
	const std::vector<Parameter>& params,
	ASTNode* body,
	const std::function<void()>& emitResult)
{
	FunctionContext context;
	InitializeFunctionContext(context, name);

	for (const auto& [_, typeName] : params)
	{
		context.symbols.Define(_);
		context.function->arity++;
		if (m_metadata.structLayouts.contains(QualifyName(typeName)))
		{
			context.structVarScopes.back()[_] = QualifyName(typeName);
		}
		else if (m_metadata.structLayouts.contains(typeName))
		{
			context.structVarScopes.back()[_] = typeName;
		}
		if (m_metadata.interfaceMethods.contains(QualifyName(typeName)))
		{
			context.interfaceVarScopes.back()[_] = FunctionContext::InterfaceBinding{
				QualifyName(typeName),
				"",
				FunctionContext::InterfaceBinding::RuntimeKind::Unknown
			};
		}
		else if (m_metadata.interfaceMethods.contains(typeName))
		{
			context.interfaceVarScopes.back()[_] = FunctionContext::InterfaceBinding{
				typeName,
				"",
				FunctionContext::InterfaceBinding::RuntimeKind::Unknown
			};
		}
	}

	m_contexts.push_back(std::move(context));
	if (body)
	{
		body->Accept(*this);
	}
	CurrentChunk().WriteConstant(std::monostate{});
	CurrentChunk().Write(OpCode::OP_RETURN);

	const auto completed = CurrentContext().function;
	const auto upvalues = CurrentContext().upvalues;
	m_contexts.pop_back();

	EmitFunctionObject(completed, upvalues);
	emitResult();
}

std::string BytecodeGenerator::DefaultImportAlias(const std::string& qualifiedName)
{
	const auto pos = qualifiedName.find_last_of('.');
	return (pos == std::string::npos) ? qualifiedName : qualifiedName.substr(pos + 1);
}
