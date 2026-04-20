#include "../SyncStaticAnalyzer.h"
#include "../common/SemanticAnalyzerSupport.h"
#include "SemanticAnalyzer.h"

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
	m_unsafeDepth = 0;
	RegisterBuiltinModules();

	if (root)
	{
		root->Accept(*this);
	}

	SyncStaticAnalyzer syncAnalyzer;
	syncAnalyzer.Analyze(root);
}

void SemanticAnalyzer::RegisterBuiltinModules()
{
	auto registerBuiltinFunction = [this](
									   const std::string& moduleName,
									   const std::string& functionName,
									   std::vector<TypeInfo> params,
									   TypeInfo resultType,
									   bool variadic = false,
									   std::optional<TypeInfo> variadicParam = std::nullopt) {
		m_modules.memberTypes[moduleName][functionName]
			= TypeInfo::Function(std::move(params), std::move(resultType), variadic, std::move(variadicParam));
		m_modules.exports[moduleName].insert(functionName);
	};

	const std::string moduleName = "std.memory";
	registerBuiltinFunction(moduleName, "active_allocations", {}, TypeInfo::Int());
	registerBuiltinFunction(moduleName, "active_bytes", {}, TypeInfo::Int());
	registerBuiltinFunction(moduleName, "total_allocations", {}, TypeInfo::Int());
	registerBuiltinFunction(moduleName, "total_bytes", {}, TypeInfo::Int());
	registerBuiltinFunction(moduleName, "deep_size", { TypeInfo::Unknown() }, TypeInfo::Int());
	registerBuiltinFunction(moduleName, "is_send", { TypeInfo::Unknown() }, TypeInfo::Bool());
	registerBuiltinFunction(moduleName, "is_sync", { TypeInfo::Unknown() }, TypeInfo::Bool());
	registerBuiltinFunction(moduleName, "assert_send", { TypeInfo::Unknown() }, TypeInfo::Bool());
	registerBuiltinFunction(moduleName, "assert_sync", { TypeInfo::Unknown() }, TypeInfo::Bool());
	registerBuiltinFunction(moduleName, "assert_no_leaks", {}, TypeInfo::Bool());
	registerBuiltinFunction(moduleName, "alloc", { TypeInfo::Int() }, TypeInfo::PointerTo(TypeInfo::Unknown()));
	registerBuiltinFunction(moduleName, "free", { TypeInfo::Unknown() }, TypeInfo::Void());

	const std::string coreModuleName = "std.core";
	registerBuiltinFunction(coreModuleName, "len", { TypeInfo::Unknown() }, TypeInfo::Int());
	registerBuiltinFunction(coreModuleName, "max", { TypeInfo::Unknown(), TypeInfo::Unknown() }, TypeInfo::Unknown());
	registerBuiltinFunction(coreModuleName, "min", { TypeInfo::Unknown(), TypeInfo::Unknown() }, TypeInfo::Unknown());
	registerBuiltinFunction(coreModuleName, "abs", { TypeInfo::Unknown() }, TypeInfo::Unknown());
	registerBuiltinFunction(coreModuleName, "sort", { TypeInfo::ArrayOf(TypeInfo::Unknown()) }, TypeInfo::ArrayOf(TypeInfo::Unknown()));
	registerBuiltinFunction(coreModuleName, "push", { TypeInfo::ArrayOf(TypeInfo::Unknown()), TypeInfo::Unknown() }, TypeInfo::ArrayOf(TypeInfo::Unknown()));
	registerBuiltinFunction(coreModuleName, "pop", { TypeInfo::ArrayOf(TypeInfo::Unknown()) }, TypeInfo::Unknown());
	registerBuiltinFunction(coreModuleName, "concat", { TypeInfo::String(), TypeInfo::String() }, TypeInfo::String());
	registerBuiltinFunction(coreModuleName, "contains", { TypeInfo::String(), TypeInfo::String() }, TypeInfo::Bool());
	registerBuiltinFunction(coreModuleName, "to_string", { TypeInfo::Unknown() }, TypeInfo::String());
	registerBuiltinFunction(coreModuleName, "to_int", { TypeInfo::Unknown() }, TypeInfo::Int());
	registerBuiltinFunction(coreModuleName, "to_float", { TypeInfo::Unknown() }, TypeInfo::Float());
	registerBuiltinFunction(coreModuleName, "to_bool", { TypeInfo::Unknown() }, TypeInfo::Bool());
	registerBuiltinFunction(coreModuleName, "clamp", { TypeInfo::Unknown(), TypeInfo::Unknown(), TypeInfo::Unknown() }, TypeInfo::Unknown());

	const std::string ioModuleName = "std.io";
	registerBuiltinFunction(ioModuleName, "print", {}, TypeInfo::Void(), true, TypeInfo::Unknown());
	registerBuiltinFunction(ioModuleName, "println", {}, TypeInfo::Void(), true, TypeInfo::Unknown());
	registerBuiltinFunction(ioModuleName, "printf", { TypeInfo::String() }, TypeInfo::Void(), true, TypeInfo::Unknown());
	registerBuiltinFunction(ioModuleName, "read", {}, TypeInfo::String());
	registerBuiltinFunction(ioModuleName, "readln", {}, TypeInfo::String());

	const std::string mathModuleName = "std.math";
	registerBuiltinFunction(mathModuleName, "max", { TypeInfo::Unknown(), TypeInfo::Unknown() }, TypeInfo::Unknown());
	registerBuiltinFunction(mathModuleName, "min", { TypeInfo::Unknown(), TypeInfo::Unknown() }, TypeInfo::Unknown());
	registerBuiltinFunction(mathModuleName, "abs", { TypeInfo::Unknown() }, TypeInfo::Unknown());
	registerBuiltinFunction(mathModuleName, "clamp", { TypeInfo::Unknown(), TypeInfo::Unknown(), TypeInfo::Unknown() }, TypeInfo::Unknown());

	const std::string arrayModuleName = "std.array";
	registerBuiltinFunction(arrayModuleName, "len", { TypeInfo::ArrayOf(TypeInfo::Unknown()) }, TypeInfo::Int());
	registerBuiltinFunction(arrayModuleName, "sort", { TypeInfo::ArrayOf(TypeInfo::Unknown()) }, TypeInfo::ArrayOf(TypeInfo::Unknown()));
	registerBuiltinFunction(arrayModuleName, "push", { TypeInfo::ArrayOf(TypeInfo::Unknown()), TypeInfo::Unknown() }, TypeInfo::ArrayOf(TypeInfo::Unknown()));
	registerBuiltinFunction(arrayModuleName, "pop", { TypeInfo::ArrayOf(TypeInfo::Unknown()) }, TypeInfo::Unknown());

	const std::string stringModuleName = "std.text";
	registerBuiltinFunction(stringModuleName, "len", { TypeInfo::String() }, TypeInfo::Int());
	registerBuiltinFunction(stringModuleName, "concat", { TypeInfo::String(), TypeInfo::String() }, TypeInfo::String());
	registerBuiltinFunction(stringModuleName, "contains", { TypeInfo::String(), TypeInfo::String() }, TypeInfo::Bool());
	registerBuiltinFunction(stringModuleName, "to_string", { TypeInfo::Unknown() }, TypeInfo::String());
	registerBuiltinFunction(stringModuleName, "to_int", { TypeInfo::Unknown() }, TypeInfo::Int());
	registerBuiltinFunction(stringModuleName, "to_float", { TypeInfo::Unknown() }, TypeInfo::Float());
	registerBuiltinFunction(stringModuleName, "to_bool", { TypeInfo::Unknown() }, TypeInfo::Bool());

	const std::string logModuleName = "std.log";
	registerBuiltinFunction(logModuleName, "Error", {}, TypeInfo::Void(), true, TypeInfo::Unknown());
	registerBuiltinFunction(logModuleName, "Warn", {}, TypeInfo::Void(), true, TypeInfo::Unknown());
	registerBuiltinFunction(logModuleName, "Info", {}, TypeInfo::Void(), true, TypeInfo::Unknown());
	registerBuiltinFunction(logModuleName, "Fatal", {}, TypeInfo::Void(), true, TypeInfo::Unknown());

	const std::string syncModuleName = "std.sync";
	registerBuiltinFunction(syncModuleName, "current_thread", {}, TypeInfo::Unknown());
	registerBuiltinFunction(syncModuleName, "spawn", {}, TypeInfo::Unknown());
	registerBuiltinFunction(syncModuleName, "mutex", {}, TypeInfo::Unknown());
	registerBuiltinFunction(syncModuleName, "lock", { TypeInfo::Unknown(), TypeInfo::Unknown() }, TypeInfo::Bool());
	registerBuiltinFunction(syncModuleName, "unlock", { TypeInfo::Unknown(), TypeInfo::Unknown() }, TypeInfo::Bool());
	registerBuiltinFunction(syncModuleName, "would_deadlock", { TypeInfo::Unknown(), TypeInfo::Unknown() }, TypeInfo::Bool());
	registerBuiltinFunction(syncModuleName, "assert_no_deadlock", {}, TypeInfo::Bool());
	registerBuiltinFunction(syncModuleName, "join", { TypeInfo::Unknown(), TypeInfo::Unknown() }, TypeInfo::Bool());
	registerBuiltinFunction(syncModuleName, "finish", { TypeInfo::Unknown() }, TypeInfo::Bool());
	registerBuiltinFunction(syncModuleName, "is_locked", { TypeInfo::Unknown() }, TypeInfo::Bool());
	registerBuiltinFunction(syncModuleName, "owner_id", { TypeInfo::Unknown() }, TypeInfo::Unknown());
	registerBuiltinFunction(syncModuleName, "thread_id", { TypeInfo::Unknown() }, TypeInfo::Int());
	registerBuiltinFunction(syncModuleName, "thread_count", {}, TypeInfo::Int());
	registerBuiltinFunction(syncModuleName, "mutex_count", {}, TypeInfo::Int());
	registerBuiltinFunction(syncModuleName, "wait_edge_count", {}, TypeInfo::Int());
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
		if (lhs.kind != TypeKind::Unknown && lhs.kind != TypeKind::Bool)
		{
			throw std::runtime_error("Logical operations require boolean operands");
		}
		if (rhs.kind != TypeKind::Unknown && rhs.kind != TypeKind::Bool)
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
	case TypeKind::Void:
	case TypeKind::Bool:
	case TypeKind::Int:
	case TypeKind::Float:
	case TypeKind::String:
		return true;
	case TypeKind::Pointer:
	case TypeKind::Ref:
		return false;
	case TypeKind::Array:
		return !type.element || IsSendable(*type.element);
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

	if (expected.Equals(actual))
	{
		return true;
	}

	if ((expected.kind == TypeKind::Array || expected.kind == TypeKind::Pointer || expected.kind == TypeKind::Ref)
		&& expected.kind == actual.kind)
	{
		if (!expected.element || !actual.element)
		{
			return true;
		}
		return IsAssignable(*expected.element, *actual.element);
	}

	if (expected.kind == TypeKind::Float && actual.kind == TypeKind::Int)
	{
		return true;
	}

	if (expected.kind == TypeKind::TypeParameter)
	{
		return SatisfiesConstraints(actual, expected.constraints);
	}

	if (expected.kind == TypeKind::Interface)
	{
		if (actual.kind == TypeKind::Module && expected.methods)
		{
			for (const auto& [methodName, methodType] : *expected.methods)
			{
				const auto actualMethod = ResolveModuleMemberType(actual.name, methodName);
				if (!actualMethod || !methodType.Equals(*actualMethod))
				{
					return false;
				}
			}
			return true;
		}

		if (actual.kind == TypeKind::Function && IsFunctionLikeInterface(expected) && expected.methods)
		{
			return expected.methods->begin()->second.Equals(actual);
		}

		if (actual.kind == TypeKind::Struct && expected.methods && actual.methods)
		{
			for (const auto& [methodName, methodType] : *expected.methods)
			{
				const auto actualMethodIt = actual.methods->find(methodName);
				if (actualMethodIt == actual.methods->end() || !methodType.Equals(actualMethodIt->second))
				{
					return false;
				}
			}
			return true;
		}

		if (actual.kind == TypeKind::Actor && expected.methods && actual.methods)
		{
			for (const auto& [methodName, methodType] : *expected.methods)
			{
				const auto actualMethodIt = actual.methods->find(methodName);
				if (actualMethodIt == actual.methods->end() || !methodType.Equals(actualMethodIt->second))
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

	if (containerType.kind != TypeKind::Array && containerType.kind != TypeKind::Unknown)
	{
		throw std::runtime_error("Indexing requires array for assignment: " + name);
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