#include "../common/EffectResumeAnalysis.h"
#include "../common/SemanticAnalyzerSupport.h"
#include "SemanticAnalyzer.h"

#include <algorithm>
#include <functional>
#include <ranges>
#include <stdexcept>

using SemanticAnalyzerDetail::FormatUndefined;
using SemanticAnalyzerDetail::ScopeExit;
using SemanticAnalyzerDetail::SplitGenericName;
using SemanticAnalyzerDetail::SubstituteTypeString;

bool SemanticAnalyzer::HasVariadicParameter(const std::vector<Parameter>& params) const
{
	return std::ranges::any_of(params, [](const Parameter& param) { return param.isVariadic; });
}

const Parameter* SemanticAnalyzer::FindVariadicParameter(const std::vector<Parameter>& params) const
{
	const auto it = std::ranges::find_if(params, [](const Parameter& param) { return param.isVariadic; });
	return it == params.end() ? nullptr : &(*it);
}

std::vector<SemanticAnalyzer::TypeInfo> SemanticAnalyzer::BuildFixedParamTypes(const std::vector<Parameter>& params) const
{
	std::vector<TypeInfo> types;
	for (const auto& param : params)
	{
		if (!param.isVariadic)
		{
			types.push_back(StringToType(param.typeName));
		}
	}
	return types;
}

std::optional<SemanticAnalyzer::TypeInfo> SemanticAnalyzer::BuildVariadicParamType(const std::vector<Parameter>& params) const
{
	if (const auto* param = FindVariadicParameter(params))
	{
		return StringToType(param->typeName);
	}
	return std::nullopt;
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::DeclaredParameterType(const Parameter& param) const
{
	const TypeInfo baseType = StringToType(param.typeName);
	return param.isVariadic ? TypeInfo::ArrayOf(baseType) : baseType;
}

void SemanticAnalyzer::ValidateParameterList(const std::vector<Parameter>& params, const std::string& ownerName) const
{
	bool seenDefault = false;
	bool seenVariadic = false;
	for (const auto& param : params)
	{
		if (param.isVariadic)
		{
			if (seenVariadic)
			{
				throw std::runtime_error("Only one variadic parameter is allowed in " + ownerName);
			}
			if (param.hasDefaultValue)
			{
				throw std::runtime_error("Variadic parameter cannot have a default value in " + ownerName);
			}
			if (seenDefault)
			{
				throw std::runtime_error("Variadic parameters cannot follow default parameters in " + ownerName);
			}
			seenVariadic = true;
			continue;
		}
		if (seenVariadic)
		{
			throw std::runtime_error("Variadic parameter must be the final parameter in " + ownerName);
		}
		if (param.hasDefaultValue)
		{
			seenDefault = true;
			continue;
		}
		if (seenDefault)
		{
			throw std::runtime_error("Parameters with defaults must be trailing in " + ownerName);
		}
	}
}

void SemanticAnalyzer::Visit(BlockNode& node)
{
	m_environment.PushScope();
	const ScopeExit scopeExit([this]() {
		m_environment.PopScope();
	});

	for (auto& stmt : node.statements)
	{
		if (stmt)
		{
			stmt->Accept(*this);
		}
	}
}

void SemanticAnalyzer::Visit(ExportDeclNode& node)
{
	if (!m_currentModule.empty())
	{
		if (!node.exportedName.empty())
		{
			m_modules.exports[m_currentModule].insert(node.exportedName);
		}
		else if (node.declaration)
		{
			if (const auto* varDecl = dynamic_cast<const VarDeclNode*>(node.declaration.get()))
			{
				m_modules.exports[m_currentModule].insert(varDecl->name);
			}
			else if (const auto* fnDecl = dynamic_cast<const FunctionDeclNode*>(node.declaration.get()))
			{
				m_modules.exports[m_currentModule].insert(fnDecl->name);
			}
			else if (const auto* typeAlias = dynamic_cast<const TypeAliasNode*>(node.declaration.get()))
			{
				m_modules.exports[m_currentModule].insert(typeAlias->name);
			}
			else if (const auto* structDecl = dynamic_cast<const StructDeclNode*>(node.declaration.get()))
			{
				m_modules.exports[m_currentModule].insert(structDecl->name);
			}
			else if (const auto* enumDecl = dynamic_cast<const EnumDeclNode*>(node.declaration.get()))
			{
				m_modules.exports[m_currentModule].insert(enumDecl->name);
			}
			else if (const auto* interfaceDecl = dynamic_cast<const InterfaceDeclNode*>(node.declaration.get()))
			{
				m_modules.exports[m_currentModule].insert(interfaceDecl->name);
			}
			else if (const auto* effectDecl = dynamic_cast<const EffectDeclNode*>(node.declaration.get()))
			{
				m_modules.exports[m_currentModule].insert(effectDecl->name);
			}
			else if (const auto* actorDecl = dynamic_cast<const ActorDeclNode*>(node.declaration.get()))
			{
				m_modules.exports[m_currentModule].insert(actorDecl->name);
			}
		}
	}

	if (node.declaration)
	{
		node.declaration->Accept(*this);
		return;
	}

	if (!node.exportedName.empty())
	{
		m_lastType = TypeInfo::Void();
		return;
	}
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(IfStatementNode& node)
{
	(void)VisitAndGet(node.condition.get());
	if (node.thenBlock)
	{
		node.thenBlock->Accept(*this);
	}
	if (node.elseBlock)
	{
		node.elseBlock->Accept(*this);
	}
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(WhileStatementNode& node)
{
	(void)VisitAndGet(node.condition.get());
	if (node.body)
	{
		node.body->Accept(*this);
	}
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(FunctionDeclNode& node)
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

	ValidateParameterList(node.params, "function '" + node.name + "'");
	std::vector<TypeInfo> paramTypes = BuildFixedParamTypes(node.params);
	const auto variadicParamType = BuildVariadicParamType(node.params);
	std::vector<std::pair<std::string, TypeInfo>> contextTypes;
	size_t requiredArity = 0;
	for (const auto& param : node.params)
	{
		if (param.isVariadic)
		{
			continue;
		}
		const TypeInfo paramType = StringToType(param.typeName);
		if (param.hasDefaultValue)
		{
			if (param.defaultValue)
			{
				const TypeInfo defaultType = VisitAndGet(param.defaultValue.get());
				if (!IsAssignable(paramType, defaultType))
				{
					throw std::runtime_error("Default value type mismatch for parameter '" + param.name + "'");
				}
			}
		}
		else
		{
			++requiredArity;
		}
	}
	const TypeInfo returnType = StringToType(node.returnType);
	for (const auto& context : node.contextRequirements)
	{
		contextTypes.emplace_back(context.name, StringToType(context.typeName));
	}

	std::unordered_set<std::string> qualifiedEffects;
	for (const auto& effectName : node.raisedEffects)
	{
		qualifiedEffects.insert(QualifyName(effectName));
	}
	const TypeInfo funcType = TypeInfo::Function(
		paramTypes,
		returnType,
		requiredArity,
		qualifiedEffects,
		contextTypes,
		node.isComptime,
		variadicParamType.has_value(),
		variadicParamType);
	Define(node.name, funcType);
	if (!m_currentModule.empty())
	{
		m_modules.memberTypes[m_currentModule][node.name] = funcType;
	}

	m_environment.PushScope();
	ScopeExit envScope([this]() {
		m_environment.PopScope();
	});
	if (node.isComptime)
	{
		++m_comptimeDepth;
	}
	++m_functionDepth;
	ScopeExit comptimeScope([this, &node]() {
		if (node.isComptime)
		{
			--m_comptimeDepth;
		}
	});
	ScopeExit functionScope([this]() {
		--m_functionDepth;
	});
	m_activeRaisedEffects.emplace_back();
	m_activeRaisedEffects.back() = qualifiedEffects;
	ScopeExit raisedEffectsScope([this]() {
		m_activeRaisedEffects.pop_back();
	});
	const auto oldReturn = m_currentExpectedReturn;
	m_currentExpectedReturn = returnType;
	ScopeExit returnScope([this, oldReturn]() {
		m_currentExpectedReturn = oldReturn;
	});

	for (size_t i = 0; i < node.params.size(); ++i)
	{
		Define(node.params[i].name, DeclaredParameterType(node.params[i]));
	}
	for (const auto& context : node.contextRequirements)
	{
		Define(context.name, StringToType(context.typeName));
	}
	ValidateContractList(node.contracts, returnType);

	if (node.body)
	{
		node.body->Accept(*this);
	}
}

void SemanticAnalyzer::Visit(FunctionExprNode& node)
{
	ValidateParameterList(node.params, "lambda");
	std::vector<TypeInfo> paramTypes = BuildFixedParamTypes(node.params);
	const auto variadicParamType = BuildVariadicParamType(node.params);
	size_t requiredArity = 0;
	for (const auto& param : node.params)
	{
		if (param.isVariadic)
		{
			continue;
		}
		const TypeInfo paramType = StringToType(param.typeName);
		if (param.hasDefaultValue)
		{
			if (param.defaultValue)
			{
				const TypeInfo defaultType = VisitAndGet(param.defaultValue.get());
				if (!IsAssignable(paramType, defaultType))
				{
					throw std::runtime_error("Default value type mismatch for lambda parameter '" + param.name + "'");
				}
			}
		}
		else
		{
			++requiredArity;
		}
	}

	const TypeInfo returnType = StringToType(node.returnType);
	TypeInfo effectiveReturnType = returnType;

	m_environment.PushScope();
	ScopeExit envScope([this]() {
		m_environment.PopScope();
	});
	++m_functionDepth;
	ScopeExit functionScope([this]() {
		--m_functionDepth;
	});
	m_activeRaisedEffects.emplace_back();
	for (const auto& effectName : node.raisedEffects)
	{
		m_activeRaisedEffects.back().insert(QualifyName(effectName));
	}
	ScopeExit raisedEffectsScope([this]() {
		m_activeRaisedEffects.pop_back();
	});
	const auto oldReturn = m_currentExpectedReturn;
	m_currentExpectedReturn = returnType;
	ScopeExit returnScope([this, oldReturn]() {
		m_currentExpectedReturn = oldReturn;
	});

	for (size_t i = 0; i < node.params.size(); ++i)
	{
		Define(node.params[i].name, DeclaredParameterType(node.params[i]));
	}

	if (node.body)
	{
		node.body->Accept(*this);
	}

	if (effectiveReturnType.kind == TypeKind::Unknown)
	{
		if (dynamic_cast<BlockNode*>(node.body.get()))
		{
			effectiveReturnType = InferFunctionBodyReturnType(node.body.get());
		}
		else
		{
			effectiveReturnType = VisitAndGet(node.body.get());
		}
	}

	m_lastType = TypeInfo::Function(
		paramTypes,
		effectiveReturnType,
		requiredArity,
		m_activeRaisedEffects.back(),
		{},
		false,
		variadicParamType.has_value(),
		variadicParamType);
}

void SemanticAnalyzer::Visit(CallNode& node)
{
	CallAnalyzer{ *this }.Analyze(node);
}

void SemanticAnalyzer::Visit(MemberAccessNode& node)
{
	const auto objectType = VisitAndGet(node.object.get());
	if (objectType.kind == TypeKind::Module)
	{
		if (const auto actualMember = ResolveModuleMemberType(objectType.name, node.member); actualMember)
		{
			m_lastType = *actualMember;
			return;
		}
		if (const auto it = m_types.structs.find(objectType.name + "." + node.member);
			it != m_types.structs.end() && IsModuleMemberExported(objectType.name, node.member))
		{
			m_lastType = it->second;
			return;
		}
		if (const auto it = m_types.enumConstructors.find(objectType.name + "." + node.member);
			it != m_types.enumConstructors.end() && IsModuleMemberExported(objectType.name, node.member))
		{
			m_lastType = it->second;
			return;
		}
		if (ModuleDefinesMember(objectType.name, node.member))
		{
			throw std::runtime_error("Module member is not exported: " + objectType.name + "." + node.member);
		}
		m_lastType = TypeInfo::Unknown();
		return;
	}

	if (objectType.kind == TypeKind::Struct)
	{
		if (objectType.fields && objectType.fields->contains(node.member))
		{
			m_lastType = objectType.fields->at(node.member);
			return;
		}
		if (objectType.methods && objectType.methods->contains(node.member))
		{
			m_lastType = objectType.methods->at(node.member);
			return;
		}
		if ((!objectType.fields || !objectType.fields->contains(node.member))
			&& (!objectType.methods || !objectType.methods->contains(node.member)))
		{
			throw std::runtime_error("Unknown struct member: " + node.member);
		}
	}

	if (objectType.kind == TypeKind::Actor)
	{
		if (!objectType.methods || !objectType.methods->contains(node.member))
		{
			throw std::runtime_error("Unknown actor member: " + node.member);
		}
		m_lastType = objectType.methods->at(node.member);
		return;
	}

	if (objectType.kind == TypeKind::Interface)
	{
		if (!objectType.methods || !objectType.methods->contains(node.member))
		{
			throw std::runtime_error("Unknown interface member: " + node.member);
		}
		m_lastType = objectType.methods->at(node.member);
		return;
	}

	if (objectType.kind == TypeKind::Enum && node.member == "tag")
	{
		m_lastType = TypeInfo::Int();
		return;
	}

	throw std::runtime_error("Member access is only supported on imported modules");
}

void SemanticAnalyzer::Visit(ModuleDeclNode& node)
{
	m_currentModule = node.qualifiedName;
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(ImportDeclNode& node)
{
	Define(node.alias, TypeInfo::Module(node.qualifiedName));
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(ReturnNode& node)
{
	if (m_functionDepth == 0 && m_comptimeDepth == 0)
	{
		throw std::runtime_error("return is only allowed inside a function or comptime block");
	}

	TypeInfo actualType = TypeInfo::Void();
	if (node.value)
	{
		actualType = VisitAndGet(node.value.get());
	}

	if (m_currentExpectedReturn.kind != TypeKind::Unknown
		&& actualType.kind != TypeKind::Unknown
		&& !IsAssignable(m_currentExpectedReturn, actualType))
	{
		throw std::runtime_error("Return type mismatch");
	}

	m_lastType = TypeInfo::Never();
}

void SemanticAnalyzer::Visit(PrintNode& node)
{
	RejectRuntimeOnlyInComptime("print");
	(void)VisitAndGet(node.value.get());
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(UnsafeNode& node)
{
	RejectRuntimeOnlyInComptime("unsafe");
	++m_unsafeDepth;
	ScopeExit unsafeScope([this]() {
		--m_unsafeDepth;
	});
	if (node.body)
	{
		node.body->Accept(*this);
	}
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(TransactionNode& node)
{
	RejectRuntimeOnlyInComptime("transaction");
	if (node.regions.empty())
	{
		throw std::runtime_error("transaction requires at least one region");
	}

	m_activeTransactions.emplace_back();
	for (const auto& region : node.regions)
	{
		EnsureDeclared(region.name);
		if (region.usesShared && !m_sharedVariables.contains(region.name))
		{
			throw std::runtime_error("transaction(shared ...) requires a shared variable: " + region.name);
		}
		m_activeTransactions.back().insert(region.name);
	}
	ScopeExit transactionScope([this]() {
		m_activeTransactions.pop_back();
	});
	if (node.body)
	{
		node.body->Accept(*this);
	}
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(HandleNode& node)
{
	RejectRuntimeOnlyInComptime("effect handlers");
	auto resolveHandlerEffects = [&](const std::string& operationName) {
		std::vector<std::string> candidates = ResolveAccessibleEffectsForOperation(operationName);
		if (!candidates.empty())
		{
			return candidates;
		}
		for (const auto& [effectName, effectType] : m_types.effects)
		{
			if (effectType.methods && effectType.methods->contains(operationName))
			{
				candidates.push_back(effectName);
			}
		}
		std::ranges::sort(candidates);
		candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
		return candidates;
	};
	std::unordered_set<std::string> handledEffects;
	for (auto& handler : node.handlers)
	{
		ValidateParameterList(handler.params, "effect handler '" + handler.effectName + "'");
		const std::vector<std::string> candidateEffects = resolveHandlerEffects(handler.effectName);
		if (candidateEffects.size() > 1)
		{
			throw std::runtime_error("Ambiguous handled effect operation: " + handler.effectName);
		}
		const std::string effectName = candidateEffects.empty()
			? QualifyName(handler.effectName)
			: candidateEffects.front();
		handler.effectKey = effectName + "." + handler.effectName;
		handledEffects.insert(effectName);
	}
	m_activeHandledEffects.push_back(std::move(handledEffects));
	ScopeExit handleScope([this]() {
		m_activeHandledEffects.pop_back();
	});

	if (node.expression)
	{
		m_lastType = VisitAndGet(node.expression.get());
	}
	else
	{
		m_lastType = TypeInfo::Void();
	}

	for (auto& handler : node.handlers)
	{
		const std::vector<std::string> candidateEffects = resolveHandlerEffects(handler.effectName);
		if (candidateEffects.size() > 1)
		{
			throw std::runtime_error("Ambiguous handled effect operation: " + handler.effectName);
		}
		const std::string effectName = candidateEffects.empty()
			? QualifyName(handler.effectName)
			: candidateEffects.front();
		handler.effectKey = effectName + "." + handler.effectName;
		const auto effectIt = m_types.effects.find(effectName);
		if (effectIt == m_types.effects.end() || !effectIt->second.methods)
		{
			throw std::runtime_error("Unknown handled effect: " + handler.effectName);
		}
		const auto opIt = effectIt->second.methods->find(handler.effectName);
		if (opIt == effectIt->second.methods->end())
		{
			throw std::runtime_error("Unknown effect operation in handler: " + handler.effectName);
		}
		const size_t expectedFixedParams = opIt->second.params.size();
		const size_t actualFixedParams = std::ranges::count_if(
			handler.params,
			[](const Parameter& param) { return !param.isVariadic; });
		if (expectedFixedParams != actualFixedParams
			|| opIt->second.variadic != HasVariadicParameter(handler.params))
		{
			throw std::runtime_error("Effect handler arity mismatch for " + handler.effectName);
		}
		const TypeInfo resumeType = opIt->second.ret ? *opIt->second.ret : TypeInfo::Void();
		if (EffectResumeAnalysis::CountResumeCalls(handler.body.get()) != 1)
		{
			throw std::runtime_error(
				"Effect handler '"
				+ handler.effectName
				+ "' must call resume(value) exactly once");
		}

		m_environment.PushScope();
		m_activeResumeTypes.push_back(resumeType);
		ScopeExit handlerScope([this]() {
			m_activeResumeTypes.pop_back();
			m_environment.PopScope();
		});
		for (size_t i = 0; i < handler.params.size(); ++i)
		{
			if (handler.params[i].isVariadic)
			{
				if (!opIt->second.variadicParam)
				{
					throw std::runtime_error("Missing variadic effect parameter type for " + handler.effectName);
				}
				Define(handler.params[i].name, TypeInfo::ArrayOf(*opIt->second.variadicParam));
				continue;
			}
			Define(handler.params[i].name, opIt->second.params[i]);
		}
		if (handler.body)
		{
			handler.body->Accept(*this);
		}
	}
}

void SemanticAnalyzer::Visit(ArrayLiteralNode& node)
{
	TypeInfo elemType = TypeInfo::Unknown();

	for (auto& el : node.elements)
	{
		const auto elType = VisitAndGet(el.get());
		if (elemType.kind == TypeKind::Unknown)
		{
			elemType = elType;
		}
		else if (elType.kind != TypeKind::Unknown && elType.kind != elemType.kind)
		{
			elemType = TypeInfo::Unknown();
		}
	}

	m_lastType = TypeInfo::ArrayOf(elemType);
}

void SemanticAnalyzer::Visit(MapLiteralNode& node)
{
	const TypeInfo keyType = StringToType(node.keyTypeName);
	const TypeInfo valueType = StringToType(node.valueTypeName);

	if (keyType.kind != TypeKind::String
		&& keyType.kind != TypeKind::Int
		&& keyType.kind != TypeKind::Bool
		&& keyType.kind != TypeKind::TypeParameter
		&& keyType.kind != TypeKind::Unknown)
	{
		throw std::runtime_error("Map keys must be string, int, or bool");
	}

	for (auto& [keyExpr, valueExpr] : node.entries)
	{
		const TypeInfo actualKeyType = VisitAndGet(keyExpr.get());
		if (actualKeyType.kind != TypeKind::Unknown && !IsAssignable(keyType, actualKeyType))
		{
			throw std::runtime_error("Map literal key type mismatch");
		}

		const TypeInfo actualValueType = VisitAndGet(valueExpr.get());
		if (actualValueType.kind != TypeKind::Unknown && !IsAssignable(valueType, actualValueType))
		{
			throw std::runtime_error("Map literal value type mismatch");
		}
	}

	m_lastType = TypeInfo::MapOf(keyType, valueType);
}

void SemanticAnalyzer::Visit(IndexNode& node)
{
	auto containerType = VisitAndGet(node.container.get());
	auto idxType = VisitAndGet(node.index.get());

	if (containerType.kind == TypeKind::Enum)
	{
		if (idxType.kind != TypeKind::Unknown && idxType.kind != TypeKind::Int)
		{
			throw std::runtime_error("Enum argument index must be integer");
		}
		m_lastType = TypeInfo::Unknown();
		return;
	}

	if (containerType.kind == TypeKind::Map)
	{
		if (containerType.key && idxType.kind != TypeKind::Unknown && !IsAssignable(*containerType.key, idxType))
		{
			throw std::runtime_error("Map key type mismatch");
		}
		m_lastType = (containerType.element) ? *containerType.element : TypeInfo::Unknown();
		return;
	}

	if (containerType.kind != TypeKind::Array && containerType.kind != TypeKind::Unknown)
	{
		throw std::runtime_error("Indexing requires array or map");
	}
	if (idxType.kind != TypeKind::Unknown && !IsPrimitiveNumeric(idxType))
	{
		throw std::runtime_error("Index must be numeric");
	}

	m_lastType = (containerType.kind == TypeKind::Array && containerType.element)
		? *containerType.element
		: TypeInfo::Unknown();
}

void SemanticAnalyzer::Visit(IterNode& node)
{
	const auto collType = VisitAndGet(node.collection.get());
	if (collType.kind != TypeKind::Array && collType.kind != TypeKind::Unknown)
	{
		throw std::runtime_error("Object is not iterable (expected array)");
	}

	TypeInfo elemType = TypeInfo::Unknown();
	if (collType.kind == TypeKind::Array && collType.element)
	{
		elemType = *collType.element;
	}

	for (auto& adapter : node.adapters)
	{
		switch (adapter.kind)
		{
		case IterAdapterKind::Drop:
		case IterAdapterKind::Take: {
			if (const auto adapterType = VisitAndGet(adapter.argument.get());
				adapterType.kind != TypeKind::Unknown && adapterType.kind != TypeKind::Int)
			{
				throw std::runtime_error("Iterator drop/take expects an integer argument");
			}
			break;
		}
		case IterAdapterKind::Reverse:
			break;
		case IterAdapterKind::Filter: {
			if (const auto adapterType = VisitAndGet(adapter.argument.get());
				adapterType.kind != TypeKind::Unknown)
			{
				if (adapterType.kind != TypeKind::Function
					|| adapterType.params.size() != 1
					|| !adapterType.ret
					|| (adapterType.ret->kind != TypeKind::Bool
						&& adapterType.ret->kind != TypeKind::Unknown))
				{
					throw std::runtime_error("Iterator filter expects a predicate function");
				}
				if (elemType.kind != TypeKind::Unknown
					&& adapterType.params[0].kind != TypeKind::Unknown
					&& !adapterType.params[0].Equals(elemType))
				{
					throw std::runtime_error("Iterator filter predicate parameter type mismatch");
				}
			}
			break;
		}
		case IterAdapterKind::Transform: {
			if (const auto adapterType = VisitAndGet(adapter.argument.get());
				adapterType.kind != TypeKind::Unknown)
			{
				if (adapterType.kind != TypeKind::Function
					|| adapterType.params.size() != 1
					|| !adapterType.ret)
				{
					throw std::runtime_error("Iterator transform expects a unary function");
				}
				if (elemType.kind != TypeKind::Unknown
					&& adapterType.params[0].kind != TypeKind::Unknown
					&& !adapterType.params[0].Equals(elemType))
				{
					throw std::runtime_error("Iterator transform function parameter type mismatch");
				}
				elemType = *adapterType.ret;
			}
			break;
		}
		}
	}

	Define(node.varName, elemType);
	if (node.body)
	{
		node.body->Accept(*this);
	}

	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(LeafNode& node)
{
	if (node.type == "integer_literal")
	{
		m_lastType = TypeInfo::Int();
	}
	else if (node.type == "float_literal")
	{
		m_lastType = TypeInfo::Float();
	}
	else if (node.type == "string_literal")
	{
		m_lastType = TypeInfo::String();
	}
	else if (node.type == "null")
	{
		m_lastType = TypeInfo::Void();
	}
	else if (node.type == "true" || node.type == "false")
	{
		m_lastType = TypeInfo::Bool();
	}
	else
	{
		m_lastType = TypeInfo::Unknown();
	}
}

void SemanticAnalyzer::Visit(RawNode& node)
{
	TypeInfo lastNonUnknown = TypeInfo::Unknown();
	for (auto& c : node.children)
	{
		if (c)
		{
			c->Accept(*this);
			if (m_lastType.kind != TypeKind::Unknown)
			{
				lastNonUnknown = m_lastType;
			}
		}
	}
	if (node.children.empty())
	{
		m_lastType = TypeInfo::Unknown();
	}
	else if (lastNonUnknown.kind != TypeKind::Unknown)
	{
		m_lastType = std::move(lastNonUnknown);
	}
}

void SemanticAnalyzer::Visit(ComptimeNode& node)
{
	++m_comptimeDepth;
	ScopeExit comptimeScope([this] { --m_comptimeDepth; });
	if (node.body)
	{
		m_lastType = VisitAndGet(node.body.get());
		return;
	}
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(ContractNode& node)
{
	if (node.expression)
	{
		const TypeInfo contractType = VisitAndGet(node.expression.get());
		if (contractType.kind != TypeKind::Unknown
			&& contractType.kind != TypeKind::Bool
			&& contractType.kind != TypeKind::Never)
		{
			throw std::runtime_error("Contract expression must be boolean");
		}
		m_lastType = contractType;
		return;
	}
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::ValidateContractList(
	const std::vector<std::unique_ptr<ContractNode>>& contracts,
	const std::optional<TypeInfo>& ensuresResult,
	const std::optional<TypeInfo>& selfType)
{
	if (contracts.empty())
	{
		return;
	}

	m_environment.PushScope();
	ScopeExit contractScope([this] {
		m_environment.PopScope();
	});

	if (ensuresResult.has_value())
	{
		Define("result", *ensuresResult, true);
	}
	if (selfType.has_value())
	{
		Define("self", *selfType, true);
	}

	for (const auto& contract : contracts)
	{
		if (!contract)
		{
			continue;
		}
		contract->Accept(*this);
	}
}

void SemanticAnalyzer::RejectRuntimeOnlyInComptime(const std::string& what) const
{
	if (m_comptimeDepth > 0)
	{
		throw std::runtime_error("Runtime-only operation is not allowed in comptime context: " + what);
	}
}
