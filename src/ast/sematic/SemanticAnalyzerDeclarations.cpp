#include "../common/SemanticAnalyzerSupport.h"
#include "SemanticAnalyzer.h"

#include <functional>
#include <ranges>
#include <stdexcept>

using SemanticAnalyzerDetail::FormatUndefined;
using SemanticAnalyzerDetail::ScopeExit;
using SemanticAnalyzerDetail::SplitGenericName;
using SemanticAnalyzerDetail::SubstituteTypeString;

void SemanticAnalyzer::Visit(VarDeclNode& node)
{
	TypeInfo initType = TypeInfo::Unknown();
	if (node.initializer)
	{
		initType = VisitAndGet(node.initializer.get());
	}

	const TypeInfo expectedType = StringToType(node.explicitType);
	if (expectedType.kind != TypeKind::Unknown && initType.kind != TypeKind::Unknown)
	{
		if (!IsAssignable(expectedType, initType))
		{
			throw std::runtime_error(
				"Type mismatch in declaration of '"
				+ node.name
				+ "': expected " + node.explicitType);
		}
	}

	const TypeInfo finalType = (expectedType.kind == TypeKind::Unknown) ? initType : expectedType;
	Define(node.name, finalType, node.isConst);
	if (node.storageClass == VarDeclNode::StorageClass::Shared)
	{
		m_sharedVariables.insert(node.name);
	}
	if (!m_currentModule.empty() && m_environment.scopes.size() == 1)
	{
		m_modules.memberTypes[m_currentModule][node.name] = finalType;
	}
}

void SemanticAnalyzer::Visit(TypeAliasNode& node)
{
	const std::string qualifiedName = QualifyName(node.name);
	if (!node.typeParams.empty())
	{
		m_types.genericAliases[node.name] = { node.typeParams, node.aliasedType };
		m_types.genericAliases[qualifiedName] = { node.typeParams, node.aliasedType };
		m_lastType = TypeInfo::Void();
		return;
	}

	m_types.aliases[node.name] = node.aliasedType;
	m_types.aliases[qualifiedName] = node.aliasedType;
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(StructDeclNode& node)
{
	std::unordered_map<std::string, TypeInfo> fields;
	fields.reserve(node.fields.size());

	for (const auto& field : node.fields)
	{
		fields.emplace(field.name, StringToType(field.typeName));
	}

	std::unordered_map<std::string, TypeInfo> methods;
	for (const auto& method : node.methods)
	{
		std::vector<TypeInfo> paramTypes;
		for (const auto& param : method.params)
		{
			paramTypes.push_back(StringToType(param.typeName));
		}
		methods.emplace(method.name, TypeInfo::Function(paramTypes, StringToType(method.returnType)));
	}

	const TypeInfo structType = TypeInfo::Struct(QualifyName(node.name), std::move(fields), methods);
	m_types.structs[structType.name] = structType;
	Define(node.name, structType);

	for (const auto& interfaceName : node.implementedInterfaces)
	{
		TypeInfo interfaceType = StringToType(interfaceName);
		if (interfaceType.kind != TypeKind::Interface || !interfaceType.methods)
		{
			throw std::runtime_error("Unknown interface in implements list: " + interfaceName);
		}
		if (!IsAssignable(interfaceType, structType))
		{
			throw std::runtime_error("Struct '" + node.name + "' does not satisfy interface '" + interfaceName + "'");
		}
	}

	for (const auto& method : node.methods)
	{
		m_environment.PushScope();
		m_environment.PushStructMethodSelf(structType);
		ScopeExit methodScope([this]() {
			m_environment.PopStructMethodSelf();
			m_environment.PopScope();
		});
		const TypeInfo returnType = StringToType(method.returnType);
		const auto oldReturn = m_currentExpectedReturn;
		m_currentExpectedReturn = returnType;
		ScopeExit returnScope([this, oldReturn]() {
			m_currentExpectedReturn = oldReturn;
		});

		Define("self", structType);
		for (size_t i = 0; i < method.params.size(); ++i)
		{
			Define(method.params[i].name, methods.at(method.name).params[i]);
		}

		if (method.body)
		{
			method.body->Accept(*this);
		}
	}
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(InterfaceDeclNode& node)
{
	std::unordered_map<std::string, TypeInfo> methods;
	for (const auto& method : node.methods)
	{
		std::vector<TypeInfo> paramTypes;
		for (const auto& param : method.params)
		{
			paramTypes.push_back(StringToType(param.typeName));
		}
		methods.emplace(method.name, TypeInfo::Function(paramTypes, StringToType(method.returnType)));
	}

	const TypeInfo interfaceType = TypeInfo::Interface(QualifyName(node.name), std::move(methods));
	m_types.interfaces[interfaceType.name] = interfaceType;
	Define(node.name, interfaceType);
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(EffectDeclNode& node)
{
	std::unordered_map<std::string, TypeInfo> operations;
	for (const auto& operation : node.operations)
	{
		std::vector<TypeInfo> paramTypes;
		for (const auto& param : operation.params)
		{
			paramTypes.push_back(StringToType(param.typeName));
		}
		const TypeInfo opType = TypeInfo::Function(paramTypes, StringToType(operation.returnType));
		operations.emplace(operation.name, opType);
		m_types.effectOperations[operation.name] = QualifyName(node.name);
		Define(operation.name, opType);
	}

	const TypeInfo effectType = TypeInfo::Effect(QualifyName(node.name), std::move(operations));
	m_types.effects[effectType.name] = effectType;
	Define(node.name, effectType);
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(ActorDeclNode& node)
{
	std::unordered_map<std::string, TypeInfo> fields;
	for (const auto& field : node.fields)
	{
		const TypeInfo fieldType = StringToType(field.typeName);
		ValidateSendable(fieldType, "Actor state field '" + field.name + "'");
		fields.emplace(field.name, fieldType);
	}

	std::unordered_map<std::string, TypeInfo> methods;
	for (const auto& method : node.methods)
	{
		std::vector<TypeInfo> paramTypes;
		for (const auto& param : method.params)
		{
			const TypeInfo paramType = StringToType(param.typeName);
			ValidateSendable(paramType, "Actor method argument '" + param.name + "'");
			paramTypes.push_back(paramType);
		}

		const TypeInfo returnType = StringToType(method.returnType);
		if (method.kind == ActorMethodDecl::Kind::Message && returnType.kind != TypeKind::Void)
		{
			throw std::runtime_error("Actor msg methods must return void");
		}
		if (method.kind == ActorMethodDecl::Kind::Query)
		{
			ValidateSendable(returnType, "Actor query return type");
		}
		methods.emplace(method.name, TypeInfo::Function(paramTypes, returnType));
	}

	const TypeInfo actorType = TypeInfo::Actor(QualifyName(node.name), fields, methods);
	m_types.actors[actorType.name] = actorType;
	Define(node.name, actorType);

	for (const auto& method : node.methods)
	{
		m_environment.PushScope();
		ScopeExit methodScope([this]() {
			m_environment.PopScope();
		});

		std::unordered_set<std::string> actorStateNames;
		for (const auto& [fieldName, fieldType] : fields)
		{
			Define(fieldName, fieldType);
			actorStateNames.insert(fieldName);
		}
		m_actorStateScopes.push_back(std::move(actorStateNames));
		ScopeExit actorStateScope([this]() {
			m_actorStateScopes.pop_back();
		});

		std::unordered_set<std::string> raisedEffects(method.raisedEffects.begin(), method.raisedEffects.end());
		m_activeRaisedEffects.push_back(std::move(raisedEffects));
		ScopeExit raisedEffectScope([this]() {
			m_activeRaisedEffects.pop_back();
		});

		if (method.kind == ActorMethodDecl::Kind::Query)
		{
			++m_actorQueryDepth;
		}
		ScopeExit queryScope([this, &method]() {
			if (method.kind == ActorMethodDecl::Kind::Query)
			{
				--m_actorQueryDepth;
			}
		});

		for (size_t i = 0; i < method.params.size(); ++i)
		{
			Define(method.params[i].name, methods.at(method.name).params[i]);
		}

		const auto oldReturn = m_currentExpectedReturn;
		m_currentExpectedReturn = StringToType(method.returnType);
		ScopeExit returnScope([this, oldReturn]() {
			m_currentExpectedReturn = oldReturn;
		});

		if (method.body)
		{
			method.body->Accept(*this);
		}
	}

	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(EnumDeclNode& node)
{
	const std::string enumName = QualifyName(node.name);
	const TypeInfo enumType = TypeInfo::Enum(enumName);
	m_types.enums[enumName] = enumType;
	Define(node.name, enumType);

	for (size_t index = 0; index < node.variants.size(); ++index)
	{
		std::vector<TypeInfo> argTypes;
		argTypes.reserve(node.variants[index].argTypes.size());
		for (const auto& argTypeName : node.variants[index].argTypes)
		{
			argTypes.push_back(StringToType(argTypeName));
		}

		const TypeInfo ctorType = TypeInfo::EnumConstructor(
			enumName,
			static_cast<int>(index),
			std::move(argTypes));
		const std::string qualifiedCtorName = QualifyName(node.variants[index].name);
		m_types.enumConstructors[qualifiedCtorName] = ctorType;
		Define(node.variants[index].name, ctorType);
	}

	m_lastType = TypeInfo::Void();
}