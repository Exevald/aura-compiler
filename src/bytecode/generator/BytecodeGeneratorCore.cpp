#include "../../ast/common/SemanticAnalyzerSupport.h"
#include "../../ast/sematic/SemanticAnalyzer.h"
#include "../../vm/execution/VirtualMachine.h"
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
		|| dynamic_cast<const GoExprNode*>(node)
		|| dynamic_cast<const AwaitExprNode*>(node)
		|| dynamic_cast<const IndexNode*>(node)
		|| dynamic_cast<const IdentifierNode*>(node)
		|| dynamic_cast<const UnaryExprNode*>(node)
		|| dynamic_cast<const IntegerLiteralNode*>(node)
		|| dynamic_cast<const FloatLiteralNode*>(node)
		|| dynamic_cast<const StringLiteralNode*>(node)
		|| dynamic_cast<const FunctionExprNode*>(node)
		|| dynamic_cast<const MemberAccessNode*>(node)
		|| dynamic_cast<const ArrayLiteralNode*>(node)
		|| dynamic_cast<const MapLiteralNode*>(node);
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
	m_comptimeFunctions.clear();
	m_comptimeBindings.clear();

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

std::string BytecodeGenerator::NormalizeImportedTypeName(
	const std::unordered_map<std::string, std::string>& importAliases,
	const std::string& typeName)
{
	std::string baseName;
	std::vector<std::string> typeArgs;
	if (SemanticAnalyzerDetail::SplitGenericName(typeName, baseName, typeArgs))
	{
		std::string result = NormalizeImportedTypeName(importAliases, baseName) + "<";
		for (size_t i = 0; i < typeArgs.size(); ++i)
		{
			if (i > 0)
			{
				result += ",";
			}
			result += NormalizeImportedTypeName(importAliases, typeArgs[i]);
		}
		result += ">";
		return result;
	}

	const auto dot = typeName.find('.');
	if (dot == std::string::npos)
	{
		return typeName;
	}
	if (const auto aliasIt = importAliases.find(typeName.substr(0, dot)); aliasIt != importAliases.end())
	{
		return aliasIt->second + typeName.substr(dot);
	}
	return typeName;
}

std::optional<std::string> BytecodeGenerator::EnsureStructMetadata(const std::string& typeName)
{
	if (typeName.empty() || typeName == "auto")
	{
		return std::nullopt;
	}

	const std::string normalizedType = NormalizeImportedTypeName(m_metadata.importAliases, typeName);
	const std::string qualifiedType = QualifyName(normalizedType);
	if (m_metadata.structLayouts.contains(qualifiedType))
	{
		return qualifiedType;
	}
	if (m_metadata.structLayouts.contains(normalizedType))
	{
		return normalizedType;
	}

	std::string baseName;
	std::vector<std::string> typeArgs;
	if (!SemanticAnalyzerDetail::SplitGenericName(qualifiedType, baseName, typeArgs))
	{
		return std::nullopt;
	}
	if (!m_metadata.structLayouts.contains(baseName))
	{
		return std::nullopt;
	}

	const auto paramsIt = m_metadata.structTypeParams.find(baseName);
	if (paramsIt == m_metadata.structTypeParams.end() || paramsIt->second.size() != typeArgs.size())
	{
		return std::nullopt;
	}

	std::unordered_map<std::string, std::string> replacements;
	for (size_t i = 0; i < typeArgs.size(); ++i)
	{
		replacements[paramsIt->second[i]] = typeArgs[i];
	}

	m_metadata.structLayouts[qualifiedType] = m_metadata.structLayouts.at(baseName);
	m_metadata.structTypeParams[qualifiedType] = paramsIt->second;
	m_metadata.structMethodNames[qualifiedType] = m_metadata.structMethodNames.at(baseName);
	if (const auto invariantIt = m_metadata.structInvariantChecks.find(baseName);
		invariantIt != m_metadata.structInvariantChecks.end())
	{
		m_metadata.structInvariantChecks[qualifiedType] = invariantIt->second;
	}

	auto& concreteFields = m_metadata.structFieldTypes[qualifiedType];
	concreteFields.clear();
	for (const auto& [fieldName, fieldType] : m_metadata.structFieldTypes.at(baseName))
	{
		concreteFields[fieldName] = SemanticAnalyzerDetail::SubstituteTypeString(fieldType, replacements);
	}

	return qualifiedType;
}

std::optional<std::string> BytecodeGenerator::EnsureActorMetadata(const std::string& typeName)
{
	if (typeName.empty() || typeName == "auto")
	{
		return std::nullopt;
	}

	const std::string normalizedType = NormalizeImportedTypeName(m_metadata.importAliases, typeName);
	const std::string qualifiedType = QualifyName(normalizedType);
	if (m_metadata.actorLayouts.contains(qualifiedType))
	{
		return qualifiedType;
	}
	if (m_metadata.actorLayouts.contains(normalizedType))
	{
		return normalizedType;
	}

	std::string baseName;
	std::vector<std::string> typeArgs;
	if (!SemanticAnalyzerDetail::SplitGenericName(qualifiedType, baseName, typeArgs))
	{
		return std::nullopt;
	}
	if (!m_metadata.actorLayouts.contains(baseName))
	{
		return std::nullopt;
	}

	m_metadata.actorLayouts[qualifiedType] = m_metadata.actorLayouts.at(baseName);
	if (const auto defaultsIt = m_metadata.actorFieldDefaults.find(baseName);
		defaultsIt != m_metadata.actorFieldDefaults.end())
	{
		m_metadata.actorFieldDefaults[qualifiedType] = defaultsIt->second;
	}
	if (const auto methodsIt = m_metadata.actorMethodNames.find(baseName);
		methodsIt != m_metadata.actorMethodNames.end())
	{
		m_metadata.actorMethodNames[qualifiedType] = methodsIt->second;
	}
	if (const auto queryIt = m_metadata.actorMethodIsQuery.find(baseName);
		queryIt != m_metadata.actorMethodIsQuery.end())
	{
		m_metadata.actorMethodIsQuery[qualifiedType] = queryIt->second;
	}
	return qualifiedType;
}

std::optional<std::string> BytecodeGenerator::EnsureInterfaceMetadata(const std::string& typeName)
{
	if (typeName.empty() || typeName == "auto")
	{
		return std::nullopt;
	}

	const std::string normalizedType = NormalizeImportedTypeName(m_metadata.importAliases, typeName);
	const std::string qualifiedType = QualifyName(normalizedType);
	if (m_metadata.interfaceMethods.contains(qualifiedType))
	{
		return qualifiedType;
	}
	if (m_metadata.interfaceMethods.contains(normalizedType))
	{
		return normalizedType;
	}

	std::string baseName;
	std::vector<std::string> typeArgs;
	if (!SemanticAnalyzerDetail::SplitGenericName(qualifiedType, baseName, typeArgs))
	{
		return std::nullopt;
	}
	if (!m_metadata.interfaceMethods.contains(baseName))
	{
		return std::nullopt;
	}

	m_metadata.interfaceMethods[qualifiedType] = m_metadata.interfaceMethods.at(baseName);
	return qualifiedType;
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
	if (contextIndex == 1
		&& !m_currentModule.empty()
		&& !m_metadata.importAliases.contains(name))
	{
		// Module-level bindings live in SharedRuntime. Capturing their top-level
		// local slots would give each function an independent value snapshot.
		// Import aliases remain lexical module objects used for member lookup.
		return std::nullopt;
	}
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
	if (name == "result"
		&& !m_activeContractResultSlots.empty()
		&& m_activeContractResultSlots.back().has_value())
	{
		CurrentChunk().Write(OpCode::OP_GET_LOCAL);
		CurrentChunk().code.push_back(*m_activeContractResultSlots.back());
		return;
	}
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

	std::string globalName = QualifyName(name);
	if (m_metadata.threadLocalGlobals.contains(globalName))
	{
		globalName = "__thread_local." + globalName;
	}
	const uint16_t nameIndex = CurrentChunk().AddConstant(
		std::make_shared<const std::string>(globalName));
	CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
	CurrentChunk().WriteOperand(OpCode::OP_GET_GLOBAL, nameIndex);
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

	std::string globalName = QualifyName(name);
	if (m_metadata.threadLocalGlobals.contains(globalName))
	{
		globalName = "__thread_local." + globalName;
	}
	const uint16_t nameIndex = CurrentChunk().AddConstant(
		std::make_shared<const std::string>(globalName));
	CurrentChunk().Write(OpCode::OP_SET_GLOBAL);
	CurrentChunk().WriteOperand(OpCode::OP_SET_GLOBAL, nameIndex);
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

		const uint16_t nameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(QualifyName(identifier->name)));
		CurrentChunk().Write(OpCode::OP_ADDR_GLOBAL);
		CurrentChunk().WriteOperand(OpCode::OP_ADDR_GLOBAL, nameIndex);
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

void BytecodeGenerator::EmitContractAssertion(const std::string& message)
{
	const uint16_t messageIndex = CurrentChunk().AddConstant(
		std::make_shared<const std::string>(message));
	CurrentChunk().Write(OpCode::OP_ASSERT);
	CurrentChunk().WriteOperand(OpCode::OP_ASSERT, messageIndex);
}

void BytecodeGenerator::RegisterFunctionSignature(
	const std::string& name,
	const std::vector<Parameter>& params,
	const std::vector<ContextBinding>& contextRequirements,
	const std::string& returnTypeName)
{
	auto& signature = m_metadata.functionSignatures[name];
	signature.refParams.clear();
	signature.defaultArgs.clear();
	signature.contextParams.clear();
	signature.returnTypeName = NormalizeImportedTypeName(m_metadata.importAliases, returnTypeName);
	signature.refParams.reserve(params.size());
	signature.defaultArgs.reserve(params.size());
	signature.contextParams.reserve(contextRequirements.size());
	signature.requiredArity = 0;
	signature.variadic = false;
	for (const auto& context : contextRequirements)
	{
		signature.contextParams.push_back(context.name);
	}
	for (const auto& param : params)
	{
		if (param.isVariadic)
		{
			signature.variadic = true;
			continue;
		}
		signature.refParams.push_back(param.typeName.rfind("ref<", 0) == 0);
		signature.defaultArgs.push_back(param.defaultValue.get());
		if (!param.hasDefaultValue)
		{
			++signature.requiredArity;
		}
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
	const uint16_t functionIndex = CurrentChunk().AddConstant(function);
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
	CurrentChunk().WriteOperand(OpCode::OP_CLOSURE, functionIndex);
}

void BytecodeGenerator::CompileFunctionBody(
	const std::string& name,
	const std::vector<Parameter>& params,
	const std::vector<ContextBinding>& contextRequirements,
	const std::vector<std::string>& raisedEffects,
	const std::vector<ContractNode*>& contracts,
	ASTNode* body,
	const std::function<void()>& emitResult,
	const bool isEffectHandler)
{
	FunctionContext context;
	InitializeFunctionContext(context, name);
	context.isEffectHandler = isEffectHandler;
	std::optional<uint8_t> contractResultSlot = std::nullopt;
	size_t minArity = 0;
	for (const auto& param : params)
	{
		if (!param.isVariadic && !param.hasDefaultValue)
		{
			++minArity;
		}
	}

	for (const auto& contextRequirement : contextRequirements)
	{
		context.symbols.Define(contextRequirement.name);
		context.function->arity++;
	}

	for (const auto& param : params)
	{
		context.symbols.Define(param.name);
		context.function->arity++;
		if (const auto structType = EnsureStructMetadata(param.typeName))
		{
			context.structVarScopes.back()[param.name] = *structType;
		}
		if (const auto interfaceType = EnsureInterfaceMetadata(param.typeName))
		{
			context.interfaceVarScopes.back()[param.name] = FunctionContext::InterfaceBinding{
				*interfaceType,
				"",
				FunctionContext::InterfaceBinding::RuntimeKind::Unknown
			};
		}
	}
	context.function->minArity = static_cast<int>(minArity);
	context.function->variadic = std::ranges::any_of(params, [](const Parameter& param) { return param.isVariadic; });

	m_contexts.push_back(std::move(context));
	m_activeFunctionContracts.push_back(contracts);
	m_activeRaisedEffects.emplace_back();
	for (const auto& effectName : raisedEffects)
	{
		m_activeRaisedEffects.back().insert(QualifyName(effectName));
	}
	if (std::ranges::any_of(contracts, [](const ContractNode* contract) {
			return contract && contract->kind == ContractKind::Ensures;
		}))
	{
		const uint8_t slot = CurrentContext().symbols.Define("__contract_result");
		CurrentChunk().WriteConstant(std::monostate{});
		CurrentChunk().Write(OpCode::OP_SET_LOCAL);
		CurrentChunk().code.push_back(slot);
		contractResultSlot = slot;
	}
	m_activeContractResultSlots.push_back(contractResultSlot);
	ScopeExit contractScope([this]() {
		m_activeContractResultSlots.pop_back();
		m_activeFunctionContracts.pop_back();
		m_activeRaisedEffects.pop_back();
	});
	for (const auto* contract : contracts)
	{
		if (!contract || contract->kind != ContractKind::Requires || !contract->expression)
		{
			continue;
		}
		contract->expression->Accept(*this);
		EmitContractAssertion("Contract requires failed in " + name);
	}
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

void BytecodeGenerator::RegisterComptimeFunctionsForChunk(
	VM::Execution::Chunk& chunk,
	const std::unordered_map<std::string, VM::Core::Value>& importedAliases)
{
	for (const auto& [alias, value] : importedAliases)
	{
		chunk.WriteConstant(value);
		const uint8_t localIndex = CurrentContext().symbols.Define(alias);
		chunk.Write(OpCode::OP_SET_LOCAL);
		chunk.code.push_back(localIndex);
	}

	std::unordered_set<const FunctionDeclNode*> uniqueFunctions;
	for (const auto& [_, function] : m_comptimeFunctions)
	{
		if (function)
		{
			uniqueFunctions.insert(function);
		}
	}

	for (const auto* function : uniqueFunctions)
	{
		const std::string exportedName = QualifyName(function->name);
		RegisterFunctionSignature(exportedName, function->params, function->contextRequirements, function->returnType);
		std::vector<ContractNode*> contracts;
		for (const auto& contract : function->contracts)
		{
			contracts.push_back(contract.get());
		}
		CompileFunctionBody(
			exportedName,
			function->params,
			function->contextRequirements,
			function->raisedEffects,
			contracts,
			function->body.get(),
			[this, &exportedName]() {
				const uint16_t nameIndex = CurrentChunk().AddConstant(
					std::make_shared<const std::string>(exportedName));
				CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
				CurrentChunk().WriteOperand(OpCode::OP_DEFINE_GLOBAL, nameIndex);
			});
	}
}

Value BytecodeGenerator::EvaluateComptime(ASTNode* node)
{
	if (!node)
	{
		return std::monostate{};
	}

	const auto savedContexts = std::move(m_contexts);
	const auto savedContracts = std::move(m_activeFunctionContracts);
	const auto savedContractResultSlots = std::move(m_activeContractResultSlots);
	const auto savedRaisedEffects = std::move(m_activeRaisedEffects);
	const auto savedHandledEffects = std::move(m_activeHandledEffects);

	FunctionContext topLevel;
	topLevel.function = std::make_shared<Function>();
	topLevel.function->name = "<comptime_top_level>";
	topLevel.symbols.Reset();
	topLevel.structVarScopes.emplace_back();
	topLevel.enumVarScopes.emplace_back();
	topLevel.actorVarScopes.emplace_back();
	topLevel.interfaceVarScopes.emplace_back();
	m_contexts.clear();
	m_contexts.push_back(std::move(topLevel));

	std::unordered_map<std::string, VM::Core::Value> importedAliases;
	for (const auto& [alias, moduleName] : m_metadata.importAliases)
	{
		auto module = std::make_shared<VM::Core::Module>();
		module->name = moduleName;
		importedAliases.emplace(alias, module);
	}

	RegisterComptimeFunctionsForChunk(CurrentChunk(), importedAliases);
	CompileFunctionBody("<comptime_eval>", {}, {}, {}, {}, node, []() {});
	CurrentChunk().Write(OpCode::OP_CALL);
	CurrentChunk().code.push_back(0);
	CurrentChunk().Write(OpCode::OP_RETURN);

	VM::Execution::VirtualMachine vm(std::make_shared<VM::Runtime::SharedRuntime>(), true);
	if (!vm.Interpret(CurrentContext().function->chunk.get()))
	{
		const std::string error = std::string(vm.GetContext().GetError());
		m_contexts = savedContexts;
		m_activeFunctionContracts = savedContracts;
		m_activeContractResultSlots = savedContractResultSlots;
		m_activeRaisedEffects = savedRaisedEffects;
		m_activeHandledEffects = savedHandledEffects;
		throw std::runtime_error(error.empty() ? "comptime execution failed" : error);
	}

	Value result = std::monostate{};
	if (!vm.GetContext().StackEmpty())
	{
		result = vm.GetContext().PeekValue(0);
	}

	m_contexts = savedContexts;
	m_activeFunctionContracts = savedContracts;
	m_activeContractResultSlots = savedContractResultSlots;
	m_activeRaisedEffects = savedRaisedEffects;
	m_activeHandledEffects = savedHandledEffects;
	return result;
}
