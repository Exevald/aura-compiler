#include "../SyncStaticAnalyzer.h"
#include "../common/SemanticAnalyzerSupport.h"
#include "SemanticAnalyzer.h"

#include <cctype>
#include <functional>
#include <ranges>
#include <stdexcept>

using SemanticAnalyzerDetail::FormatUndefined;
using SemanticAnalyzerDetail::ScopeExit;
using SemanticAnalyzerDetail::SplitGenericName;
using SemanticAnalyzerDetail::SubstituteTypeString;

void SemanticAnalyzer::Visit(BlockNode& node)
{
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

	std::vector<TypeInfo> paramTypes;
	for (auto& [name, typeName] : node.params)
	{
		(void)name;
		paramTypes.push_back(StringToType(typeName));
	}
	const TypeInfo returnType = StringToType(node.returnType);

	const TypeInfo funcType = TypeInfo::Function(paramTypes, returnType);
	Define(node.name, funcType);
	if (!m_currentModule.empty())
	{
		m_modules.memberTypes[m_currentModule][node.name] = funcType;
	}

	m_environment.PushScope();
	ScopeExit envScope([this]() {
		m_environment.PopScope();
	});
	m_activeRaisedEffects.emplace_back(node.raisedEffects.begin(), node.raisedEffects.end());
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
		Define(node.params[i].name, paramTypes[i]);
	}

	if (node.body)
	{
		node.body->Accept(*this);
	}
}

void SemanticAnalyzer::Visit(FunctionExprNode& node)
{
	std::vector<TypeInfo> paramTypes;
	for (auto& [name, typeName] : node.params)
	{
		(void)name;
		paramTypes.push_back(StringToType(typeName));
	}

	const TypeInfo returnType = StringToType(node.returnType);
	TypeInfo effectiveReturnType = returnType;

	m_environment.PushScope();
	ScopeExit envScope([this]() {
		m_environment.PopScope();
	});
	m_activeRaisedEffects.emplace_back();
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
		Define(node.params[i].name, paramTypes[i]);
	}

	if (node.body)
	{
		node.body->Accept(*this);
	}

	if (effectiveReturnType.kind == TypeKind::Unknown)
	{
		effectiveReturnType = InferFunctionBodyReturnType(node.body.get());
	}

	m_lastType = TypeInfo::Function(paramTypes, effectiveReturnType);
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
	TypeInfo actualType = TypeInfo::Void();
	if (node.value)
	{
		actualType = VisitAndGet(node.value.get());
	}

	if (m_currentExpectedReturn.kind != TypeKind::Unknown
		&& actualType.kind != TypeKind::Unknown
		&& !m_currentExpectedReturn.Equals(actualType))
	{
		if (!(m_currentExpectedReturn.kind == TypeKind::Float
				&& actualType.kind == TypeKind::Int))
		{
			throw std::runtime_error("Return type mismatch");
		}
	}
}

void SemanticAnalyzer::Visit(PrintNode& node)
{
	(void)VisitAndGet(node.value.get());
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(UnsafeNode& node)
{
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
	m_activeTransactions.emplace_back();
	m_activeTransactions.back().insert(node.regionName);
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
	std::unordered_set<std::string> handledEffects;
	for (const auto& handler : node.handlers)
	{
		std::string effectName = handler.effectName;
		if (const auto it = m_types.effectOperations.find(handler.effectName); it != m_types.effectOperations.end())
		{
			effectName = it->second;
		}
		else
		{
			effectName = QualifyName(handler.effectName);
		}
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

	for (const auto& handler : node.handlers)
	{
		std::string effectName = handler.effectName;
		if (const auto it = m_types.effectOperations.find(handler.effectName); it != m_types.effectOperations.end())
		{
			effectName = it->second;
		}
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
		if (opIt->second.params.size() != handler.params.size())
		{
			throw std::runtime_error("Effect handler arity mismatch for " + handler.effectName);
		}

		m_environment.PushScope();
		ScopeExit handlerScope([this]() {
			m_environment.PopScope();
		});
		for (size_t i = 0; i < handler.params.size(); ++i)
		{
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

	if (containerType.kind != TypeKind::Array && containerType.kind != TypeKind::Unknown)
	{
		throw std::runtime_error("Indexing requires array");
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
	auto collType = VisitAndGet(node.collection.get());
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
			const auto adapterType = VisitAndGet(adapter.argument.get());
			if (adapterType.kind != TypeKind::Unknown && adapterType.kind != TypeKind::Int)
			{
				throw std::runtime_error("Iterator drop/take expects an integer argument");
			}
			break;
		}
		case IterAdapterKind::Reverse:
			break;
		case IterAdapterKind::Filter: {
			const auto adapterType = VisitAndGet(adapter.argument.get());
			if (adapterType.kind != TypeKind::Unknown)
			{
				if (adapterType.kind != TypeKind::Function
					|| adapterType.params.size() != 1
					|| !adapterType.ret
					|| (adapterType.ret->kind != TypeKind::Bool && adapterType.ret->kind != TypeKind::Unknown))
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
			const auto adapterType = VisitAndGet(adapter.argument.get());
			if (adapterType.kind != TypeKind::Unknown)
			{
				if (adapterType.kind != TypeKind::Function || adapterType.params.size() != 1 || !adapterType.ret)
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
	if (node.body)
	{
		node.body->Accept(*this);
	}
	m_lastType = TypeInfo::Void();
}
