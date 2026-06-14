#include "../../builtin/BuiltinModuleRegistry.h"
#include "../SyncStaticAnalyzer.h"
#include "../common/SemanticAnalyzerSupport.h"
#include "SemanticAnalyzer.h"

#include <algorithm>
#include <ranges>
#include <stdexcept>

using SemanticAnalyzerDetail::FormatUndefined;
using SemanticAnalyzerDetail::ScopeExit;
using SemanticAnalyzerDetail::SplitGenericName;
using SemanticAnalyzerDetail::SubstituteTypeString;

void SemanticAnalyzer::Analyze(ASTNode* root)
{
	m_lastType = TypeInfo::Unknown();
	m_environment.Reset();
	m_types.Clear();
	m_modules.Clear();
	m_currentModule.clear();
	m_typeParamScopes.clear();
	m_activeResumeTypes.clear();
	m_unsafeDepth = 0;
	RegisterBuiltinTypes();
	RegisterBuiltinModules();

	if (root)
	{
		root->Accept(*this);
	}

	SyncStaticAnalyzer syncAnalyzer;
	syncAnalyzer.Analyze(root);
}

void SemanticAnalyzer::RegisterBuiltinTypes()
{
	auto registerOpaqueStruct = [&](const std::string& qualifiedName) {
		m_types.structs[qualifiedName] = TypeInfo::Struct(qualifiedName, {}, {});
	};

	registerOpaqueStruct("std.context.Context");
	registerOpaqueStruct("std.sync.Thread");
	registerOpaqueStruct("std.sync.Mutex");
	registerOpaqueStruct("std.service.Store");
	registerOpaqueStruct("std.net.Listener");
	registerOpaqueStruct("std.net.Connection");
	registerOpaqueStruct("std.mysql.Connection");
	registerOpaqueStruct("std.mysql.Pool");
	registerOpaqueStruct("std.mysql.Statement");
	registerOpaqueStruct("std.mysql.Result");
	registerOpaqueStruct("std.mysql.Row");
	registerOpaqueStruct("std.mq.rabbitmq.Connection");
	registerOpaqueStruct("std.mq.rabbitmq.Message");
	registerOpaqueStruct("std.http.server.Request");
	registerOpaqueStruct("std.concurrent.waitgroup.WaitGroup");
	registerOpaqueStruct("std.concurrent.errorgroup.ErrorGroup");

	m_types.genericEnums["std.option.Option"] = {
		{ TypeParameterDecl{ "T", {} } },
		TypeInfo::Enum("std.option.Option")
	};
	m_types.genericEnums["std.result.Result"] = {
		{ TypeParameterDecl{ "T", {} }, TypeParameterDecl{ "E", {} } },
		TypeInfo::Enum("std.result.Result")
	};
}

void SemanticAnalyzer::RegisterBuiltinModules()
{
	for (const auto& module : Aura::Builtin::BuiltinModules())
	{
		if (!module.installNativeRuntime && (!module.sourcePath.empty() || !module.auraSource.empty()))
		{
			continue;
		}

		for (const auto& function : module.functions)
		{
			std::vector<TypeInfo> params;
			params.reserve(function.params.size());
			for (const auto paramType : function.params)
			{
				params.push_back(StringToType(std::string(paramType)));
			}

			std::optional<TypeInfo> variadicParam = std::nullopt;
			if (function.variadicParam.has_value())
			{
				variadicParam = StringToType(std::string(*function.variadicParam));
			}

			m_modules.memberTypes[std::string(module.name)][std::string(function.name)]
				= TypeInfo::Function(
					std::move(params),
					StringToType(std::string(function.result)),
					static_cast<size_t>(-1),
					{},
					{},
					false,
					function.variadic,
					std::move(variadicParam));
			m_modules.exports[std::string(module.name)].insert(std::string(function.name));
		}
	}
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::VisitAndGet(ASTNode* node)
{
	return AnalyzeExpr(node);
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::AnalyzeExpr(ASTNode* node)
{
	if (!node)
	{
		return TypeInfo::Unknown();
	}

	node->Accept(*this);
	return m_lastType;
}

bool SemanticAnalyzer::IsPrimitiveNumeric(const TypeInfo& t)
{
	if (t.kind == TypeKind::Int || t.kind == TypeKind::Float)
	{
		return true;
	}

	if (t.kind == TypeKind::TypeParameter)
	{
		for (const auto& constraint : t.constraints)
		{
			if (IsPrimitiveNumeric(constraint))
			{
				return true;
			}
		}
	}

	return false;
}

bool SemanticAnalyzer::IsTruthyBinaryOp(const std::string& op)
{
	return op == "and" || op == "or" || op == "&&" || op == "||";
}

void SemanticAnalyzer::EnsureDeclared(const std::string& name) const
{
	m_environment.EnsureDeclared(name);
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::Resolve(const std::string& name) const
{
	return m_environment.Resolve(name);
}

bool SemanticAnalyzer::ResolveIsConst(const std::string& name) const
{
	return m_environment.ResolveIsConst(name);
}

void SemanticAnalyzer::Define(const std::string& name, TypeInfo type, const bool isConst)
{
	m_environment.Define(name, std::move(type), isConst);
}

void SemanticAnalyzer::AnalyzeTypeForBinaryOp(const std::string& op, const TypeInfo& lhs, const TypeInfo& rhs)
{
	auto resolveNumericType = [&](const TypeInfo& type) -> TypeInfo {
		if (type.kind == TypeKind::TypeParameter)
		{
			for (const auto& constraint : type.constraints)
			{
				if (constraint.kind == TypeKind::Float)
				{
					return TypeInfo::Float();
				}
				if (constraint.kind == TypeKind::Int)
				{
					return TypeInfo::Int();
				}
			}
		}
		return type;
	};

	const TypeInfo effectiveLhs = resolveNumericType(lhs);
	const TypeInfo effectiveRhs = resolveNumericType(rhs);

	if (IsTruthyBinaryOp(op))
	{
		if (lhs.kind != TypeKind::Unknown && lhs.kind != TypeKind::Bool && lhs.kind != TypeKind::Never)
		{
			throw std::runtime_error("Logical operations require boolean operands");
		}
		if (rhs.kind != TypeKind::Unknown && rhs.kind != TypeKind::Bool && rhs.kind != TypeKind::Never)
		{
			throw std::runtime_error("Logical operations require boolean operands");
		}
		m_lastType = TypeInfo::Bool();
		return;
	}

	if (op != "+"
		&& op != "-"
		&& op != "*"
		&& op != "/"
		&& op != "div"
		&& op != "mod"
		&& op != "=="
		&& op != "!="
		&& op != "<"
		&& op != ">"
		&& op != "<="
		&& op != ">=")
	{
		throw std::runtime_error("Unsupported binary operator for codegen: " + op);
	}

	if (op == "+" || op == "-" || op == "*" || op == "/" || op == "div" || op == "mod")
	{
		if (op == "+" && (effectiveLhs.kind == TypeKind::String || effectiveRhs.kind == TypeKind::String))
		{
			m_lastType = TypeInfo::String();
			return;
		}

		if (!IsPrimitiveNumeric(effectiveLhs) || !IsPrimitiveNumeric(effectiveRhs))
		{
			throw std::runtime_error("Arithmetic requires numeric types");
		}
		if (effectiveLhs.kind == TypeKind::Float || effectiveRhs.kind == TypeKind::Float)
		{
			if (op == "div" || op == "mod")
			{
				throw std::runtime_error("Integer arithmetic requires integer operands");
			}
			m_lastType = TypeInfo::Float();
		}
		else
		{
			m_lastType = TypeInfo::Int();
		}
		return;
	}
	m_lastType = TypeInfo::Bool();
}

bool SemanticAnalyzer::IsFunctionLikeInterface(const TypeInfo& interfaceType)
{
	return interfaceType.kind == TypeKind::Interface
		&& interfaceType.methods
		&& interfaceType.methods->size() == 1;
}

std::optional<SemanticAnalyzer::TypeInfo> SemanticAnalyzer::ResolveModuleMemberType(
	const std::string& moduleName,
	const std::string& member) const
{
	if (!IsModuleMemberExported(moduleName, member))
	{
		return std::nullopt;
	}

	if (const auto moduleIt = m_modules.memberTypes.find(moduleName); moduleIt != m_modules.memberTypes.end())
	{
		if (const auto memberIt = moduleIt->second.find(member); memberIt != moduleIt->second.end())
		{
			return memberIt->second;
		}
	}
	if (const auto it = m_types.actors.find(moduleName + "." + member); it != m_types.actors.end())
	{
		return it->second;
	}
	if (const auto it = m_types.effects.find(moduleName + "." + member); it != m_types.effects.end())
	{
		return it->second;
	}
	return std::nullopt;
}

bool SemanticAnalyzer::IsModuleMemberExported(const std::string& moduleName, const std::string& member) const
{
	if (const auto it = m_modules.exports.find(moduleName); it != m_modules.exports.end())
	{
		return it->second.contains(member);
	}
	return true;
}

bool SemanticAnalyzer::ModuleDefinesMember(const std::string& moduleName, const std::string& member) const
{
	if (const auto moduleIt = m_modules.memberTypes.find(moduleName); moduleIt != m_modules.memberTypes.end())
	{
		if (moduleIt->second.contains(member))
		{
			return true;
		}
	}
	if (m_types.structs.contains(moduleName + "." + member))
	{
		return true;
	}
	if (m_types.enumConstructors.contains(moduleName + "." + member))
	{
		return true;
	}
	if (m_types.enums.contains(moduleName + "." + member))
	{
		return true;
	}
	if (m_types.interfaces.contains(moduleName + "." + member))
	{
		return true;
	}
	if (m_types.actors.contains(moduleName + "." + member))
	{
		return true;
	}
	if (m_types.effects.contains(moduleName + "." + member))
	{
		return true;
	}
	return false;
}

bool SemanticAnalyzer::CurrentContextAllowsEffect(const std::string& effectName) const
{
	for (const auto& scope : std::ranges::reverse_view(m_activeHandledEffects))
	{
		if (scope.contains(effectName))
		{
			return true;
		}
	}
	for (const auto& scope : std::ranges::reverse_view(m_activeRaisedEffects))
	{
		if (scope.contains(effectName))
		{
			return true;
		}
	}
	return false;
}

std::vector<std::string> SemanticAnalyzer::ResolveAccessibleEffectsForOperation(const std::string& operationName) const
{
	std::vector<std::string> candidates;
	auto collectMatches = [&](const auto& scopeStack) {
		for (const auto& scope : scopeStack)
		{
			for (const auto& effectName : scope)
			{
				if (const auto effectIt = m_types.effects.find(effectName);
					effectIt != m_types.effects.end() && effectIt->second.methods
					&& effectIt->second.methods->contains(operationName))
				{
					candidates.push_back(effectName);
				}
			}
		}
	};

	collectMatches(m_activeRaisedEffects);
	collectMatches(m_activeHandledEffects);

	std::ranges::sort(candidates);
	candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
	return candidates;
}

bool SemanticAnalyzer::IsUnsafeMemoryCall(const CallNode& node)
{
	const auto* member = dynamic_cast<const MemberAccessNode*>(node.callee.get());
	if (!member || (member->member != "alloc" && member->member != "free"))
	{
		return false;
	}

	const TypeInfo objectType = VisitAndGet(member->object.get());
	return objectType.kind == TypeKind::Module && objectType.name == "std.memory";
}

bool SemanticAnalyzer::HasActiveTransaction(const std::string& regionName) const
{
	for (const auto& scope : std::ranges::reverse_view(m_activeTransactions))
	{
		if (scope.contains(regionName))
		{
			return true;
		}
	}
	return false;
}

bool SemanticAnalyzer::IsCurrentActorState(const std::string& name) const
{
	for (const auto& scope : std::ranges::reverse_view(m_actorStateScopes))
	{
		if (scope.contains(name))
		{
			return true;
		}
	}
	return false;
}

bool SemanticAnalyzer::IsSendable(const TypeInfo& type)
{
	switch (type.kind)
	{
	case TypeKind::Unknown:
	case TypeKind::Any:
	case TypeKind::Void:
	case TypeKind::Bool:
	case TypeKind::Int:
	case TypeKind::Float:
	case TypeKind::String:
	case TypeKind::Task:
	case TypeKind::Channel:
		return true;
	case TypeKind::Pointer:
	case TypeKind::Ref:
		return false;
	case TypeKind::Array:
		return !type.element || IsSendable(*type.element);
	case TypeKind::Map:
		return (!type.key || IsSendable(*type.key))
			&& (!type.element || IsSendable(*type.element));
	case TypeKind::Struct:
	case TypeKind::Actor:
		if (!type.fields)
		{
			return true;
		}
		for (const auto& [_, fieldType] : *type.fields)
		{
			if (!IsSendable(fieldType))
			{
				return false;
			}
		}
		return true;
	default:
		return false;
	}
}

void SemanticAnalyzer::ValidateSendable(const TypeInfo& type, const std::string& what) const
{
	if (!IsSendable(type))
	{
		throw std::runtime_error(what + " must be sendable");
	}
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::InferFunctionBodyReturnType(ASTNode* body)
{
	auto* block = dynamic_cast<BlockNode*>(body);
	if (!block)
	{
		return TypeInfo::Unknown();
	}

	TypeInfo inferred = TypeInfo::Unknown();
	for (const auto& stmt : block->statements)
	{
		const auto* returnNode = dynamic_cast<ReturnNode*>(stmt.get());
		if (!returnNode || !returnNode->value)
		{
			continue;
		}

		TypeInfo current = VisitAndGet(returnNode->value.get());
		if (inferred.kind == TypeKind::Unknown)
		{
			inferred = current;
		}
		else if (!inferred.Equals(current))
		{
			return TypeInfo::Unknown();
		}
	}

	return inferred;
}

bool SemanticAnalyzer::IsAssignable(const TypeInfo& expected, const TypeInfo& actual) const
{
	if (expected.kind == TypeKind::Unknown || actual.kind == TypeKind::Unknown)
	{
		return true;
	}
	if (expected.kind == TypeKind::Any || actual.kind == TypeKind::Any)
	{
		return true;
	}
	if (actual.kind == TypeKind::Never)
	{
		return true;
	}

	if (expected.Equals(actual))
	{
		return true;
	}

	if ((expected.kind == TypeKind::Struct
			|| expected.kind == TypeKind::Enum
			|| expected.kind == TypeKind::Interface
			|| expected.kind == TypeKind::Actor)
		&& expected.kind == actual.kind)
	{
		std::string expectedBase;
		std::string actualBase;
		std::vector<std::string> expectedArgs;
		std::vector<std::string> actualArgs;
		if (SplitGenericName(expected.name, expectedBase, expectedArgs)
			&& SplitGenericName(actual.name, actualBase, actualArgs)
			&& expectedArgs.size() == actualArgs.size())
		{
			const TypeInfo expectedBaseType = StringToType(expectedBase);
			const TypeInfo actualBaseType = StringToType(actualBase);
			if (expectedBaseType.name == actualBaseType.name)
			{
				for (size_t i = 0; i < expectedArgs.size(); ++i)
				{
					if (!IsAssignable(StringToType(expectedArgs[i]), StringToType(actualArgs[i])))
					{
						return false;
					}
				}
				return true;
			}
		}
	}

	if ((expected.kind == TypeKind::Array || expected.kind == TypeKind::Channel || expected.kind == TypeKind::Pointer
			|| expected.kind == TypeKind::Ref || expected.kind == TypeKind::Task)
		&& expected.kind == actual.kind)
	{
		if (!expected.element || !actual.element)
		{
			return true;
		}
		return IsAssignable(*expected.element, *actual.element);
	}

	if (expected.kind == TypeKind::Map && actual.kind == TypeKind::Map)
	{
		if ((!expected.key || !actual.key) || (!expected.element || !actual.element))
		{
			return true;
		}
		return IsAssignable(*expected.key, *actual.key)
			&& IsAssignable(*expected.element, *actual.element);
	}

	if (expected.kind == TypeKind::Float && actual.kind == TypeKind::Int)
	{
		return true;
	}

	if (expected.kind == TypeKind::TypeParameter)
	{
		return SatisfiesConstraints(actual, expected.constraints);
	}

	if (expected.kind == TypeKind::Function && actual.kind == TypeKind::Function)
	{
		if (!expected.ret || !actual.ret)
		{
			return expected.ret == actual.ret;
		}
		if (expected.minArity != actual.minArity
			|| expected.variadic != actual.variadic
			|| expected.isComptime != actual.isComptime
			|| expected.params.size() != actual.params.size())
		{
			return false;
		}
		if (expected.variadic)
		{
			if (!expected.variadicParam || !actual.variadicParam)
			{
				if (expected.variadicParam != actual.variadicParam)
				{
					return false;
				}
			}
			else if (!IsAssignable(*expected.variadicParam, *actual.variadicParam))
			{
				return false;
			}
		}
		if ((!expected.contextRequirements || !actual.contextRequirements)
			? (expected.contextRequirements != actual.contextRequirements)
			: (expected.contextRequirements->size() != actual.contextRequirements->size()))
		{
			return false;
		}
		if (expected.contextRequirements && actual.contextRequirements)
		{
			for (size_t i = 0; i < expected.contextRequirements->size(); ++i)
			{
				const auto& expectedContext = (*expected.contextRequirements)[i];
				const auto& actualContext = (*actual.contextRequirements)[i];
				if (expectedContext.first != actualContext.first
					|| !IsAssignable(expectedContext.second, actualContext.second))
				{
					return false;
				}
			}
		}
		if (expected.raisedEffects && actual.raisedEffects)
		{
			for (const auto& actualEffect : *actual.raisedEffects)
			{
				if (!expected.raisedEffects->contains(actualEffect))
				{
					return false;
				}
			}
		}
		else if ((expected.raisedEffects && !expected.raisedEffects->empty())
			|| (actual.raisedEffects && !actual.raisedEffects->empty()))
		{
			return false;
		}
		for (size_t i = 0; i < expected.params.size(); ++i)
		{
			if (!IsAssignable(actual.params[i], expected.params[i]))
			{
				return false;
			}
		}
		return IsAssignable(*expected.ret, *actual.ret);
	}

	if (expected.kind == TypeKind::Interface)
	{
		if (actual.kind == TypeKind::Struct && expected.methods && actual.methods)
		{
			bool explicitlyImplemented = false;
			if (actual.implementedInterfaces)
			{
				explicitlyImplemented = actual.implementedInterfaces->contains(expected.name);
				if (!explicitlyImplemented)
				{
					std::string expectedBase;
					std::vector<std::string> expectedArgs;
					if (SplitGenericName(expected.name, expectedBase, expectedArgs))
					{
						for (const auto& implementedName : *actual.implementedInterfaces)
						{
							std::string implementedBase;
							std::vector<std::string> implementedArgs;
							if (SplitGenericName(implementedName, implementedBase, implementedArgs)
								&& implementedBase == expectedBase)
							{
								explicitlyImplemented = true;
								break;
							}
						}
					}
				}
			}
			if (!explicitlyImplemented)
			{
				return false;
			}
			for (const auto& [methodName, methodType] : *expected.methods)
			{
				const auto actualMethodIt = actual.methods->find(methodName);
				if (actualMethodIt == actual.methods->end() || !IsAssignable(methodType, actualMethodIt->second))
				{
					return false;
				}
			}
			return true;
		}
	}

	return false;
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::AddressableValueType(ASTNode* node)
{
	if (!node)
	{
		return TypeInfo::Unknown();
	}

	if (auto* identifier = dynamic_cast<IdentifierNode*>(node))
	{
		return Resolve(identifier->name);
	}

	if (auto* memberAccess = dynamic_cast<MemberAccessNode*>(node))
	{
		return VisitAndGet(memberAccess);
	}

	if (auto* index = dynamic_cast<IndexNode*>(node))
	{
		return VisitAndGet(index);
	}

	throw std::runtime_error("ref argument must be an assignable expression");
}

bool SemanticAnalyzer::IsConstAddressable(ASTNode* node) const
{
	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node))
	{
		return ResolveIsConst(identifier->name);
	}
	return false;
}

void SemanticAnalyzer::ValidateRefArgument(const TypeInfo& expectedRef, ASTNode* arg, const size_t argIndex)
{
	if (expectedRef.kind != TypeKind::Ref)
	{
		throw std::runtime_error("Internal semantic analyzer error: expected ref type");
	}

	if (IsConstAddressable(arg))
	{
		throw std::runtime_error("Cannot pass const value as ref argument");
	}

	const TypeInfo actualType = AddressableValueType(arg);
	const TypeInfo expectedType = expectedRef.element ? *expectedRef.element : TypeInfo::Unknown();
	if (!IsAssignable(expectedType, actualType))
	{
		throw std::runtime_error(
			"Argument "
			+ std::to_string(argIndex + 1)
			+ " mismatch: expected ref-compatible value");
	}
}

void SemanticAnalyzer::AnalyzeAssignment(ASTNode* valueExpr, const std::string& name, ASTNode* indexExpr, bool hasIndex)
{
	bool declared = true;
	try
	{
		EnsureDeclared(name);
	}
	catch (const std::runtime_error&)
	{
		declared = false;
	}

	auto valueType = VisitAndGet(valueExpr);

	if (!declared)
	{
		if (!m_environment.structMethodSelfStack.empty())
		{
			const auto& selfType = m_environment.structMethodSelfStack.back();
			if (selfType.fields && selfType.fields->contains(name) && !hasIndex)
			{
				const auto expectedType = selfType.fields->at(name);
				if (!IsAssignable(expectedType, valueType))
				{
					throw std::runtime_error("Struct field assignment type mismatch for '" + name + "'");
				}
				m_lastType = expectedType;
				return;
			}
		}
		throw std::runtime_error(FormatUndefined(name));
	}

	const auto declaredType = Resolve(name);

	if (!hasIndex)
	{
		if (m_sharedVariables.contains(name) && !HasActiveTransaction(name))
		{
			throw std::runtime_error("Shared variable mutation requires transaction(shared " + name + ")");
		}
		if (m_actorQueryDepth > 0 && IsCurrentActorState(name))
		{
			throw std::runtime_error("Query methods cannot mutate actor state");
		}
		if (ResolveIsConst(name))
		{
			throw std::runtime_error("Cannot assign to const variable: " + name);
		}
		if (!IsAssignable(declaredType, valueType))
		{
			throw std::runtime_error("Type mismatch in assignment to '" + name + "'");
		}
		m_lastType = declaredType.kind == TypeKind::Unknown ? valueType : declaredType;
		return;
	}

	const auto containerType = declaredType;
	const auto indexType = VisitAndGet(indexExpr);

	if (containerType.kind != TypeKind::Array && containerType.kind != TypeKind::Map && containerType.kind != TypeKind::Unknown)
	{
		throw std::runtime_error("Indexing requires array or map for assignment: " + name);
	}

	if (containerType.kind == TypeKind::Map)
	{
		if (containerType.key && indexType.kind != TypeKind::Unknown && !IsAssignable(*containerType.key, indexType))
		{
			throw std::runtime_error("Map key type mismatch for assignment: " + name);
		}
		if (containerType.element
			&& containerType.element->kind != TypeKind::Unknown
			&& valueType.kind != TypeKind::Unknown
			&& !IsAssignable(*containerType.element, valueType))
		{
			throw std::runtime_error("Map value type mismatch for assignment: " + name);
		}
		m_lastType = valueType;
		return;
	}

	if (indexType.kind != TypeKind::Unknown && !IsPrimitiveNumeric(indexType))
	{
		throw std::runtime_error("Index must be a numeric type for assignment: " + name);
	}

	if (containerType.kind == TypeKind::Array
		&& containerType.element
		&& valueType.kind != TypeKind::Unknown)
	{
		*containerType.element = std::move(valueType);
		Define(name, containerType, ResolveIsConst(name));
	}

	m_lastType = valueType;
}
