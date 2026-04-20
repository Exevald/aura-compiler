#include "../BytecodeGenerator.h"

#include <memory>
#include <ranges>

using namespace VM::Core;

std::optional<uint8_t> BytecodeGenerator::ResolveStructFieldIndex(const ASTNode* object, const std::string& member) const
{
	return MetadataResolver{ *this }.ResolveStructFieldIndex(object, member);
}

std::optional<std::string> BytecodeGenerator::InferStructTypeName(const ASTNode* node) const
{
	return MetadataResolver{ *this }.InferStructTypeName(node);
}

std::optional<std::string> BytecodeGenerator::ResolveStructVariableType(const std::string& name) const
{
	return MetadataResolver{ *this }.ResolveStructVariableType(name);
}

std::optional<std::string> BytecodeGenerator::InferActorTypeName(const ASTNode* node) const
{
	return MetadataResolver{ *this }.InferActorTypeName(node);
}

std::optional<std::string> BytecodeGenerator::ResolveActorVariableType(const std::string& name) const
{
	return MetadataResolver{ *this }.ResolveActorVariableType(name);
}

std::optional<std::string> BytecodeGenerator::InferEnumTypeName(const ASTNode* node) const
{
	return MetadataResolver{ *this }.InferEnumTypeName(node);
}

std::optional<std::string> BytecodeGenerator::ResolveEnumVariableType(const std::string& name) const
{
	return MetadataResolver{ *this }.ResolveEnumVariableType(name);
}

std::optional<BytecodeGenerator::FunctionContext::InterfaceBinding> BytecodeGenerator::ResolveInterfaceBinding(
	const std::string& name) const
{
	return MetadataResolver{ *this }.ResolveInterfaceBinding(name);
}

std::optional<BytecodeGenerator::FunctionContext::InterfaceBinding> BytecodeGenerator::InferInterfaceBinding(
	const ASTNode* node,
	const std::string& explicitType) const
{
	return MetadataResolver{ *this }.InferInterfaceBinding(node, explicitType);
}

bool BytecodeGenerator::IsFunctionBackedInterfaceMethod(const ASTNode* object, const std::string& member) const
{
	return MetadataResolver{ *this }.IsFunctionBackedInterfaceMethod(object, member);
}

std::optional<std::string> BytecodeGenerator::ResolveStructMethod(const ASTNode* object, const std::string& member) const
{
	return MetadataResolver{ *this }.ResolveStructMethod(object, member);
}

std::optional<std::string> BytecodeGenerator::ResolveActorMethod(const ASTNode* object, const std::string& member) const
{
	return MetadataResolver{ *this }.ResolveActorMethod(object, member);
}

bool BytecodeGenerator::IsActorQueryMethod(const ASTNode* object, const std::string& member) const
{
	return MetadataResolver{ *this }.IsActorQueryMethod(object, member);
}

bool BytecodeGenerator::IsEnumTagAccess(const ASTNode* object, const std::string& member) const
{
	return MetadataResolver{ *this }.IsEnumTagAccess(object, member);
}

std::optional<std::string> BytecodeGenerator::ResolveEffectOperation(const ASTNode* node) const
{
	return MetadataResolver{ *this }.ResolveEffectOperation(node);
}

std::optional<uint8_t> BytecodeGenerator::MetadataResolver::ResolveStructFieldIndex(
	const ASTNode* object,
	const std::string& member) const
{
	const auto structType = InferStructTypeName(object);
	if (!structType)
	{
		return std::nullopt;
	}

	const auto layoutIt = generator.m_metadata.structLayouts.find(*structType);
	if (layoutIt == generator.m_metadata.structLayouts.end())
	{
		return std::nullopt;
	}

	for (size_t index = 0; index < layoutIt->second.size(); ++index)
	{
		if (layoutIt->second[index] == member)
		{
			return static_cast<uint8_t>(index);
		}
	}

	return std::nullopt;
}

std::optional<std::string> BytecodeGenerator::MetadataResolver::InferStructTypeName(const ASTNode* node) const
{
	if (!node)
	{
		return std::nullopt;
	}

	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node))
	{
		const std::string qualifiedName = generator.QualifyName(identifier->name);
		if (generator.m_metadata.structLayouts.contains(qualifiedName))
		{
			return qualifiedName;
		}
		if (generator.m_metadata.structLayouts.contains(identifier->name))
		{
			return identifier->name;
		}
		return ResolveStructVariableType(identifier->name);
	}

	if (const auto* memberAccess = dynamic_cast<const MemberAccessNode*>(node))
	{
		const auto parentStructType = InferStructTypeName(memberAccess->object.get());
		if (!parentStructType)
		{
			if (const auto* moduleIdentifier = dynamic_cast<const IdentifierNode*>(memberAccess->object.get()))
			{
				if (const auto aliasIt = generator.m_metadata.importAliases.find(moduleIdentifier->name);
					aliasIt != generator.m_metadata.importAliases.end())
				{
					if (const std::string qualifiedName = aliasIt->second + "." + memberAccess->member;
						generator.m_metadata.structLayouts.contains(qualifiedName))
					{
						return qualifiedName;
					}
				}
			}
			return std::nullopt;
		}

		const auto fieldTypesIt = generator.m_metadata.structFieldTypes.find(*parentStructType);
		if (fieldTypesIt == generator.m_metadata.structFieldTypes.end())
		{
			return std::nullopt;
		}

		const auto fieldTypeIt = fieldTypesIt->second.find(memberAccess->member);
		if (fieldTypeIt == fieldTypesIt->second.end())
		{
			return std::nullopt;
		}

		const std::string qualifiedFieldType = generator.QualifyName(fieldTypeIt->second);
		if (generator.m_metadata.structLayouts.contains(qualifiedFieldType))
		{
			return qualifiedFieldType;
		}
		if (generator.m_metadata.structLayouts.contains(fieldTypeIt->second))
		{
			return fieldTypeIt->second;
		}
		return std::nullopt;
	}

	if (const auto* call = dynamic_cast<const CallNode*>(node))
	{
		if (const auto* calleeIdentifier = dynamic_cast<const IdentifierNode*>(call->callee.get()))
		{
			const std::string qualifiedName = generator.QualifyName(calleeIdentifier->name);
			if (generator.m_metadata.structLayouts.contains(qualifiedName))
			{
				return qualifiedName;
			}
			if (generator.m_metadata.structLayouts.contains(calleeIdentifier->name))
			{
				return calleeIdentifier->name;
			}
		}

		if (const auto* memberAccess = dynamic_cast<const MemberAccessNode*>(call->callee.get()))
		{
			if (const auto* moduleIdentifier = dynamic_cast<const IdentifierNode*>(memberAccess->object.get()))
			{
				if (const auto aliasIt = generator.m_metadata.importAliases.find(moduleIdentifier->name);
					aliasIt != generator.m_metadata.importAliases.end())
				{
					const std::string qualifiedName = aliasIt->second + "." + memberAccess->member;
					if (generator.m_metadata.structLayouts.contains(qualifiedName))
					{
						return qualifiedName;
					}
				}
			}
		}
	}

	return std::nullopt;
}

std::optional<std::string> BytecodeGenerator::MetadataResolver::ResolveStructVariableType(const std::string& name) const
{
	for (auto contextIt = generator.m_contexts.rbegin(); contextIt != generator.m_contexts.rend(); ++contextIt)
	{
		for (auto scopeIt = contextIt->structVarScopes.rbegin(); scopeIt != contextIt->structVarScopes.rend(); ++scopeIt)
		{
			if (const auto it = scopeIt->find(name); it != scopeIt->end())
			{
				return it->second;
			}
		}
	}
	return std::nullopt;
}

std::optional<std::string> BytecodeGenerator::MetadataResolver::InferActorTypeName(const ASTNode* node) const
{
	if (!node)
	{
		return std::nullopt;
	}

	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node))
	{
		const std::string qualifiedName = generator.QualifyName(identifier->name);
		if (generator.m_metadata.actorLayouts.contains(qualifiedName))
		{
			return qualifiedName;
		}
		if (generator.m_metadata.actorLayouts.contains(identifier->name))
		{
			return identifier->name;
		}
		return ResolveActorVariableType(identifier->name);
	}

	if (const auto* call = dynamic_cast<const CallNode*>(node))
	{
		if (const auto* calleeIdentifier = dynamic_cast<const IdentifierNode*>(call->callee.get()))
		{
			const std::string qualifiedName = generator.QualifyName(calleeIdentifier->name);
			if (generator.m_metadata.actorLayouts.contains(qualifiedName))
			{
				return qualifiedName;
			}
			if (generator.m_metadata.actorLayouts.contains(calleeIdentifier->name))
			{
				return calleeIdentifier->name;
			}
		}
	}

	return std::nullopt;
}

std::optional<std::string> BytecodeGenerator::MetadataResolver::ResolveActorVariableType(const std::string& name) const
{
	for (auto contextIt = generator.m_contexts.rbegin(); contextIt != generator.m_contexts.rend(); ++contextIt)
	{
		for (auto scopeIt = contextIt->actorVarScopes.rbegin(); scopeIt != contextIt->actorVarScopes.rend(); ++scopeIt)
		{
			if (const auto it = scopeIt->find(name); it != scopeIt->end())
			{
				return it->second;
			}
		}
	}
	return std::nullopt;
}

std::optional<std::string> BytecodeGenerator::MetadataResolver::InferEnumTypeName(const ASTNode* node) const
{
	if (!node)
	{
		return std::nullopt;
	}

	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node))
	{
		return ResolveEnumVariableType(identifier->name);
	}

	if (const auto* call = dynamic_cast<const CallNode*>(node))
	{
		if (const auto* calleeIdentifier = dynamic_cast<const IdentifierNode*>(call->callee.get()))
		{
			if (const auto it = generator.m_metadata.enumVariants.find(generator.QualifyName(calleeIdentifier->name));
				it != generator.m_metadata.enumVariants.end())
			{
				return it->second.enumName;
			}
		}

		if (const auto* memberAccess = dynamic_cast<const MemberAccessNode*>(call->callee.get()))
		{
			if (const auto* moduleIdentifier = dynamic_cast<const IdentifierNode*>(memberAccess->object.get()))
			{
				if (const auto aliasIt = generator.m_metadata.importAliases.find(moduleIdentifier->name);
					aliasIt != generator.m_metadata.importAliases.end())
				{
					if (const auto it = generator.m_metadata.enumVariants.find(aliasIt->second + "." + memberAccess->member);
						it != generator.m_metadata.enumVariants.end())
					{
						return it->second.enumName;
					}
				}
			}
		}
	}

	return std::nullopt;
}

std::optional<std::string> BytecodeGenerator::MetadataResolver::ResolveEnumVariableType(const std::string& name) const
{
	for (auto contextIt = generator.m_contexts.rbegin(); contextIt != generator.m_contexts.rend(); ++contextIt)
	{
		for (auto scopeIt = contextIt->enumVarScopes.rbegin(); scopeIt != contextIt->enumVarScopes.rend(); ++scopeIt)
		{
			if (const auto it = scopeIt->find(name); it != scopeIt->end())
			{
				return it->second;
			}
		}
	}
	return std::nullopt;
}

std::optional<BytecodeGenerator::FunctionContext::InterfaceBinding> BytecodeGenerator::MetadataResolver::ResolveInterfaceBinding(
	const std::string& name) const
{
	for (auto contextIt = generator.m_contexts.rbegin(); contextIt != generator.m_contexts.rend(); ++contextIt)
	{
		for (auto scopeIt = contextIt->interfaceVarScopes.rbegin(); scopeIt != contextIt->interfaceVarScopes.rend(); ++scopeIt)
		{
			if (const auto it = scopeIt->find(name); it != scopeIt->end())
			{
				return it->second;
			}
		}
	}
	return std::nullopt;
}

std::optional<BytecodeGenerator::FunctionContext::InterfaceBinding> BytecodeGenerator::MetadataResolver::InferInterfaceBinding(
	const ASTNode* node,
	const std::string& explicitType) const
{
	auto normalizeInterfaceName = [&](const std::string& typeName) -> std::string {
		if (typeName.empty() || typeName == "auto")
		{
			return {};
		}
		if (generator.m_metadata.interfaceMethods.contains(generator.QualifyName(typeName)))
		{
			return generator.QualifyName(typeName);
		}
		if (generator.m_metadata.interfaceMethods.contains(typeName))
		{
			return typeName;
		}
		return {};
	};

	const std::string interfaceName = normalizeInterfaceName(explicitType);
	if (!node)
	{
		if (!interfaceName.empty())
		{
			return FunctionContext::InterfaceBinding{
				interfaceName,
				"",
				FunctionContext::InterfaceBinding::RuntimeKind::Unknown
			};
		}
		return std::nullopt;
	}

	if (dynamic_cast<const FunctionExprNode*>(node))
	{
		if (!interfaceName.empty())
		{
			return FunctionContext::InterfaceBinding{
				interfaceName,
				"",
				FunctionContext::InterfaceBinding::RuntimeKind::Function
			};
		}
		return std::nullopt;
	}

	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node))
	{
		if (const auto binding = ResolveInterfaceBinding(identifier->name))
		{
			return binding;
		}

		if (!interfaceName.empty())
		{
			if (generator.m_metadata.importAliases.contains(identifier->name))
			{
				return FunctionContext::InterfaceBinding{
					interfaceName,
					"",
					FunctionContext::InterfaceBinding::RuntimeKind::Module
				};
			}

			if (const auto structType = ResolveStructVariableType(identifier->name))
			{
				return FunctionContext::InterfaceBinding{
					interfaceName,
					*structType,
					FunctionContext::InterfaceBinding::RuntimeKind::Struct
				};
			}

			return FunctionContext::InterfaceBinding{
				interfaceName,
				"",
				FunctionContext::InterfaceBinding::RuntimeKind::Function
			};
		}
	}

	if (!interfaceName.empty())
	{
		if (const auto structType = InferStructTypeName(node))
		{
			return FunctionContext::InterfaceBinding{
				interfaceName,
				*structType,
				FunctionContext::InterfaceBinding::RuntimeKind::Struct
			};
		}
	}

	return std::nullopt;
}

bool BytecodeGenerator::MetadataResolver::IsFunctionBackedInterfaceMethod(const ASTNode* object, const std::string& member) const
{
	const auto* identifier = dynamic_cast<const IdentifierNode*>(object);
	if (!identifier)
	{
		return false;
	}

	const auto binding = ResolveInterfaceBinding(identifier->name);
	if (!binding || binding->runtimeKind != FunctionContext::InterfaceBinding::RuntimeKind::Function)
	{
		return false;
	}

	const auto methodsIt = generator.m_metadata.interfaceMethods.find(binding->interfaceName);
	if (methodsIt == generator.m_metadata.interfaceMethods.end() || methodsIt->second.size() != 1)
	{
		return false;
	}

	return methodsIt->second.front() == member;
}

std::optional<std::string> BytecodeGenerator::MetadataResolver::ResolveStructMethod(const ASTNode* object, const std::string& member) const
{
	if (const auto structType = InferStructTypeName(object))
	{
		if (const auto it = generator.m_metadata.structMethodNames.find(*structType);
			it != generator.m_metadata.structMethodNames.end())
		{
			if (const auto methodIt = it->second.find(member); methodIt != it->second.end())
			{
				return methodIt->second;
			}
		}
	}

	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(object))
	{
		if (const auto binding = ResolveInterfaceBinding(identifier->name))
		{
			if (binding->runtimeKind == FunctionContext::InterfaceBinding::RuntimeKind::Struct
				&& !binding->structType.empty())
			{
				if (const auto it = generator.m_metadata.structMethodNames.find(binding->structType);
					it != generator.m_metadata.structMethodNames.end())
				{
					if (const auto methodIt = it->second.find(member); methodIt != it->second.end())
					{
						return methodIt->second;
					}
				}
			}
		}
	}

	return std::nullopt;
}

std::optional<std::string> BytecodeGenerator::MetadataResolver::ResolveActorMethod(const ASTNode* object, const std::string& member) const
{
	if (const auto actorType = InferActorTypeName(object))
	{
		if (const auto it = generator.m_metadata.actorMethodNames.find(*actorType);
			it != generator.m_metadata.actorMethodNames.end())
		{
			if (const auto methodIt = it->second.find(member); methodIt != it->second.end())
			{
				return methodIt->second;
			}
		}
	}

	return std::nullopt;
}

bool BytecodeGenerator::MetadataResolver::IsActorQueryMethod(const ASTNode* object, const std::string& member) const
{
	if (const auto actorType = InferActorTypeName(object))
	{
		if (const auto it = generator.m_metadata.actorMethodIsQuery.find(*actorType);
			it != generator.m_metadata.actorMethodIsQuery.end())
		{
			if (const auto methodIt = it->second.find(member); methodIt != it->second.end())
			{
				return methodIt->second;
			}
		}
	}
	return false;
}

bool BytecodeGenerator::MetadataResolver::IsEnumTagAccess(const ASTNode* object, const std::string& member) const
{
	if (member != "tag")
	{
		return false;
	}

	if (InferEnumTypeName(object))
	{
		return true;
	}

	if (const auto* call = dynamic_cast<const CallNode*>(object))
	{
		if (const auto* identifier = dynamic_cast<const IdentifierNode*>(call->callee.get()))
		{
			return generator.m_metadata.enumVariants.contains(generator.QualifyName(identifier->name));
		}

		if (const auto* memberAccess = dynamic_cast<const MemberAccessNode*>(call->callee.get()))
		{
			if (const auto* moduleIdentifier = dynamic_cast<const IdentifierNode*>(memberAccess->object.get()))
			{
				if (const auto aliasIt = generator.m_metadata.importAliases.find(moduleIdentifier->name);
					aliasIt != generator.m_metadata.importAliases.end())
				{
					return generator.m_metadata.enumVariants.contains(aliasIt->second + "." + memberAccess->member);
				}
			}
		}
	}

	return false;
}

std::optional<std::string> BytecodeGenerator::MetadataResolver::ResolveEffectOperation(const ASTNode* node) const
{
	const auto* identifier = dynamic_cast<const IdentifierNode*>(node);
	if (!identifier)
	{
		return std::nullopt;
	}

	for (const auto& operations : generator.m_metadata.effectOperations | std::views::values)
	{
		if (operations.contains(identifier->name))
		{
			return identifier->name;
		}
	}
	return std::nullopt;
}