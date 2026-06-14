#include "../../ast/common/SemanticAnalyzerSupport.h"
#include "../../ast/sematic/SemanticAnalyzer.h"
#include "../BytecodeGenerator.h"

#include <algorithm>
#include <memory>
#include <ranges>
#include <stdexcept>

using namespace VM::Core;

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
	CurrentChunk().WriteConstant(std::make_shared<const std::string>(node.value));
}

void BytecodeGenerator::Visit(IdentifierNode& node)
{
	EmitGetVariable(node.name);
}

void BytecodeGenerator::Visit(UnaryExprNode& node)
{
	if (node.op == "+")
	{
		node.operand->Accept(*this);
		return;
	}
	if (node.op == "-")
	{
		node.operand->Accept(*this);
		CurrentChunk().Write(OpCode::OP_NEGATE);
		return;
	}
	if (node.op == "!" || node.op == "not")
	{
		node.operand->Accept(*this);
		CurrentChunk().Write(OpCode::OP_NOT);
		return;
	}
	if (node.op == "*")
	{
		node.operand->Accept(*this);
		CurrentChunk().Write(OpCode::OP_DEREF_GET);
		return;
	}
	if (node.op == "&")
	{
		EmitAddressOf(node.operand.get());
		return;
	}

	throw std::runtime_error("Unsupported unary operator for codegen: " + node.op);
}

void BytecodeGenerator::Visit(BinaryExprNode& node)
{
	node.left->Accept(*this);
	node.right->Accept(*this);
	EmitBinaryOp(node.op);
}

void BytecodeGenerator::Visit(AssignmentNode& node)
{
	AccessEmitter{ *this }.EmitAssignment(node);
}

void BytecodeGenerator::Visit(VarDeclNode& node)
{
	if (node.initializer)
	{
		node.initializer->Accept(*this);
	}
	else
	{
		CurrentChunk().WriteConstant(std::monostate{});
	}

	const uint8_t localIndex = CurrentContext().symbols.Define(node.name);
	CurrentChunk().Write(OpCode::OP_SET_LOCAL);
	CurrentChunk().code.push_back(localIndex);

	if (m_contexts.size() == 1 && !m_currentModule.empty())
	{
		CurrentChunk().Write(OpCode::OP_GET_LOCAL);
		CurrentChunk().code.push_back(localIndex);
		std::string globalName = QualifyName(node.name);
		if (node.storageClass == VarDeclNode::StorageClass::ThreadLocal)
		{
			m_metadata.threadLocalGlobals.insert(globalName);
			globalName = "__thread_local." + globalName;
		}
		const uint16_t nameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(globalName));
		CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
		CurrentChunk().WriteOperand(OpCode::OP_DEFINE_GLOBAL, nameIndex);

		if (node.storageClass == VarDeclNode::StorageClass::Shared)
		{
			const uint16_t mutexFnIndex = CurrentChunk().AddConstant(
				std::make_shared<const std::string>("std.sync_native.mutex"));
			CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
			CurrentChunk().WriteOperand(OpCode::OP_GET_GLOBAL, mutexFnIndex);
			CurrentChunk().Write(OpCode::OP_CALL);
			CurrentChunk().code.push_back(0);

			const uint16_t hiddenMutexName = CurrentChunk().AddConstant(
				std::make_shared<const std::string>("__txn_mutex." + node.name));
			CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
			CurrentChunk().WriteOperand(OpCode::OP_DEFINE_GLOBAL, hiddenMutexName);
		}
	}

	if (const auto structType = InferStructTypeName(node.initializer.get()))
	{
		const std::string explicitType = NormalizeImportedTypeName(m_metadata.importAliases, node.explicitType);
		if (!explicitType.empty() && explicitType != "auto")
		{
			if (const auto normalized = EnsureStructMetadata(explicitType))
			{
				CurrentContext().structVarScopes.back()[node.name] = *normalized;
			}
			else
			{
				CurrentContext().structVarScopes.back()[node.name] = *structType;
			}
		}
		else
		{
			CurrentContext().structVarScopes.back()[node.name] = *structType;
		}
	}
	else if (!node.explicitType.empty() && node.explicitType != "auto")
	{
		const std::string explicitType = NormalizeImportedTypeName(m_metadata.importAliases, node.explicitType);
		if (const auto normalized = EnsureStructMetadata(explicitType))
		{
			CurrentContext().structVarScopes.back()[node.name] = *normalized;
		}
	}

	if (const auto enumType = InferEnumTypeName(node.initializer.get()))
	{
		CurrentContext().enumVarScopes.back()[node.name] = *enumType;
	}
	else if (!node.explicitType.empty() && node.explicitType != "auto")
	{
		std::string explicitType = NormalizeImportedTypeName(m_metadata.importAliases, node.explicitType);
		std::string enumBase;
		std::vector<std::string> enumArgs;
		if (SemanticAnalyzerDetail::SplitGenericName(explicitType, enumBase, enumArgs))
		{
			explicitType = enumBase;
		}
		for (const auto& variantInfo : m_metadata.enumVariants | std::views::values)
		{
			if (variantInfo.enumName == explicitType)
			{
				CurrentContext().enumVarScopes.back()[node.name] = explicitType;
				break;
			}
		}
	}

	if (const auto actorType = InferActorTypeName(node.initializer.get()))
	{
		if (const std::string explicitType = NormalizeImportedTypeName(m_metadata.importAliases, node.explicitType);
			!explicitType.empty() && explicitType != "auto")
		{
			if (const auto normalized = EnsureActorMetadata(explicitType))
			{
				CurrentContext().actorVarScopes.back()[node.name] = *normalized;
			}
			else
			{
				CurrentContext().actorVarScopes.back()[node.name] = *actorType;
			}
		}
		else
		{
			CurrentContext().actorVarScopes.back()[node.name] = *actorType;
		}
	}
	else if (!node.explicitType.empty() && node.explicitType != "auto")
	{
		const std::string explicitType = NormalizeImportedTypeName(m_metadata.importAliases, node.explicitType);
		if (const auto normalized = EnsureActorMetadata(explicitType))
		{
			CurrentContext().actorVarScopes.back()[node.name] = *normalized;
		}
	}

	if (const auto interfaceBinding = InferInterfaceBinding(node.initializer.get(), node.explicitType))
	{
		CurrentContext().interfaceVarScopes.back()[node.name] = *interfaceBinding;
	}
	else if (!node.explicitType.empty() && node.explicitType != "auto")
	{
		const std::string explicitType = NormalizeImportedTypeName(m_metadata.importAliases, node.explicitType);
		if (const auto interfaceName = EnsureInterfaceMetadata(explicitType))
		{
			CurrentContext().interfaceVarScopes.back()[node.name] = FunctionContext::InterfaceBinding{
				*interfaceName,
				"",
				FunctionContext::InterfaceBinding::RuntimeKind::Unknown
			};
		}
	}
}

void BytecodeGenerator::Visit(TypeAliasNode& node)
{
	(void)node;
}

void BytecodeGenerator::Visit(InterfaceDeclNode& node)
{
	auto& methods = m_metadata.interfaceMethods[QualifyName(node.name)];
	methods.clear();
	for (const auto& method : node.methods)
	{
		methods.push_back(method.name);
	}
}

void BytecodeGenerator::Visit(EffectDeclNode& node)
{
	auto& operations = m_metadata.effectOperations[QualifyName(node.name)];
	operations.clear();
	for (const auto& operation : node.operations)
	{
		operations[operation.name] = QualifyName(node.name);
	}
}

void BytecodeGenerator::Visit(ActorDeclNode& node)
{
	const std::string qualifiedName = QualifyName(node.name);
	auto& layout = m_metadata.actorLayouts[qualifiedName];
	auto& defaults = m_metadata.actorFieldDefaults[qualifiedName];
	layout.clear();
	defaults.clear();
	layout.reserve(node.fields.size());
	defaults.reserve(node.fields.size());
	for (const auto& field : node.fields)
	{
		layout.push_back(field.name);
		defaults.push_back(field.initializer.get());
	}

	auto& methodNames = m_metadata.actorMethodNames[qualifiedName];
	auto& methodKinds = m_metadata.actorMethodIsQuery[qualifiedName];
	methodNames.clear();
	methodKinds.clear();

	for (const auto& method : node.methods)
	{
		const std::string hiddenName = qualifiedName + "." + method.name;
		methodNames[method.name] = hiddenName;
		methodKinds[method.name] = method.kind == ActorMethodDecl::Kind::Query;
		std::vector<Parameter> loweredParams;
		loweredParams.push_back({ "self", qualifiedName });
		for (const auto& param : method.params)
		{
			loweredParams.push_back(param);
		}
		RegisterFunctionSignature(hiddenName, loweredParams, method.contextRequirements, method.returnType);

		FunctionContext context;
		InitializeFunctionContext(context, hiddenName);
		context.actorSelfType = qualifiedName;
		context.symbols.Define("self");
		context.function->arity++;
		for (const auto& [name, typeName] : method.contextRequirements)
		{
			context.symbols.Define(name);
			context.function->arity++;
		}
		for (const auto& field : node.fields)
		{
			context.actorStateNames.insert(field.name);
		}
		for (const auto& param : method.params)
		{
			context.symbols.Define(param.name);
			context.function->arity++;
		}

		m_contexts.push_back(std::move(context));
		std::vector<ContractNode*> activeContracts;
		activeContracts.reserve(method.contracts.size());
		for (const auto& contract : method.contracts)
		{
			activeContracts.push_back(contract.get());
		}
		m_activeFunctionContracts.push_back(activeContracts);
		m_activeRaisedEffects.emplace_back();
		for (const auto& effectName : method.raisedEffects)
		{
			m_activeRaisedEffects.back().insert(QualifyName(effectName));
		}
		std::optional<uint8_t> contractResultSlot = std::nullopt;
		if (std::ranges::any_of(activeContracts, [](const ContractNode* contract) {
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
		for (const auto* contract : activeContracts)
		{
			if (!contract || contract->kind != ContractKind::Requires || !contract->expression)
			{
				continue;
			}
			contract->expression->Accept(*this);
			EmitContractAssertion("Contract requires failed in " + hiddenName);
		}
		if (method.body)
		{
			method.body->Accept(*this);
		}
		CurrentChunk().WriteConstant(std::monostate{});
		CurrentChunk().Write(OpCode::OP_RETURN);

		auto completed = CurrentContext().function;
		auto upvalues = CurrentContext().upvalues;
		m_activeContractResultSlots.pop_back();
		m_activeFunctionContracts.pop_back();
		m_activeRaisedEffects.pop_back();
		m_contexts.pop_back();

		EmitFunctionObject(completed, upvalues);
		const uint16_t nameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(hiddenName));
		CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
		CurrentChunk().WriteOperand(OpCode::OP_DEFINE_GLOBAL, nameIndex);
	}

	for (const auto& [methodName, hiddenName] : methodNames)
	{
		const uint16_t hiddenNameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(hiddenName));
		CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
		CurrentChunk().WriteOperand(OpCode::OP_GET_GLOBAL, hiddenNameIndex);
		const uint16_t methodNameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(methodName));
		CurrentChunk().Write(OpCode::OP_CONSTANT);
		CurrentChunk().WriteOperand(OpCode::OP_CONSTANT, methodNameIndex);
	}
	CurrentChunk().Write(OpCode::OP_BUILD_ACTOR_METHODS);
	CurrentChunk().code.push_back(static_cast<uint8_t>(methodNames.size()));
	const uint16_t methodTableNameIndex = CurrentChunk().AddConstant(
		std::make_shared<const std::string>(qualifiedName + ".__methods"));
	CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
	CurrentChunk().WriteOperand(OpCode::OP_DEFINE_GLOBAL, methodTableNameIndex);
}

void BytecodeGenerator::Visit(StructDeclNode& node)
{
	const std::string qualifiedName = QualifyName(node.name);
	m_metadata.structLayouts[qualifiedName].clear();
	m_metadata.structFieldTypes[qualifiedName].clear();
	m_metadata.structTypeParams[qualifiedName].clear();
	m_metadata.structMethodNames[qualifiedName].clear();
	auto& layout = m_metadata.structLayouts[qualifiedName];
	auto& fieldTypes = m_metadata.structFieldTypes[qualifiedName];
	auto& typeParams = m_metadata.structTypeParams[qualifiedName];
	layout.reserve(node.fields.size());
	typeParams.reserve(node.typeParams.size());
	for (const auto& typeParam : node.typeParams)
	{
		typeParams.push_back(typeParam.name);
	}
	for (const auto& field : node.fields)
	{
		layout.push_back(field.name);
		if (m_metadata.structLayouts.contains(QualifyName(field.typeName)))
		{
			fieldTypes[field.name] = QualifyName(field.typeName);
		}
		else
		{
			fieldTypes[field.name] = field.typeName;
		}
	}

	if (std::ranges::any_of(node.contracts, [](const std::unique_ptr<ContractNode>& contract) {
			return contract && contract->kind == ContractKind::Invariant;
		}))
	{
		const std::string invariantName = qualifiedName + ".__invariant";
		m_metadata.structInvariantChecks[qualifiedName] = invariantName;

		FunctionContext context;
		InitializeFunctionContext(context, invariantName);
		context.methodSelfStructType = qualifiedName;
		context.symbols.Define("self");
		context.function->arity++;
		context.structVarScopes.back()["self"] = qualifiedName;
		m_contexts.push_back(std::move(context));
		for (const auto& contract : node.contracts)
		{
			if (!contract || contract->kind != ContractKind::Invariant || !contract->expression)
			{
				continue;
			}
			contract->expression->Accept(*this);
			EmitContractAssertion("Contract invariant failed in " + qualifiedName);
		}
		CurrentChunk().WriteConstant(std::monostate{});
		CurrentChunk().Write(OpCode::OP_RETURN);

		auto completed = CurrentContext().function;
		auto upvalues = CurrentContext().upvalues;
		m_contexts.pop_back();

		EmitFunctionObject(completed, upvalues);
		const uint16_t invariantNameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(invariantName));
		CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
		CurrentChunk().WriteOperand(OpCode::OP_DEFINE_GLOBAL, invariantNameIndex);
	}

	for (const auto& method : node.methods)
	{
		const std::string hiddenName = qualifiedName + "." + method.name;
		m_metadata.structMethodNames[qualifiedName][method.name] = hiddenName;

		std::vector<Parameter> loweredParams;
		loweredParams.push_back({ "self", qualifiedName });
		for (const auto& param : method.params)
		{
			loweredParams.push_back(param);
		}
		RegisterFunctionSignature(hiddenName, loweredParams, method.contextRequirements, method.returnType);

		FunctionContext context;
		InitializeFunctionContext(context, hiddenName);
		context.methodSelfStructType = qualifiedName;
		context.symbols.Define("self");
		context.function->arity++;
		context.structVarScopes.back()["self"] = qualifiedName;
		for (const auto& [name, typeName] : method.contextRequirements)
		{
			context.symbols.Define(name);
			context.function->arity++;
		}
		for (const auto& param : method.params)
		{
			context.symbols.Define(param.name);
			context.function->arity++;
		}

		m_contexts.push_back(std::move(context));
		std::vector<ContractNode*> activeContracts;
		activeContracts.reserve(method.contracts.size());
		for (const auto& contract : method.contracts)
		{
			activeContracts.push_back(contract.get());
		}
		m_activeFunctionContracts.push_back(activeContracts);
		m_activeRaisedEffects.emplace_back();
		for (const auto& effectName : method.raisedEffects)
		{
			m_activeRaisedEffects.back().insert(QualifyName(effectName));
		}
		std::optional<uint8_t> contractResultSlot = std::nullopt;
		if (std::ranges::any_of(activeContracts, [](const ContractNode* contract) {
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
		for (const auto* contract : activeContracts)
		{
			if (!contract || contract->kind != ContractKind::Requires || !contract->expression)
			{
				continue;
			}
			contract->expression->Accept(*this);
			EmitContractAssertion("Contract requires failed in " + hiddenName);
		}
		if (method.body)
		{
			method.body->Accept(*this);
		}
		CurrentChunk().WriteConstant(std::monostate{});
		CurrentChunk().Write(OpCode::OP_RETURN);

		auto completed = CurrentContext().function;
		auto upvalues = CurrentContext().upvalues;
		m_activeContractResultSlots.pop_back();
		m_activeFunctionContracts.pop_back();
		m_activeRaisedEffects.pop_back();
		m_contexts.pop_back();

		EmitFunctionObject(completed, upvalues);
		const uint16_t nameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(hiddenName));
		CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
		CurrentChunk().WriteOperand(OpCode::OP_DEFINE_GLOBAL, nameIndex);
	}
}

void BytecodeGenerator::Visit(EnumDeclNode& node)
{
	const std::string enumName = QualifyName(node.name);
	for (size_t index = 0; index < node.variants.size(); ++index)
	{
		m_metadata.enumVariants[QualifyName(node.variants[index].name)] = {
			static_cast<uint8_t>(index),
			static_cast<uint8_t>(node.variants[index].argTypes.size()),
			enumName
		};
	}
}
