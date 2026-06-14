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
	const TypeInfo expectedType = StringToType(node.explicitType);
	TypeInfo initType = TypeInfo::Unknown();
	if (node.initializer)
	{
		const auto oldReturn = m_currentExpectedReturn;
		if (expectedType.kind != TypeKind::Unknown)
		{
			m_currentExpectedReturn = expectedType;
		}
		ScopeExit restoreExpected([this, oldReturn]() {
			m_currentExpectedReturn = oldReturn;
		});
		initType = VisitAndGet(node.initializer.get());
	}
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
	if (!node.typeParams.empty())
	{
		m_typeParamScopes.emplace_back();
		ScopeExit typeParamScope([this]() {
			m_typeParamScopes.pop_back();
		});
		for (const auto& typeParam : node.typeParams)
		{
			std::vector<TypeInfo> constraints;
			for (const auto& constraintName : typeParam.constraints)
			{
				constraints.push_back(StringToType(constraintName));
			}
			m_typeParamScopes.back()[typeParam.name] = TypeInfo::TypeParameter(typeParam.name, std::move(constraints));
		}

		std::unordered_map<std::string, TypeInfo> fields;
		TypeRegistry::ConstructorSignature ctorSignature;
		ctorSignature.requiredArity = node.fields.size();
		ctorSignature.params.reserve(node.fields.size());
		for (const auto& field : node.fields)
		{
			const TypeInfo fieldType = StringToType(field.typeName);
			fields.emplace(field.name, fieldType);
			ctorSignature.params.emplace_back(field.name, fieldType);
		}
		std::unordered_map<std::string, TypeInfo> methods;
		for (const auto& method : node.methods)
		{
			ValidateParameterList(method.params, "method '" + method.name + "'");
			std::vector<TypeInfo> paramTypes = BuildFixedParamTypes(method.params);
			const auto variadicParamType = BuildVariadicParamType(method.params);
			std::vector<std::pair<std::string, TypeInfo>> contextTypes;
			size_t requiredArity = 0;
			for (const auto& param : method.params)
			{
				if (!param.isVariadic && !param.hasDefaultValue)
				{
					++requiredArity;
				}
			}
			for (const auto& context : method.contextRequirements)
			{
				contextTypes.emplace_back(context.name, StringToType(context.typeName));
			}
			std::unordered_set<std::string> raisedEffects;
			for (const auto& effectName : method.raisedEffects)
			{
				raisedEffects.insert(QualifyName(effectName));
			}
			methods.emplace(
				method.name,
				TypeInfo::Function(
					paramTypes,
					StringToType(method.returnType),
					requiredArity,
					raisedEffects,
					contextTypes,
					false,
					variadicParamType.has_value(),
					variadicParamType));
		}

		std::unordered_set<std::string> implementedInterfaces;
		for (const auto& interfaceName : node.implementedInterfaces)
		{
			const TypeInfo interfaceType = StringToType(interfaceName);
			if (interfaceType.kind != TypeKind::Interface || !interfaceType.methods)
			{
				throw std::runtime_error("Unknown interface in implements list: " + interfaceName);
			}
			implementedInterfaces.insert(interfaceType.name);
		}

		const std::string qualifiedName = QualifyName(node.name);
		const TypeInfo templatedType = TypeInfo::Struct(qualifiedName, fields, methods, implementedInterfaces);
		m_types.genericStructs[node.name] = { node.typeParams, templatedType };
		m_types.genericStructs[qualifiedName] = { node.typeParams, templatedType };
		m_types.constructorSignatures[node.name] = ctorSignature;
		m_types.constructorSignatures[qualifiedName] = ctorSignature;
		Define(node.name, templatedType);
		m_lastType = TypeInfo::Void();
		return;
	}
	std::unordered_map<std::string, TypeInfo> fields;
	fields.reserve(node.fields.size());
	TypeRegistry::ConstructorSignature ctorSignature;
	ctorSignature.requiredArity = node.fields.size();
	ctorSignature.params.reserve(node.fields.size());

	for (const auto& field : node.fields)
	{
		const TypeInfo fieldType = StringToType(field.typeName);
		fields.emplace(field.name, fieldType);
		ctorSignature.params.emplace_back(field.name, fieldType);
	}

	std::unordered_map<std::string, TypeInfo> methods;
	for (const auto& method : node.methods)
	{
		ValidateParameterList(method.params, "method '" + method.name + "'");
		std::vector<TypeInfo> paramTypes = BuildFixedParamTypes(method.params);
		const auto variadicParamType = BuildVariadicParamType(method.params);
		size_t requiredArity = 0;
		for (const auto& param : method.params)
		{
			if (!param.isVariadic && !param.hasDefaultValue)
			{
				++requiredArity;
			}
		}
		std::unordered_set<std::string> raisedEffects;
		for (const auto& effectName : method.raisedEffects)
		{
			raisedEffects.insert(QualifyName(effectName));
		}
		methods.emplace(
			method.name,
			TypeInfo::Function(
				paramTypes,
				StringToType(method.returnType),
				requiredArity,
				raisedEffects,
				{},
				false,
				variadicParamType.has_value(),
				variadicParamType));
	}

	std::unordered_set<std::string> implementedInterfaces;
	for (const auto& interfaceName : node.implementedInterfaces)
	{
		const TypeInfo interfaceType = StringToType(interfaceName);
		if (interfaceType.kind != TypeKind::Interface || !interfaceType.methods)
		{
			throw std::runtime_error("Unknown interface in implements list: " + interfaceName);
		}
		implementedInterfaces.insert(interfaceType.name);
	}

	const TypeInfo structType = TypeInfo::Struct(
		QualifyName(node.name),
		std::move(fields),
		methods,
		implementedInterfaces);
	m_types.structs[structType.name] = structType;
	m_types.constructorSignatures[node.name] = ctorSignature;
	m_types.constructorSignatures[structType.name] = ctorSignature;
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
		++m_functionDepth;
		ScopeExit functionScope([this]() {
			--m_functionDepth;
		});
		const auto oldReturn = m_currentExpectedReturn;
		m_currentExpectedReturn = returnType;
		ScopeExit returnScope([this, oldReturn]() {
			m_currentExpectedReturn = oldReturn;
		});

		Define("self", structType);
		for (size_t i = 0; i < method.params.size(); ++i)
		{
			Define(method.params[i].name, DeclaredParameterType(method.params[i]));
		}
		for (const auto& context : method.contextRequirements)
		{
			Define(context.name, StringToType(context.typeName));
		}
		ValidateContractList(method.contracts, returnType, structType);

		if (method.body)
		{
			method.body->Accept(*this);
		}
	}
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(InterfaceDeclNode& node)
{
	if (!node.typeParams.empty())
	{
		m_typeParamScopes.emplace_back();
		ScopeExit typeParamScope([this]() {
			m_typeParamScopes.pop_back();
		});
		for (const auto& typeParam : node.typeParams)
		{
			std::vector<TypeInfo> constraints;
			for (const auto& constraintName : typeParam.constraints)
			{
				constraints.push_back(StringToType(constraintName));
			}
			m_typeParamScopes.back()[typeParam.name] = TypeInfo::TypeParameter(typeParam.name, std::move(constraints));
		}

		std::unordered_map<std::string, TypeInfo> methods;
		for (const auto& method : node.methods)
		{
			ValidateParameterList(method.params, "interface method '" + method.name + "'");
			std::vector<TypeInfo> paramTypes = BuildFixedParamTypes(method.params);
			const auto variadicParamType = BuildVariadicParamType(method.params);
			size_t requiredArity = 0;
			for (const auto& param : method.params)
			{
				if (!param.isVariadic && !param.hasDefaultValue)
				{
					++requiredArity;
				}
			}
			std::unordered_set<std::string> raisedEffects;
			for (const auto& effectName : method.raisedEffects)
			{
				raisedEffects.insert(QualifyName(effectName));
			}
			methods.emplace(
				method.name,
				TypeInfo::Function(
					paramTypes,
					StringToType(method.returnType),
					requiredArity,
					raisedEffects,
					{},
					false,
					variadicParamType.has_value(),
					variadicParamType));
		}

		const std::string qualifiedName = QualifyName(node.name);
		const TypeInfo templatedType = TypeInfo::Interface(qualifiedName, methods);
		m_types.genericInterfaces[node.name] = { node.typeParams, templatedType };
		m_types.genericInterfaces[qualifiedName] = { node.typeParams, templatedType };
		Define(node.name, templatedType);
		m_lastType = TypeInfo::Void();
		return;
	}
	std::unordered_map<std::string, TypeInfo> methods;
	for (const auto& method : node.methods)
	{
		ValidateParameterList(method.params, "interface method '" + method.name + "'");
		std::vector<TypeInfo> paramTypes = BuildFixedParamTypes(method.params);
		const auto variadicParamType = BuildVariadicParamType(method.params);
		size_t requiredArity = 0;
		for (const auto& param : method.params)
		{
			if (!param.isVariadic && !param.hasDefaultValue)
			{
				++requiredArity;
			}
		}
		std::unordered_set<std::string> raisedEffects;
		for (const auto& effectName : method.raisedEffects)
		{
			raisedEffects.insert(QualifyName(effectName));
		}
		methods.emplace(
			method.name,
			TypeInfo::Function(
				paramTypes,
				StringToType(method.returnType),
				requiredArity,
				raisedEffects,
				{},
				false,
				variadicParamType.has_value(),
				variadicParamType));
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
		ValidateParameterList(operation.params, "effect operation '" + operation.name + "'");
		std::vector<TypeInfo> paramTypes = BuildFixedParamTypes(operation.params);
		const auto variadicParamType = BuildVariadicParamType(operation.params);
		const TypeInfo opType = TypeInfo::Function(
			paramTypes,
			StringToType(operation.returnType),
			paramTypes.size(),
			{},
			{},
			false,
			variadicParamType.has_value(),
			variadicParamType);
		operations.emplace(operation.name, opType);
		Define(operation.name, opType);
	}

	const TypeInfo effectType = TypeInfo::Effect(QualifyName(node.name), std::move(operations));
	m_types.effects[effectType.name] = effectType;
	Define(node.name, effectType);
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(ActorDeclNode& node)
{
	if (!node.typeParams.empty())
	{
		m_typeParamScopes.emplace_back();
		ScopeExit typeParamScope([this]() {
			m_typeParamScopes.pop_back();
		});
		for (const auto& typeParam : node.typeParams)
		{
			std::vector<TypeInfo> constraints;
			for (const auto& constraintName : typeParam.constraints)
			{
				constraints.push_back(StringToType(constraintName));
			}
			m_typeParamScopes.back()[typeParam.name] = TypeInfo::TypeParameter(typeParam.name, std::move(constraints));
		}

		std::unordered_map<std::string, TypeInfo> fields;
		TypeRegistry::ConstructorSignature ctorSignature;
		ctorSignature.params.reserve(node.fields.size());
		bool seenDefaultField = false;
		for (const auto& field : node.fields)
		{
			const TypeInfo fieldType = StringToType(field.typeName);
			fields.emplace(field.name, fieldType);
			ctorSignature.params.emplace_back(field.name, fieldType);
			if (field.initializer)
			{
				seenDefaultField = true;
			}
			else
			{
				if (seenDefaultField)
				{
					throw std::runtime_error("Actor state fields with defaults must be trailing");
				}
				++ctorSignature.requiredArity;
			}
		}
		std::unordered_map<std::string, TypeInfo> methods;
		for (const auto& method : node.methods)
		{
			ValidateParameterList(method.params, "actor method '" + method.name + "'");
			std::vector<TypeInfo> paramTypes = BuildFixedParamTypes(method.params);
			const auto variadicParamType = BuildVariadicParamType(method.params);
			std::vector<std::pair<std::string, TypeInfo>> contextTypes;
			size_t requiredArity = 0;
			for (const auto& param : method.params)
			{
				if (!param.isVariadic && !param.hasDefaultValue)
				{
					++requiredArity;
				}
			}
			for (const auto& context : method.contextRequirements)
			{
				contextTypes.emplace_back(context.name, StringToType(context.typeName));
			}
			std::unordered_set<std::string> raisedEffects;
			for (const auto& effectName : method.raisedEffects)
			{
				raisedEffects.insert(QualifyName(effectName));
			}
			methods.emplace(
				method.name,
				TypeInfo::Function(
					paramTypes,
					StringToType(method.returnType),
					requiredArity,
					raisedEffects,
					contextTypes,
					false,
					variadicParamType.has_value(),
					variadicParamType));
		}

		const std::string qualifiedName = QualifyName(node.name);
		const TypeInfo templatedType = TypeInfo::Actor(qualifiedName, fields, methods);
		m_types.genericActors[node.name] = { node.typeParams, templatedType };
		m_types.genericActors[qualifiedName] = { node.typeParams, templatedType };
		m_types.constructorSignatures[node.name] = ctorSignature;
		m_types.constructorSignatures[qualifiedName] = ctorSignature;
		Define(node.name, templatedType);
		m_lastType = TypeInfo::Void();
		return;
	}
	std::unordered_map<std::string, TypeInfo> fields;
	TypeRegistry::ConstructorSignature ctorSignature;
	ctorSignature.params.reserve(node.fields.size());
	bool seenDefaultField = false;
	for (const auto& field : node.fields)
	{
		const TypeInfo fieldType = StringToType(field.typeName);
		ValidateSendable(fieldType, "Actor state field '" + field.name + "'");
		fields.emplace(field.name, fieldType);
		ctorSignature.params.emplace_back(field.name, fieldType);
		if (field.initializer)
		{
			seenDefaultField = true;
		}
		else
		{
			if (seenDefaultField)
			{
				throw std::runtime_error("Actor state fields with defaults must be trailing");
			}
			++ctorSignature.requiredArity;
		}
	}

	std::unordered_map<std::string, TypeInfo> methods;
	for (const auto& method : node.methods)
	{
		ValidateParameterList(method.params, "actor method '" + method.name + "'");
		std::vector<TypeInfo> paramTypes;
		paramTypes.reserve(method.params.size());
		const auto variadicParamType = BuildVariadicParamType(method.params);
		std::vector<std::pair<std::string, TypeInfo>> contextTypes;
		size_t requiredArity = 0;
		for (const auto& param : method.params)
		{
			if (param.isVariadic)
			{
				continue;
			}
			const TypeInfo paramType = StringToType(param.typeName);
			ValidateSendable(paramType, "Actor method argument '" + param.name + "'");
			paramTypes.push_back(paramType);
			if (!param.hasDefaultValue)
			{
				++requiredArity;
			}
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
		std::unordered_set<std::string> raisedEffects;
		for (const auto& effectName : method.raisedEffects)
		{
			raisedEffects.insert(QualifyName(effectName));
		}
		for (const auto& context : method.contextRequirements)
		{
			contextTypes.emplace_back(context.name, StringToType(context.typeName));
		}
		methods.emplace(
			method.name,
			TypeInfo::Function(
				paramTypes,
				returnType,
				requiredArity,
				raisedEffects,
				contextTypes,
				false,
				variadicParamType.has_value(),
				variadicParamType));
	}

	const TypeInfo actorType = TypeInfo::Actor(QualifyName(node.name), fields, methods);
	m_types.actors[actorType.name] = actorType;
	m_types.constructorSignatures[node.name] = ctorSignature;
	m_types.constructorSignatures[actorType.name] = ctorSignature;
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

		std::unordered_set<std::string> raisedEffects;
		for (const auto& effectName : method.raisedEffects)
		{
			raisedEffects.insert(QualifyName(effectName));
		}
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
			Define(method.params[i].name, DeclaredParameterType(method.params[i]));
		}
		for (const auto& context : method.contextRequirements)
		{
			Define(context.name, StringToType(context.typeName));
		}

		const auto oldReturn = m_currentExpectedReturn;
		++m_functionDepth;
		ScopeExit functionScope([this]() {
			--m_functionDepth;
		});
		m_currentExpectedReturn = StringToType(method.returnType);
		ScopeExit returnScope([this, oldReturn]() {
			m_currentExpectedReturn = oldReturn;
		});

		ValidateContractList(method.contracts, m_currentExpectedReturn, actorType);

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
	if (!node.typeParams.empty())
	{
		m_typeParamScopes.emplace_back();
		ScopeExit typeParamScope([this]() {
			m_typeParamScopes.pop_back();
		});
		for (const auto& typeParam : node.typeParams)
		{
			std::vector<TypeInfo> constraints;
			for (const auto& constraintName : typeParam.constraints)
			{
				constraints.push_back(StringToType(constraintName));
			}
			m_typeParamScopes.back()[typeParam.name] = TypeInfo::TypeParameter(typeParam.name, std::move(constraints));
		}
		std::string templatedName = enumName + "<";
		for (size_t i = 0; i < node.typeParams.size(); ++i)
		{
			if (i != 0)
			{
				templatedName += ", ";
			}
			templatedName += node.typeParams[i].name;
		}
		templatedName += ">";
		m_types.genericEnums[node.name] = { node.typeParams, TypeInfo::Enum(templatedName) };
		m_types.genericEnums[enumName] = { node.typeParams, TypeInfo::Enum(templatedName) };
		for (size_t index = 0; index < node.variants.size(); ++index)
		{
			std::vector<TypeInfo> argTypes;
			for (const auto& argTypeName : node.variants[index].argTypes)
			{
				argTypes.push_back(StringToType(argTypeName));
			}
			const TypeInfo ctorType = TypeInfo::EnumConstructor(
				templatedName,
				static_cast<int>(index),
				std::move(argTypes));
			m_types.enumConstructors[QualifyName(node.variants[index].name)] = ctorType;
			Define(node.variants[index].name, ctorType);
		}
		m_lastType = TypeInfo::Void();
		return;
	}

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
