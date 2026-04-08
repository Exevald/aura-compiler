#include "SemanticAnalyzer.h"
#include "SyncStaticAnalyzer.h"

#include <cctype>
#include <functional>
#include <ranges>
#include <stdexcept>

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

std::string FormatUndefined(const std::string& name)
{
	return "Undefined variable: " + name;
}

std::string Trim(std::string value)
{
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
	{
		value.erase(value.begin());
	}
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
	{
		value.pop_back();
	}
	return value;
}

std::vector<std::string> SplitTopLevel(const std::string& text, const char delimiter)
{
	std::vector<std::string> parts;
	int depthAngles = 0;
	int depthBrackets = 0;
	int depthParens = 0;
	size_t start = 0;

	for (size_t i = 0; i < text.size(); ++i)
	{
		switch (text[i])
		{
		case '<':
			++depthAngles;
			break;
		case '>':
			--depthAngles;
			break;
		case '[':
			++depthBrackets;
			break;
		case ']':
			--depthBrackets;
			break;
		case '(':
			++depthParens;
			break;
		case ')':
			--depthParens;
			break;
		default:
			break;
		}

		if (text[i] == delimiter && depthAngles == 0 && depthBrackets == 0 && depthParens == 0)
		{
			parts.push_back(Trim(text.substr(start, i - start)));
			start = i + 1;
		}
	}

	parts.push_back(Trim(text.substr(start)));
	return parts;
}

bool SplitGenericName(const std::string& text, std::string& baseName, std::vector<std::string>& args)
{
	const size_t anglePos = text.find('<');
	if (anglePos == std::string::npos || text.back() != '>')
	{
		return false;
	}

	int depth = 0;
	for (size_t i = anglePos; i < text.size(); ++i)
	{
		if (text[i] == '<')
		{
			++depth;
		}
		else if (text[i] == '>')
		{
			--depth;
			if (depth == 0 && i != text.size() - 1)
			{
				return false;
			}
		}
	}

	baseName = Trim(text.substr(0, anglePos));
	args = SplitTopLevel(text.substr(anglePos + 1, text.size() - anglePos - 2), ',');
	return true;
}

std::string SubstituteTypeString(
	const std::string& text,
	const std::unordered_map<std::string, std::string>& replacements)
{
	for (const auto& [name, replacement] : replacements)
	{
		if (text == name)
		{
			return replacement;
		}
	}

	if ((text.rfind("ptr<", 0) == 0 || text.rfind("ref<", 0) == 0) && text.back() == '>')
	{
		return text.substr(0, 4) + SubstituteTypeString(text.substr(4, text.size() - 5), replacements) + ">";
	}

	if (text.size() > 2 && text.front() == '[' && text.back() == ']')
	{
		return "[" + SubstituteTypeString(text.substr(1, text.size() - 2), replacements) + "]";
	}

	if (const size_t arrowPos = text.find("->"); arrowPos != std::string::npos)
	{
		return SubstituteTypeString(text.substr(0, arrowPos), replacements)
			+ "->"
			+ SubstituteTypeString(text.substr(arrowPos + 2), replacements);
	}

	std::string baseName;
	std::vector<std::string> args;
	if (SplitGenericName(text, baseName, args))
	{
		std::string result = baseName + "<";
		for (size_t i = 0; i < args.size(); ++i)
		{
			if (i > 0)
			{
				result += ",";
			}
			result += SubstituteTypeString(args[i], replacements);
		}
		result += ">";
		return result;
	}

	return text;
}
} // namespace

void SemanticAnalyzer::EnvironmentState::Reset()
{
	scopes.clear();
	scopes.emplace_back();
	structMethodSelfStack.clear();
}

void SemanticAnalyzer::EnvironmentState::PushScope()
{
	scopes.emplace_back();
}

void SemanticAnalyzer::EnvironmentState::PopScope()
{
	if (scopes.empty())
	{
		throw std::runtime_error("Internal semantic analyzer error: empty environment");
	}
	scopes.pop_back();
}

void SemanticAnalyzer::EnvironmentState::PushStructMethodSelf(TypeInfo selfType)
{
	structMethodSelfStack.push_back(std::move(selfType));
}

void SemanticAnalyzer::EnvironmentState::PopStructMethodSelf()
{
	if (structMethodSelfStack.empty())
	{
		throw std::runtime_error("Internal semantic analyzer error: empty struct self stack");
	}
	structMethodSelfStack.pop_back();
}

void SemanticAnalyzer::EnvironmentState::Define(const std::string& name, TypeInfo type, const bool isConst)
{
	if (scopes.empty())
	{
		throw std::runtime_error("Internal semantic analyzer error: no environment scope");
	}

	scopes.back()[name] = { std::move(type), isConst };
}

void SemanticAnalyzer::EnvironmentState::EnsureDeclared(const std::string& name) const
{
	if (scopes.empty())
	{
		throw std::runtime_error("Internal semantic analyzer error: empty environment");
	}

	for (const auto& scope : std::ranges::reverse_view(scopes))
	{
		if (scope.contains(name))
		{
			return;
		}
	}

	throw std::runtime_error(FormatUndefined(name));
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::EnvironmentState::Resolve(const std::string& name) const
{
	for (const auto& scope : std::ranges::reverse_view(scopes))
	{
		if (scope.contains(name))
		{
			return scope.at(name).type;
		}
	}

	if (!structMethodSelfStack.empty())
	{
		const auto& selfType = structMethodSelfStack.back();
		if (selfType.fields && selfType.fields->contains(name))
		{
			return selfType.fields->at(name);
		}
	}

	throw std::runtime_error(FormatUndefined(name));
}

bool SemanticAnalyzer::EnvironmentState::ResolveIsConst(const std::string& name) const
{
	for (const auto& scope : std::ranges::reverse_view(scopes))
	{
		if (scope.contains(name))
		{
			return scope.at(name).isConst;
		}
	}

	throw std::runtime_error(FormatUndefined(name));
}

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
	registerBuiltinFunction(coreModuleName, "clamp", { TypeInfo::Unknown(), TypeInfo::Unknown(), TypeInfo::Unknown() }, TypeInfo::Unknown());

	const std::string ioModuleName = "std.io";
	registerBuiltinFunction(ioModuleName, "print", {}, TypeInfo::Void(), true, TypeInfo::Unknown());
	registerBuiltinFunction(ioModuleName, "println", {}, TypeInfo::Void(), true, TypeInfo::Unknown());
	registerBuiltinFunction(ioModuleName, "printf", { TypeInfo::String() }, TypeInfo::Void(), true, TypeInfo::Unknown());

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
	return false;
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

	if ((expected.kind == TypeKind::Array || expected.kind == TypeKind::Pointer)
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
	}

	return false;
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

void SemanticAnalyzer::CallAnalyzer::ValidateTypeCompatibility(
	const TypeInfo& expected,
	const TypeInfo& actual,
	const std::string& error) const
{
	if (actual.kind == TypeKind::Unknown || analyzer.IsAssignable(expected, actual))
	{
		return;
	}

	throw std::runtime_error(error);
}

void SemanticAnalyzer::CallAnalyzer::AnalyzeStructConstructorCall(
	const TypeInfo& funcType,
	const std::vector<ASTNodePtr>& args) const
{
	if (!funcType.fields)
	{
		throw std::runtime_error("Struct metadata is missing");
	}

	if (args.size() != funcType.fields->size())
	{
		throw std::runtime_error(
			"Struct constructor expects "
			+ std::to_string(funcType.fields->size())
			+ " arguments, but got " + std::to_string(args.size()));
	}

	size_t index = 0;
	for (const auto& [fieldName, fieldType] : *funcType.fields)
	{
		const TypeInfo argType = analyzer.VisitAndGet(args[index].get());
		ValidateTypeCompatibility(fieldType, argType, "Struct field argument mismatch for '" + fieldName + "'");
		++index;
	}

	analyzer.m_lastType = funcType;
}

void SemanticAnalyzer::CallAnalyzer::AnalyzeEnumConstructorCall(
	const TypeInfo& funcType,
	const std::vector<ASTNodePtr>& args) const
{
	if (args.size() != funcType.variantArgs.size())
	{
		throw std::runtime_error(
			"Enum constructor expects "
			+ std::to_string(funcType.variantArgs.size())
			+ " arguments, but got " + std::to_string(args.size()));
	}

	for (size_t i = 0; i < args.size(); ++i)
	{
		const TypeInfo argType = analyzer.VisitAndGet(args[i].get());
		ValidateTypeCompatibility(funcType.variantArgs[i], argType, "Enum constructor argument mismatch");
	}

	analyzer.m_lastType = TypeInfo::Enum(funcType.name);
}

void SemanticAnalyzer::CallAnalyzer::AnalyzeFunctionCall(
	const TypeInfo& funcType,
	const std::vector<ASTNodePtr>& args) const
{
	if ((!funcType.variadic && args.size() != funcType.params.size())
		|| (funcType.variadic && args.size() < funcType.params.size()))
	{
		throw std::runtime_error(
			"Function call expects "
			+ std::string(funcType.variadic ? "at least " : "")
			+ std::to_string(funcType.params.size())
			+ " arguments, but got " + std::to_string(args.size()));
	}

	std::unordered_map<std::string, TypeInfo> genericBindings;
	for (size_t i = 0; i < args.size(); ++i)
	{
		const TypeInfo argType = analyzer.VisitAndGet(args[i].get());
		TypeInfo expectedType = TypeInfo::Unknown();
		if (i < funcType.params.size())
		{
			expectedType = funcType.params[i];
		}
		else if (funcType.variadic && funcType.variadicParam)
		{
			expectedType = *funcType.variadicParam;
		}
		if (!analyzer.BindGenericType(expectedType, argType, genericBindings) && argType.kind != TypeKind::Unknown)
		{
			throw std::runtime_error(
				"Argument "
				+ std::to_string(i + 1)
				+ " does not satisfy the required generic type/constraints");
		}

		expectedType = analyzer.SubstituteTypeParameters(expectedType, genericBindings);
		if (!analyzer.IsAssignable(expectedType, argType) && argType.kind != TypeKind::Unknown)
		{
			throw std::runtime_error(
				"Argument "
				+ std::to_string(i + 1)
				+ " mismatch: expected "
				+ std::to_string(static_cast<int>(expectedType.kind))
				+ ", got " + std::to_string(static_cast<int>(argType.kind)));
		}
	}

	analyzer.m_lastType = analyzer.SubstituteTypeParameters(*funcType.ret, genericBindings);
}

void SemanticAnalyzer::CallAnalyzer::Analyze(const CallNode& node) const
{
	const TypeInfo funcType = analyzer.VisitAndGet(node.callee.get());
	if (funcType.kind == TypeKind::Unknown)
	{
		for (auto& arg : node.args)
		{
			(void)analyzer.VisitAndGet(arg.get());
		}
		analyzer.m_lastType = TypeInfo::Unknown();
		return;
	}

	if (funcType.kind == TypeKind::Struct)
	{
		AnalyzeStructConstructorCall(funcType, node.args);
		return;
	}

	if (funcType.kind == TypeKind::EnumConstructor)
	{
		AnalyzeEnumConstructorCall(funcType, node.args);
		return;
	}

	if (funcType.kind != TypeKind::Function)
	{
		throw std::runtime_error("Target expression is not a function");
	}

	AnalyzeFunctionCall(funcType, node.args);
}

std::string SemanticAnalyzer::QualifyName(const std::string& name) const
{
	if (name.find('.') != std::string::npos)
	{
		return name;
	}
	if (m_currentModule.empty())
	{
		return name;
	}
	return m_currentModule + "." + name;
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::TypeResolver::ResolveTypeName(const std::string& name) const
{
	for (const auto& scope : std::ranges::reverse_view(analyzer.m_typeParamScopes))
	{
		if (const auto it = scope.find(name); it != scope.end())
		{
			return it->second;
		}
	}

	if (const auto aliasIt = analyzer.m_types.aliases.find(name); aliasIt != analyzer.m_types.aliases.end())
	{
		return ResolveTypeName(aliasIt->second);
	}

	std::string genericBase;
	std::vector<std::string> genericArgs;
	if (SplitGenericName(name, genericBase, genericArgs))
	{
		if (const auto genericAliasIt = analyzer.m_types.genericAliases.find(genericBase);
			genericAliasIt != analyzer.m_types.genericAliases.end())
		{
			if (genericAliasIt->second.typeParams.size() != genericArgs.size())
			{
				return TypeInfo::Unknown();
			}

			std::unordered_map<std::string, std::string> replacements;
			for (size_t i = 0; i < genericArgs.size(); ++i)
			{
				replacements[genericAliasIt->second.typeParams[i].name] = genericArgs[i];
			}
			return ResolveTypeName(SubstituteTypeString(genericAliasIt->second.aliasedType, replacements));
		}
	}

	if (name == "int")
	{
		return TypeInfo::Int();
	}
	if (name == "float")
	{
		return TypeInfo::Float();
	}
	if (name == "string")
	{
		return TypeInfo::String();
	}
	if (name == "bool")
	{
		return TypeInfo::Bool();
	}
	if (name == "void")
	{
		return TypeInfo::Void();
	}

	if ((name.size() > 5 && name.rfind("ptr<", 0) == 0 && name.back() == '>')
		|| (name.size() > 5 && name.rfind("ref<", 0) == 0 && name.back() == '>'))
	{
		const std::string inner = name.substr(4, name.size() - 5);
		return TypeInfo::PointerTo(ResolveTypeName(inner));
	}

	if (name.size() > 2 && name.front() == '[' && name.back() == ']')
	{
		const std::string inner = name.substr(1, name.size() - 2);
		return TypeInfo::ArrayOf(ResolveTypeName(inner));
	}

	for (size_t i = 0; i + 1 < name.size(); ++i)
	{
		if (name[i] == '-' && name[i + 1] == '>')
		{
			const std::string left = name.substr(0, i);
			const std::string right = name.substr(i + 2);
			return TypeInfo::Function({ ResolveTypeName(left) }, ResolveTypeName(right));
		}
	}

	if (const auto it = analyzer.m_types.structs.find(name); it != analyzer.m_types.structs.end())
	{
		return it->second;
	}
	if (const auto it = analyzer.m_types.enums.find(name); it != analyzer.m_types.enums.end())
	{
		return it->second;
	}
	if (const auto it = analyzer.m_types.interfaces.find(name); it != analyzer.m_types.interfaces.end())
	{
		return it->second;
	}

	const std::string qualifiedName = analyzer.QualifyName(name);
	if (auto it = analyzer.m_types.structs.find(qualifiedName); it != analyzer.m_types.structs.end())
	{
		return it->second;
	}
	if (auto it = analyzer.m_types.enums.find(qualifiedName); it != analyzer.m_types.enums.end())
	{
		return it->second;
	}
	if (auto it = analyzer.m_types.interfaces.find(qualifiedName); it != analyzer.m_types.interfaces.end())
	{
		return it->second;
	}

	return TypeInfo::Unknown();
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::StringToType(const std::string& name) const
{
	return TypeResolver{ *this }.ResolveTypeName(name);
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::TypeResolver::Substitute(
	const TypeInfo& type,
	const std::unordered_map<std::string, TypeInfo>& bindings)
{
	if (type.kind == TypeKind::TypeParameter)
	{
		if (const auto it = bindings.find(type.name); it != bindings.end())
		{
			return it->second;
		}
		return type;
	}

	if (type.kind == TypeKind::Array && type.element)
	{
		return TypeInfo::ArrayOf(Substitute(*type.element, bindings));
	}

	if (type.kind == TypeKind::Pointer && type.element)
	{
		return TypeInfo::PointerTo(Substitute(*type.element, bindings));
	}

	if (type.kind == TypeKind::Function && type.ret)
	{
		std::vector<TypeInfo> params;
		params.reserve(type.params.size());
		for (const auto& param : type.params)
		{
			params.push_back(Substitute(param, bindings));
		}
		std::optional<TypeInfo> variadicParam = std::nullopt;
		if (type.variadicParam)
		{
			variadicParam = Substitute(*type.variadicParam, bindings);
		}
		return TypeInfo::Function(params, Substitute(*type.ret, bindings), type.variadic, std::move(variadicParam));
	}

	return type;
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::SubstituteTypeParameters(
	const TypeInfo& type,
	const std::unordered_map<std::string, TypeInfo>& bindings) const
{
	return TypeResolver{ *this }.Substitute(type, bindings);
}

bool SemanticAnalyzer::TypeResolver::Satisfies(const TypeInfo& actual, const std::vector<TypeInfo>& constraints) const
{
	for (const auto& constraint : constraints)
	{
		if (!analyzer.IsAssignable(constraint, actual))
		{
			return false;
		}
	}
	return true;
}

bool SemanticAnalyzer::SatisfiesConstraints(const TypeInfo& actual, const std::vector<TypeInfo>& constraints) const
{
	return TypeResolver{ *this }.Satisfies(actual, constraints);
}

bool SemanticAnalyzer::TypeResolver::Bind(
	const TypeInfo& expected,
	const TypeInfo& actual,
	std::unordered_map<std::string, TypeInfo>& bindings) const
{
	if (expected.kind == TypeKind::Unknown || actual.kind == TypeKind::Unknown)
	{
		return true;
	}

	if (expected.kind == TypeKind::TypeParameter)
	{
		if (!Satisfies(actual, expected.constraints))
		{
			return false;
		}

		if (const auto it = bindings.find(expected.name); it != bindings.end())
		{
			return it->second.Equals(actual);
		}

		bindings[expected.name] = actual;
		return true;
	}

	if (expected.kind == TypeKind::Array && actual.kind == TypeKind::Array && expected.element && actual.element)
	{
		return Bind(*expected.element, *actual.element, bindings);
	}

	if (expected.kind == TypeKind::Pointer && actual.kind == TypeKind::Pointer && expected.element && actual.element)
	{
		return Bind(*expected.element, *actual.element, bindings);
	}

	if (expected.kind == TypeKind::Function && actual.kind == TypeKind::Function && expected.ret && actual.ret)
	{
		if (expected.variadic != actual.variadic)
		{
			return false;
		}
		if (expected.params.size() != actual.params.size())
		{
			return false;
		}
		for (size_t i = 0; i < expected.params.size(); ++i)
		{
			if (!Bind(expected.params[i], actual.params[i], bindings))
			{
				return false;
			}
		}
		if (expected.variadic)
		{
			if (!expected.variadicParam || !actual.variadicParam)
			{
				return expected.variadicParam == actual.variadicParam
					&& Bind(*expected.ret, *actual.ret, bindings);
			}
			if (!Bind(*expected.variadicParam, *actual.variadicParam, bindings))
			{
				return false;
			}
		}
		return Bind(*expected.ret, *actual.ret, bindings);
	}

	return analyzer.IsAssignable(expected, actual);
}

bool SemanticAnalyzer::BindGenericType(
	const TypeInfo& expected,
	const TypeInfo& actual,
	std::unordered_map<std::string, TypeInfo>& bindings) const
{
	return TypeResolver{ *this }.Bind(expected, actual, bindings);
}

void SemanticAnalyzer::Visit(IntegerLiteralNode& node)
{
	(void)node;
	m_lastType = TypeInfo::Int();
}

void SemanticAnalyzer::Visit(FloatLiteralNode& node)
{
	(void)node;
	m_lastType = TypeInfo::Float();
}

void SemanticAnalyzer::Visit(StringLiteralNode& node)
{
	(void)node;
	m_lastType = TypeInfo::String();
}

void SemanticAnalyzer::Visit(IdentifierNode& node)
{
	m_lastType = Resolve(node.name);
}

void SemanticAnalyzer::Visit(UnaryExprNode& node)
{
	const TypeInfo operandType = VisitAndGet(node.operand.get());

	if (node.op == "+")
	{
		if (operandType.kind != TypeKind::Unknown && !IsPrimitiveNumeric(operandType))
		{
			throw std::runtime_error("Unary '+' requires a numeric operand");
		}
		m_lastType = operandType;
		return;
	}

	if (node.op == "-")
	{
		if (operandType.kind != TypeKind::Unknown && !IsPrimitiveNumeric(operandType))
		{
			throw std::runtime_error("Unary '-' requires a numeric operand");
		}
		m_lastType = operandType;
		return;
	}

	if (node.op == "!" || node.op == "not")
	{
		if (operandType.kind != TypeKind::Unknown && operandType.kind != TypeKind::Bool)
		{
			throw std::runtime_error("Logical negation requires a boolean operand");
		}
		m_lastType = TypeInfo::Bool();
		return;
	}

	if (node.op == "*")
	{
		if (m_unsafeDepth <= 0)
		{
			throw std::runtime_error("Pointer dereference requires an unsafe block");
		}
		if (operandType.kind != TypeKind::Pointer)
		{
			throw std::runtime_error("Dereference requires a pointer operand");
		}
		m_lastType = operandType.element ? *operandType.element : TypeInfo::Unknown();
		return;
	}

	if (node.op == "&")
	{
		if (m_unsafeDepth <= 0)
		{
			throw std::runtime_error("Address-of requires an unsafe block");
		}
		if (dynamic_cast<IdentifierNode*>(node.operand.get()))
		{
			const auto* identifier = dynamic_cast<IdentifierNode*>(node.operand.get());
			if (ResolveIsConst(identifier->name))
			{
				throw std::runtime_error("Cannot take address of const variable: " + identifier->name);
			}
		}

		m_lastType = TypeInfo::PointerTo(operandType);
		return;
	}

	throw std::runtime_error("Unsupported unary operator for codegen: " + node.op);
}

void SemanticAnalyzer::Visit(BinaryExprNode& node)
{
	const auto lhs = VisitAndGet(node.left.get());
	const auto rhs = VisitAndGet(node.right.get());
	AnalyzeTypeForBinaryOp(node.op, lhs, rhs);
}

void SemanticAnalyzer::Visit(AssignmentNode& node)
{
	if (node.object)
	{
		const TypeInfo objectType = VisitAndGet(node.object.get());
		if (objectType.kind != TypeKind::Struct || !objectType.fields)
		{
			throw std::runtime_error("Member assignment requires a struct instance");
		}
		if (!objectType.fields->contains(node.member))
		{
			throw std::runtime_error("Unknown struct field: " + node.member);
		}

		const TypeInfo expectedType = objectType.fields->at(node.member);
		const TypeInfo actualType = VisitAndGet(node.value.get());
		if (actualType.kind != TypeKind::Unknown && !expectedType.Equals(actualType))
		{
			if (!(expectedType.kind == TypeKind::Float && actualType.kind == TypeKind::Int))
			{
				throw std::runtime_error("Struct field assignment type mismatch for '" + node.member + "'");
			}
		}

		m_lastType = expectedType;
		return;
	}

	if (node.dereferenceTarget)
	{
		if (m_unsafeDepth <= 0)
		{
			throw std::runtime_error("Dereference assignment requires an unsafe block");
		}
		const TypeInfo pointerType = VisitAndGet(node.dereferenceTarget.get());
		if (pointerType.kind != TypeKind::Pointer || !pointerType.element)
		{
			throw std::runtime_error("Dereference assignment requires a pointer target");
		}

		const TypeInfo actualType = VisitAndGet(node.value.get());
		const TypeInfo expectedType = *pointerType.element;
		if (expectedType.kind != TypeKind::Unknown
			&& actualType.kind != TypeKind::Unknown
			&& !expectedType.Equals(actualType))
		{
			if (!(expectedType.kind == TypeKind::Float && actualType.kind == TypeKind::Int))
			{
				throw std::runtime_error("Dereference assignment type mismatch");
			}
		}

		m_lastType = expectedType;
		return;
	}

	AnalyzeAssignment(node.value.get(), node.name, node.index.get(), node.index != nullptr);
}

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

		const TypeInfo ctorType = TypeInfo::EnumConstructor(enumName, static_cast<int>(index), std::move(argTypes));
		const std::string qualifiedCtorName = QualifyName(node.variants[index].name);
		m_types.enumConstructors[qualifiedCtorName] = ctorType;
		Define(node.variants[index].name, ctorType);
	}

	m_lastType = TypeInfo::Void();
}

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
