#include "../common/SemanticAnalyzerSupport.h"
#include "SemanticAnalyzer.h"

#include <ranges>
#include <stdexcept>

using SemanticAnalyzerDetail::FormatUndefined;
using SemanticAnalyzerDetail::ScopeExit;
using SemanticAnalyzerDetail::SplitGenericName;
using SemanticAnalyzerDetail::SubstituteTypeString;

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
		ValidateTypeCompatibility(
			fieldType,
			argType,
			"Struct field argument mismatch for '" + fieldName + "'");
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
		ValidateTypeCompatibility(
			funcType.variantArgs[i],
			argType,
			"Enum constructor argument mismatch");
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
		TypeInfo expectedType = TypeInfo::Unknown();
		if (i < funcType.params.size())
		{
			expectedType = funcType.params[i];
		}
		else if (funcType.variadic && funcType.variadicParam)
		{
			expectedType = *funcType.variadicParam;
		}

		if (expectedType.kind == TypeKind::Ref)
		{
			analyzer.ValidateRefArgument(expectedType, args[i].get(), i);
			continue;
		}

		const TypeInfo argType = analyzer.VisitAndGet(args[i].get());
		if (!analyzer.BindGenericType(
				expectedType,
				argType,
				genericBindings)
			&& argType.kind != TypeKind::Unknown)
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
	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node.callee.get()))
	{
		if (const auto opIt = analyzer.m_types.effectOperations.find(identifier->name);
			opIt != analyzer.m_types.effectOperations.end())
		{
			const auto effectIt = analyzer.m_types.effects.find(opIt->second);
			if (effectIt == analyzer.m_types.effects.end() || !effectIt->second.methods)
			{
				throw std::runtime_error("Unknown effect operation: " + identifier->name);
			}
			const auto methodIt = effectIt->second.methods->find(identifier->name);
			if (methodIt == effectIt->second.methods->end())
			{
				throw std::runtime_error("Unknown effect operation: " + identifier->name);
			}
			if (!analyzer.CurrentContextAllowsEffect(effectIt->second.name))
			{
				throw std::runtime_error("Unhandled effect operation: " + identifier->name);
			}
			AnalyzeFunctionCall(methodIt->second, node.args);
			return;
		}
	}

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

	if (funcType.kind == TypeKind::Actor)
	{
		AnalyzeStructConstructorCall(funcType, node.args);
		analyzer.m_lastType = funcType;
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

	if (const auto aliasIt = analyzer.m_types.aliases.find(name);
		aliasIt != analyzer.m_types.aliases.end())
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
			return ResolveTypeName(SubstituteTypeString(
				genericAliasIt->second.aliasedType,
				replacements));
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

	if (name.size() > 5 && name.rfind("ptr<", 0) == 0 && name.back() == '>')
	{
		const std::string inner = name.substr(4, name.size() - 5);
		return TypeInfo::PointerTo(ResolveTypeName(inner));
	}

	if (name.size() > 5 && name.rfind("ref<", 0) == 0 && name.back() == '>')
	{
		const std::string inner = name.substr(4, name.size() - 5);
		return TypeInfo::RefTo(ResolveTypeName(inner));
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
	if (const auto it = analyzer.m_types.actors.find(name); it != analyzer.m_types.actors.end())
	{
		return it->second;
	}
	if (const auto it = analyzer.m_types.effects.find(name); it != analyzer.m_types.effects.end())
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
	if (auto it = analyzer.m_types.actors.find(qualifiedName); it != analyzer.m_types.actors.end())
	{
		return it->second;
	}
	if (auto it = analyzer.m_types.effects.find(qualifiedName); it != analyzer.m_types.effects.end())
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

	if (type.kind == TypeKind::Ref && type.element)
	{
		return TypeInfo::RefTo(Substitute(*type.element, bindings));
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
		return TypeInfo::Function(
			params,
			Substitute(*type.ret, bindings),
			type.variadic,
			std::move(variadicParam));
	}

	return type;
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::SubstituteTypeParameters(
	const TypeInfo& type,
	const std::unordered_map<std::string, TypeInfo>& bindings) const
{
	return TypeResolver{ *this }.Substitute(type, bindings);
}

bool SemanticAnalyzer::TypeResolver::Satisfies(
	const TypeInfo& actual,
	const std::vector<TypeInfo>& constraints) const
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

bool SemanticAnalyzer::SatisfiesConstraints(
	const TypeInfo& actual,
	const std::vector<TypeInfo>& constraints) const
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

	if (expected.kind == TypeKind::Array
		&& actual.kind == TypeKind::Array
		&& expected.element
		&& actual.element)
	{
		return Bind(*expected.element, *actual.element, bindings);
	}

	if (expected.kind == TypeKind::Pointer
		&& actual.kind == TypeKind::Pointer
		&& expected.element && actual.element)
	{
		return Bind(*expected.element, *actual.element, bindings);
	}

	if (expected.kind == TypeKind::Ref
		&& actual.kind == TypeKind::Ref
		&& expected.element
		&& actual.element)
	{
		return Bind(*expected.element, *actual.element, bindings);
	}

	if (expected.kind == TypeKind::Function
		&& actual.kind == TypeKind::Function
		&& expected.ret
		&& actual.ret)
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