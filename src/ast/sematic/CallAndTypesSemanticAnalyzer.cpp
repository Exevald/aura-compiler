#include "../common/SemanticAnalyzerSupport.h"
#include "SemanticAnalyzer.h"

#include <algorithm>
#include <ranges>
#include <stdexcept>

using SemanticAnalyzerDetail::FormatUndefined;
using SemanticAnalyzerDetail::ScopeExit;
using SemanticAnalyzerDetail::SplitGenericName;
using SemanticAnalyzerDetail::SubstituteTypeString;

namespace
{

bool IsSpawnModuleName(const std::string& moduleName)
{
	return moduleName == "std.task"
		|| moduleName == "std.concurrent.waitgroup"
		|| moduleName == "std.concurrent.errorgroup";
}

} // namespace

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
	std::string signatureLookupName = funcType.name;
	if (const auto angle = signatureLookupName.find('<'); angle != std::string::npos)
	{
		signatureLookupName = signatureLookupName.substr(0, angle);
	}
	auto ctorIt = analyzer.m_types.constructorSignatures.find(signatureLookupName);
	if (ctorIt == analyzer.m_types.constructorSignatures.end())
	{
		ctorIt = analyzer.m_types.constructorSignatures.find(analyzer.QualifyName(signatureLookupName));
	}
	if (ctorIt == analyzer.m_types.constructorSignatures.end())
	{
		throw std::runtime_error("Struct metadata is missing");
	}

	const auto& ctorSignature = ctorIt->second;
	if (args.size() < ctorSignature.requiredArity || args.size() > ctorSignature.params.size())
	{
		throw std::runtime_error(
			"Struct constructor expects "
			+ (ctorSignature.requiredArity == ctorSignature.params.size()
					? std::to_string(ctorSignature.params.size())
					: (std::to_string(ctorSignature.requiredArity) + ".."
						  + std::to_string(ctorSignature.params.size())))
			+ " arguments, but got " + std::to_string(args.size()));
	}

	std::unordered_map<std::string, TypeInfo> genericBindings;
	for (size_t index = 0; index < args.size(); ++index)
	{
		const auto& [fieldName, fieldType] = ctorSignature.params[index];
		const TypeInfo argType = analyzer.VisitAndGet(args[index].get());
		(void)analyzer.BindGenericType(fieldType, argType, genericBindings);
		const TypeInfo expectedFieldType = analyzer.SubstituteTypeParameters(fieldType, genericBindings);
		ValidateTypeCompatibility(
			expectedFieldType,
			argType,
			"Struct field argument mismatch for '" + fieldName + "'");
	}

	if (genericBindings.empty())
	{
		analyzer.m_lastType = funcType;
		return;
	}

	auto typeToString = [&](const TypeInfo& type, const auto& self) -> std::string {
		switch (type.kind)
		{
		case TypeKind::Any:
			return "any";
		case TypeKind::Int:
			return "int";
		case TypeKind::Float:
			return "float";
		case TypeKind::String:
			return "string";
		case TypeKind::Bool:
			return "bool";
		case TypeKind::Void:
			return "void";
		case TypeKind::Never:
			return "never";
		case TypeKind::Array:
			return type.element ? "[" + self(*type.element, self) + "]" : "[]";
		case TypeKind::Channel:
			return type.element ? "channel<" + self(*type.element, self) + ">" : "channel<any>";
		case TypeKind::Pointer:
			return type.element ? "ptr<" + self(*type.element, self) + ">" : "ptr<any>";
		case TypeKind::Ref:
			return type.element ? "ref<" + self(*type.element, self) + ">" : "ref<any>";
		default:
			return !type.name.empty() ? type.name : "any";
		}
	};

	std::vector<std::string> orderedTypeArgs;
	if (auto genericIt = analyzer.m_types.genericStructs.find(funcType.name);
		genericIt == analyzer.m_types.genericStructs.end())
	{
		genericIt = analyzer.m_types.genericStructs.find(analyzer.QualifyName(funcType.name));
		if (genericIt != analyzer.m_types.genericStructs.end())
		{
			for (const auto& typeParam : genericIt->second.typeParams)
			{
				orderedTypeArgs.push_back(typeToString(
					genericBindings.contains(typeParam.name)
						? genericBindings.at(typeParam.name)
						: TypeInfo::Unknown(),
					typeToString));
			}
		}
	}
	else
	{
		for (const auto& typeParam : genericIt->second.typeParams)
		{
			orderedTypeArgs.push_back(typeToString(
				genericBindings.contains(typeParam.name)
					? genericBindings.at(typeParam.name)
					: TypeInfo::Unknown(),
				typeToString));
		}
	}

	auto concreteType = analyzer.SubstituteTypeParameters(funcType, genericBindings);
	if (!orderedTypeArgs.empty())
	{
		std::string resolvedName = funcType.name + "<";
		for (size_t i = 0; i < orderedTypeArgs.size(); ++i)
		{
			if (i != 0)
			{
				resolvedName += ", ";
			}
			resolvedName += orderedTypeArgs[i];
		}
		resolvedName += ">";
		concreteType.name = resolvedName;
	}
	analyzer.m_lastType = concreteType;
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

	std::unordered_map<std::string, TypeInfo> genericBindings;
	std::string genericBase;
	std::vector<std::string> constructorGenericArgs;
	if (SplitGenericName(funcType.name, genericBase, constructorGenericArgs))
	{
		std::string expectedBase;
		std::vector<std::string> expectedGenericArgs;
		if (analyzer.m_currentExpectedReturn.kind == TypeKind::Enum
			&& SplitGenericName(analyzer.m_currentExpectedReturn.name, expectedBase, expectedGenericArgs)
			&& expectedBase == genericBase
			&& expectedGenericArgs.size() == constructorGenericArgs.size())
		{
			auto genericIt = analyzer.m_types.genericEnums.find(genericBase);
			if (genericIt == analyzer.m_types.genericEnums.end())
			{
				genericIt = analyzer.m_types.genericEnums.find(analyzer.QualifyName(genericBase));
			}
			if (genericIt != analyzer.m_types.genericEnums.end()
				&& genericIt->second.typeParams.size() == expectedGenericArgs.size())
			{
				for (size_t i = 0; i < expectedGenericArgs.size(); ++i)
				{
					genericBindings[genericIt->second.typeParams[i].name]
						= analyzer.StringToType(expectedGenericArgs[i]);
				}
			}
		}
	}

	for (size_t i = 0; i < args.size(); ++i)
	{
		const TypeInfo argType = analyzer.VisitAndGet(args[i].get());
		if (!analyzer.BindGenericType(funcType.variantArgs[i], argType, genericBindings)
			&& argType.kind != TypeKind::Unknown)
		{
			throw std::runtime_error("Enum constructor argument does not satisfy generic constraints");
		}
		const TypeInfo expectedType
			= analyzer.SubstituteTypeParameters(funcType.variantArgs[i], genericBindings);
		ValidateTypeCompatibility(
			expectedType,
			argType,
			"Enum constructor argument mismatch");
	}

	std::unordered_map<std::string, std::string> nameBindings;
	for (const auto& [genericName, boundType] : genericBindings)
	{
		switch (boundType.kind)
		{
		case TypeKind::Int:
			nameBindings[genericName] = "int";
			break;
		case TypeKind::Float:
			nameBindings[genericName] = "float";
			break;
		case TypeKind::String:
			nameBindings[genericName] = "string";
			break;
		case TypeKind::Bool:
			nameBindings[genericName] = "bool";
			break;
		default:
			if (!boundType.name.empty())
			{
				nameBindings[genericName] = boundType.name;
			}
			break;
		}
	}
	analyzer.m_lastType = analyzer.StringToType(SubstituteTypeString(funcType.name, nameBindings));
}

void SemanticAnalyzer::CallAnalyzer::AnalyzeFunctionCall(
	const TypeInfo& funcType,
	const std::vector<ASTNodePtr>& args) const
{
	ValidateContextRequirements(funcType);
	if ((!funcType.variadic && (args.size() < funcType.minArity || args.size() > funcType.params.size()))
		|| (funcType.variadic && args.size() < funcType.minArity))
	{
		throw std::runtime_error(
			"Function call expects "
			+ (funcType.minArity == funcType.params.size()
					? std::to_string(funcType.params.size())
					: std::to_string(funcType.minArity) + ".." + std::to_string(funcType.params.size()))
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

void SemanticAnalyzer::CallAnalyzer::ValidateContextRequirements(const TypeInfo& funcType) const
{
	if (!funcType.contextRequirements)
	{
		return;
	}
	for (const auto& [name, expectedType] : *funcType.contextRequirements)
	{
		const TypeInfo actualType = analyzer.Resolve(name);
		if (actualType.kind == TypeKind::Unknown)
		{
			throw std::runtime_error("Missing context binding '" + name + "' for call");
		}
		if (!analyzer.IsAssignable(expectedType, actualType))
		{
			throw std::runtime_error("Context binding '" + name + "' has incompatible type");
		}
	}
}

void SemanticAnalyzer::CallAnalyzer::Analyze(const CallNode& node) const
{
	if (const auto* member = dynamic_cast<const MemberAccessNode*>(node.callee.get()))
	{
		const TypeInfo receiverType = analyzer.VisitAndGet(member->object.get());
		if (receiverType.kind == TypeKind::Module
			&& receiverType.name == "std.channel"
			&& member->member == "make")
		{
			if (node.args.size() != 1)
			{
				throw std::runtime_error("make expects exactly one argument");
			}
			(void)analyzer.VisitAndGet(node.args[0].get());
			if (analyzer.m_currentExpectedReturn.kind == TypeKind::Channel)
			{
				analyzer.m_lastType = analyzer.m_currentExpectedReturn;
			}
			else
			{
				analyzer.m_lastType = TypeInfo::ChannelOf(TypeInfo::Any());
			}
			return;
		}

		if (receiverType.kind == TypeKind::Module
			&& receiverType.name == "std.channel"
			&& member->member == "recv")
		{
			if (node.args.size() != 1)
			{
				throw std::runtime_error("recv expects exactly one argument");
			}
			(void)analyzer.VisitAndGet(node.args[0].get());
			if (analyzer.m_currentExpectedReturn.kind == TypeKind::Enum)
			{
				analyzer.m_lastType = analyzer.m_currentExpectedReturn;
				return;
			}
		}

		if (receiverType.kind == TypeKind::Module
			&& receiverType.name == "std.task"
			&& member->member == "join_result")
		{
			if (node.args.size() != 1)
			{
				throw std::runtime_error("join_result expects exactly one argument");
			}
			(void)analyzer.VisitAndGet(node.args[0].get());
			if (analyzer.m_currentExpectedReturn.kind == TypeKind::Enum)
			{
				analyzer.m_lastType = analyzer.m_currentExpectedReturn;
				return;
			}
		}

		if (receiverType.kind == TypeKind::Module
			&& member->member == "spawn"
			&& IsSpawnModuleName(receiverType.name))
		{
			const size_t calleeIndex = receiverType.name == "std.task" ? 0 : 1;
			if (node.args.size() <= calleeIndex)
			{
				throw std::runtime_error(member->member + " expects a callable argument");
			}

			auto* calleeExpr = node.args[calleeIndex].get();
			if (!dynamic_cast<const IdentifierNode*>(calleeExpr)
				&& !dynamic_cast<const MemberAccessNode*>(calleeExpr))
			{
				throw std::runtime_error("spawn currently supports only named or module-qualified function calls");
			}

			const TypeInfo calleeType = analyzer.VisitAndGet(calleeExpr);
			if (const auto* calleeMember = dynamic_cast<const MemberAccessNode*>(calleeExpr))
			{
				const TypeInfo calleeReceiverType = analyzer.VisitAndGet(calleeMember->object.get());
				if (calleeReceiverType.kind != TypeKind::Module)
				{
					analyzer.ValidateSendable(calleeReceiverType, "spawn receiver");
				}
			}

			for (size_t i = calleeIndex + 1; i < node.args.size(); ++i)
			{
				analyzer.ValidateSendable(analyzer.VisitAndGet(node.args[i].get()), "spawn argument");
			}

			if (calleeType.kind == TypeKind::Function && calleeType.ret)
			{
				if (receiverType.name == "std.task")
				{
					analyzer.m_lastType = TypeInfo::TaskOf(*calleeType.ret);
				}
				else
				{
					if (calleeType.ret->kind != TypeKind::Void)
					{
						throw std::runtime_error(member->member + " requires a callable returning void");
					}
					analyzer.m_lastType = TypeInfo::TaskOf(TypeInfo::Void());
				}
			}
			else
			{
				analyzer.m_lastType = TypeInfo::TaskOf(receiverType.name == "std.task" ? TypeInfo::Any() : TypeInfo::Void());
			}
			return;
		}
	}

	if (analyzer.IsUnsafeMemoryCall(node) && analyzer.m_unsafeDepth <= 0)
	{
		throw std::runtime_error("std.memory.alloc/free require an unsafe block");
	}

	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node.callee.get()))
	{
		if (identifier->name == "resume")
		{
			if (analyzer.m_activeResumeTypes.empty())
			{
				throw std::runtime_error("resume(value) is only available inside an effect handler");
			}
			if (node.args.size() != 1)
			{
				throw std::runtime_error("resume(value) expects exactly one argument");
			}

			const TypeInfo actualType = analyzer.VisitAndGet(node.args[0].get());
			const TypeInfo expectedType = analyzer.m_activeResumeTypes.back();
			if (!analyzer.IsAssignable(expectedType, actualType))
			{
				throw std::runtime_error("resume(value) type mismatch");
			}
			analyzer.m_lastType = expectedType;
			return;
		}

		const std::vector<std::string> candidateEffects = analyzer.ResolveAccessibleEffectsForOperation(identifier->name);
		if (!candidateEffects.empty())
		{
			if (analyzer.m_comptimeDepth > 0)
			{
				throw std::runtime_error("Effect operations are not allowed in comptime context");
			}
			if (candidateEffects.size() > 1)
			{
				throw std::runtime_error("Ambiguous effect operation: " + identifier->name);
			}
			const auto effectIt = analyzer.m_types.effects.find(candidateEffects.front());
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
	if (analyzer.m_comptimeDepth > 0 && !funcType.isComptime)
	{
		throw std::runtime_error("Runtime-only function call is not allowed in comptime context");
	}
	if (analyzer.m_comptimeDepth == 0 && funcType.isComptime)
	{
		throw std::runtime_error("comptime function can only be called from comptime context");
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
	auto resolveImportedQualifiedName = [&](const std::string& rawName) -> std::optional<std::string> {
		const auto dot = rawName.find('.');
		if (dot == std::string::npos)
		{
			return std::nullopt;
		}
		const std::string alias = rawName.substr(0, dot);
		try
		{
			const TypeInfo aliasType = analyzer.Resolve(alias);
			if (aliasType.kind == TypeKind::Module)
			{
				return aliasType.name + rawName.substr(dot);
			}
		}
		catch (const std::runtime_error&)
		{
		}
		return std::nullopt;
	};

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
		if (const auto importedQualifiedName = resolveImportedQualifiedName(name))
		{
			return ResolveTypeName(*importedQualifiedName);
		}
		auto resolveNominalGeneric = [&](const auto& registry) -> std::optional<TypeInfo> {
			auto it = registry.find(genericBase);
			if (it == registry.end())
			{
				it = registry.find(analyzer.QualifyName(genericBase));
			}
			if (it == registry.end() || it->second.typeParams.size() != genericArgs.size())
			{
				return std::nullopt;
			}
			std::unordered_map<std::string, TypeInfo> bindings;
			for (size_t i = 0; i < genericArgs.size(); ++i)
			{
				bindings[it->second.typeParams[i].name] = ResolveTypeName(genericArgs[i]);
			}
			auto resolved = Substitute(it->second.templatedType, bindings);
			resolved.name = name;
			return resolved;
		};
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
		if (const auto resolved = resolveNominalGeneric(analyzer.m_types.genericStructs))
		{
			return *resolved;
		}
		if (const auto resolved = resolveNominalGeneric(analyzer.m_types.genericEnums))
		{
			return *resolved;
		}
		if (const auto resolved = resolveNominalGeneric(analyzer.m_types.genericInterfaces))
		{
			return *resolved;
		}
		if (const auto resolved = resolveNominalGeneric(analyzer.m_types.genericActors))
		{
			return *resolved;
		}
		if (const auto it = analyzer.m_types.structs.find(genericBase);
			it != analyzer.m_types.structs.end())
		{
			auto resolved = it->second;
			resolved.name = name;
			return resolved;
		}
		if (const auto it = analyzer.m_types.enums.find(genericBase);
			it != analyzer.m_types.enums.end())
		{
			auto resolved = it->second;
			resolved.name = name;
			return resolved;
		}
		if (const auto it = analyzer.m_types.interfaces.find(genericBase);
			it != analyzer.m_types.interfaces.end())
		{
			auto resolved = it->second;
			resolved.name = name;
			return resolved;
		}
		if (const auto it = analyzer.m_types.actors.find(genericBase);
			it != analyzer.m_types.actors.end())
		{
			auto resolved = it->second;
			resolved.name = name;
			return resolved;
		}
		const std::string qualifiedGenericBase = analyzer.QualifyName(genericBase);
		if (const auto it = analyzer.m_types.structs.find(qualifiedGenericBase);
			it != analyzer.m_types.structs.end())
		{
			auto resolved = it->second;
			resolved.name = name;
			return resolved;
		}
		if (const auto it = analyzer.m_types.enums.find(qualifiedGenericBase);
			it != analyzer.m_types.enums.end())
		{
			auto resolved = it->second;
			resolved.name = name;
			return resolved;
		}
		if (const auto it = analyzer.m_types.interfaces.find(qualifiedGenericBase);
			it != analyzer.m_types.interfaces.end())
		{
			auto resolved = it->second;
			resolved.name = name;
			return resolved;
		}
		if (const auto it = analyzer.m_types.actors.find(qualifiedGenericBase);
			it != analyzer.m_types.actors.end())
		{
			auto resolved = it->second;
			resolved.name = name;
			return resolved;
		}
	}

	if (name == "int")
	{
		return TypeInfo::Int();
	}
	if (name == "any")
	{
		return TypeInfo::Any();
	}
	if (name == "float")
	{
		return TypeInfo::Float();
	}
	if (name == "string")
	{
		return TypeInfo::String();
	}
	if (name == "never")
	{
		return TypeInfo::Never();
	}
	if (name == "bool")
	{
		return TypeInfo::Bool();
	}
	if (name == "void")
	{
		return TypeInfo::Void();
	}

	if (const auto importedQualifiedName = resolveImportedQualifiedName(name))
	{
		return ResolveTypeName(*importedQualifiedName);
	}

	if (name.size() >= 2 && name.front() == '(' && name.back() == ')')
	{
		return ResolveTypeName(name.substr(1, name.size() - 2));
	}

	if (name.size() > 5 && name.rfind("ptr<", 0) == 0 && name.back() == '>')
	{
		const std::string inner = name.substr(4, name.size() - 5);
		return TypeInfo::PointerTo(ResolveTypeName(inner));
	}

	if (name.size() > 6 && name.rfind("task<", 0) == 0 && name.back() == '>')
	{
		const std::string inner = name.substr(5, name.size() - 6);
		return TypeInfo::TaskOf(ResolveTypeName(inner));
	}

	if (name.size() > 9 && name.rfind("channel<", 0) == 0 && name.back() == '>')
	{
		const std::string inner = name.substr(8, name.size() - 9);
		return TypeInfo::ChannelOf(ResolveTypeName(inner));
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
	std::string mapBase;
	std::vector<std::string> mapArgs;
	if (SplitGenericName(name, mapBase, mapArgs))
	{
		if (mapBase == "map" && mapArgs.size() == 2)
		{
			return TypeInfo::MapOf(ResolveTypeName(mapArgs[0]), ResolveTypeName(mapArgs[1]));
		}
	}

	size_t arrowPos = std::string::npos;
	size_t depth = 0;
	for (size_t i = 0; i + 1 < name.size(); ++i)
	{
		if (name[i] == '<' || name[i] == '[' || name[i] == '(')
		{
			++depth;
			continue;
		}
		if ((name[i] == '>' || name[i] == ']' || name[i] == ')') && depth > 0)
		{
			--depth;
			continue;
		}
		if (depth == 0 && name[i] == '-' && name[i + 1] == '>')
		{
			arrowPos = i;
			break;
		}
	}
	if (arrowPos != std::string::npos)
	{
		const std::string left = name.substr(0, arrowPos);
		const std::string right = name.substr(arrowPos + 2);
		std::vector<TypeInfo> params;
		if (left.size() >= 2 && left.front() == '(' && left.back() == ')')
		{
			const std::string inner = left.substr(1, left.size() - 2);
			size_t segmentStart = 0;
			size_t segmentDepth = 0;
			for (size_t i = 0; i <= inner.size(); ++i)
			{
				if (i < inner.size())
				{
					if (inner[i] == '<' || inner[i] == '[' || inner[i] == '(')
					{
						++segmentDepth;
					}
					else if ((inner[i] == '>' || inner[i] == ']' || inner[i] == ')') && segmentDepth > 0)
					{
						--segmentDepth;
					}
				}
				if (i == inner.size() || (inner[i] == ',' && segmentDepth == 0))
				{
					const std::string token = inner.substr(segmentStart, i - segmentStart);
					if (!token.empty())
					{
						params.push_back(ResolveTypeName(token));
					}
					segmentStart = i + 1;
				}
			}
		}
		else
		{
			params.push_back(ResolveTypeName(left));
		}
		return TypeInfo::Function(std::move(params), ResolveTypeName(right));
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
	if (type.kind == TypeKind::Map && type.key && type.element)
	{
		return TypeInfo::MapOf(Substitute(*type.key, bindings), Substitute(*type.element, bindings));
	}

	if (type.kind == TypeKind::Channel && type.element)
	{
		return TypeInfo::ChannelOf(Substitute(*type.element, bindings));
	}

	if (type.kind == TypeKind::Task && type.element)
	{
		return TypeInfo::TaskOf(Substitute(*type.element, bindings));
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
			type.minArity,
			type.raisedEffects ? *type.raisedEffects : std::unordered_set<std::string>{},
			type.contextRequirements ? *type.contextRequirements : std::vector<std::pair<std::string, TypeInfo>>{},
			type.isComptime,
			type.variadic,
			std::move(variadicParam));
	}

	if ((type.kind == TypeKind::Struct || type.kind == TypeKind::Actor || type.kind == TypeKind::Interface)
		&& type.fields)
	{
		std::unordered_map<std::string, TypeInfo> fields;
		for (const auto& [fieldName, fieldType] : *type.fields)
		{
			fields.emplace(fieldName, Substitute(fieldType, bindings));
		}
		std::unordered_map<std::string, TypeInfo> methods;
		if (type.methods)
		{
			for (const auto& [methodName, methodType] : *type.methods)
			{
				methods.emplace(methodName, Substitute(methodType, bindings));
			}
		}
		if (type.kind == TypeKind::Struct)
		{
			return TypeInfo::Struct(
				type.name,
				std::move(fields),
				std::move(methods),
				type.implementedInterfaces ? *type.implementedInterfaces : std::unordered_set<std::string>{});
		}
		if (type.kind == TypeKind::Actor)
		{
			return TypeInfo::Actor(type.name, std::move(fields), std::move(methods));
		}
		return TypeInfo::Interface(type.name, std::move(methods));
	}

	if (type.kind == TypeKind::Interface && type.methods)
	{
		std::unordered_map<std::string, TypeInfo> methods;
		for (const auto& [methodName, methodType] : *type.methods)
		{
			methods.emplace(methodName, Substitute(methodType, bindings));
		}
		return TypeInfo::Interface(type.name, std::move(methods));
	}

	if (type.kind == TypeKind::EnumConstructor)
	{
		std::vector<TypeInfo> args;
		args.reserve(type.variantArgs.size());
		for (const auto& argType : type.variantArgs)
		{
			args.push_back(Substitute(argType, bindings));
		}
		return TypeInfo::EnumConstructor(type.name, type.enumTag, std::move(args));
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

	if (expected.kind == TypeKind::Channel
		&& actual.kind == TypeKind::Channel
		&& expected.element
		&& actual.element)
	{
		return Bind(*expected.element, *actual.element, bindings);
	}

	if (expected.kind == TypeKind::Task
		&& actual.kind == TypeKind::Task
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
		if (expected.minArity != actual.minArity || expected.isComptime != actual.isComptime)
		{
			return false;
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
				if ((*expected.contextRequirements)[i].first != (*actual.contextRequirements)[i].first
					|| !Bind((*expected.contextRequirements)[i].second, (*actual.contextRequirements)[i].second, bindings))
				{
					return false;
				}
			}
		}
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
		if ((!expected.raisedEffects || !actual.raisedEffects)
				? (expected.raisedEffects != actual.raisedEffects)
				: (*expected.raisedEffects != *actual.raisedEffects))
		{
			return false;
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
