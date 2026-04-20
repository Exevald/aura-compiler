#include "../../ast/sematic/SemanticAnalyzer.h"
#include "../BytecodeGenerator.h"

#include <algorithm>
#include <memory>
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
		const uint8_t nameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(QualifyName(node.name)));
		CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
		CurrentChunk().code.push_back(nameIndex);

		if (node.storageClass == VarDeclNode::StorageClass::Shared)
		{
			const uint8_t mutexFnIndex = CurrentChunk().AddConstant(
				std::make_shared<const std::string>("std.sync.mutex"));
			CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
			CurrentChunk().code.push_back(mutexFnIndex);
			CurrentChunk().Write(OpCode::OP_CALL);
			CurrentChunk().code.push_back(0);

			const uint8_t hiddenMutexName = CurrentChunk().AddConstant(
				std::make_shared<const std::string>("__txn_mutex." + node.name));
			CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
			CurrentChunk().code.push_back(hiddenMutexName);
		}
	}

	if (const auto structType = InferStructTypeName(node.initializer.get()))
	{
		CurrentContext().structVarScopes.back()[node.name] = *structType;
	}
	else if (!node.explicitType.empty() && node.explicitType != "auto")
	{
		if (m_metadata.structLayouts.contains(QualifyName(node.explicitType)))
		{
			CurrentContext().structVarScopes.back()[node.name] = QualifyName(node.explicitType);
		}
		else if (m_metadata.structLayouts.contains(node.explicitType))
		{
			CurrentContext().structVarScopes.back()[node.name] = node.explicitType;
		}
	}

	if (const auto enumType = InferEnumTypeName(node.initializer.get()))
	{
		CurrentContext().enumVarScopes.back()[node.name] = *enumType;
	}

	if (const auto actorType = InferActorTypeName(node.initializer.get()))
	{
		CurrentContext().actorVarScopes.back()[node.name] = *actorType;
	}
	else if (!node.explicitType.empty() && node.explicitType != "auto")
	{
		if (m_metadata.actorLayouts.contains(QualifyName(node.explicitType)))
		{
			CurrentContext().actorVarScopes.back()[node.name] = QualifyName(node.explicitType);
		}
		else if (m_metadata.actorLayouts.contains(node.explicitType))
		{
			CurrentContext().actorVarScopes.back()[node.name] = node.explicitType;
		}
	}

	if (const auto interfaceBinding = InferInterfaceBinding(node.initializer.get(), node.explicitType))
	{
		CurrentContext().interfaceVarScopes.back()[node.name] = *interfaceBinding;
	}
	else if (!node.explicitType.empty() && node.explicitType != "auto")
	{
		if (m_metadata.interfaceMethods.contains(QualifyName(node.explicitType)))
		{
			CurrentContext().interfaceVarScopes.back()[node.name] = FunctionContext::InterfaceBinding{
				QualifyName(node.explicitType),
				"",
				FunctionContext::InterfaceBinding::RuntimeKind::Unknown
			};
		}
		else if (m_metadata.interfaceMethods.contains(node.explicitType))
		{
			CurrentContext().interfaceVarScopes.back()[node.name] = FunctionContext::InterfaceBinding{
				node.explicitType,
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
	layout.clear();
	layout.reserve(node.fields.size());
	for (const auto& field : node.fields)
	{
		layout.push_back(field.name);
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

		FunctionContext context;
		InitializeFunctionContext(context, hiddenName);
		context.actorSelfType = qualifiedName;
		context.symbols.Define("self");
		context.function->arity++;
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
		if (method.body)
		{
			method.body->Accept(*this);
		}
		CurrentChunk().WriteConstant(std::monostate{});
		CurrentChunk().Write(OpCode::OP_RETURN);

		auto completed = CurrentContext().function;
		auto upvalues = CurrentContext().upvalues;
		m_contexts.pop_back();

		EmitFunctionObject(completed, upvalues);
		const uint8_t nameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(hiddenName));
		CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
		CurrentChunk().code.push_back(nameIndex);
	}

	for (const auto& [methodName, hiddenName] : methodNames)
	{
		const uint8_t hiddenNameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(hiddenName));
		CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
		CurrentChunk().code.push_back(hiddenNameIndex);
		const uint8_t methodNameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(methodName));
		CurrentChunk().Write(OpCode::OP_CONSTANT);
		CurrentChunk().code.push_back(methodNameIndex);
	}
	CurrentChunk().Write(OpCode::OP_BUILD_ACTOR_METHODS);
	CurrentChunk().code.push_back(static_cast<uint8_t>(methodNames.size()));
	const uint8_t methodTableNameIndex = CurrentChunk().AddConstant(
		std::make_shared<const std::string>(qualifiedName + ".__methods"));
	CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
	CurrentChunk().code.push_back(methodTableNameIndex);
}

void BytecodeGenerator::Visit(StructDeclNode& node)
{
	const std::string qualifiedName = QualifyName(node.name);
	m_metadata.structLayouts[qualifiedName].clear();
	m_metadata.structFieldTypes[qualifiedName].clear();
	m_metadata.structMethodNames[qualifiedName].clear();
	auto& layout = m_metadata.structLayouts[qualifiedName];
	auto& fieldTypes = m_metadata.structFieldTypes[qualifiedName];
	layout.reserve(node.fields.size());
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
		RegisterFunctionSignature(hiddenName, loweredParams);

		FunctionContext context;
		InitializeFunctionContext(context, hiddenName);
		context.methodSelfStructType = qualifiedName;
		context.symbols.Define("self");
		context.function->arity++;
		context.structVarScopes.back()["self"] = qualifiedName;
		for (const auto& param : method.params)
		{
			context.symbols.Define(param.name);
			context.function->arity++;
		}

		m_contexts.push_back(std::move(context));
		if (method.body)
		{
			method.body->Accept(*this);
		}
		CurrentChunk().WriteConstant(std::monostate{});
		CurrentChunk().Write(OpCode::OP_RETURN);

		auto completed = CurrentContext().function;
		auto upvalues = CurrentContext().upvalues;
		m_contexts.pop_back();

		EmitFunctionObject(completed, upvalues);
		const uint8_t nameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(hiddenName));
		CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
		CurrentChunk().code.push_back(nameIndex);
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
