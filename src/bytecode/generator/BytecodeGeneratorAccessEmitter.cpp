#include "../BytecodeGenerator.h"

#include <memory>
#include <stdexcept>

using namespace VM::Core;

namespace
{
std::string GenericBaseName(const std::string& name)
{
	if (const auto pos = name.find('<'); pos != std::string::npos)
	{
		return name.substr(0, pos);
	}
	return name;
}
}

void BytecodeGenerator::AccessEmitter::EmitCallArguments(
	const std::vector<ASTNodePtr>& args,
	const std::vector<bool>& refParams,
	const size_t refOffset) const
{
	for (size_t i = 0; i < args.size(); ++i)
	{
		const size_t refIndex = i + refOffset;
		const bool byRef = refIndex < refParams.size() && refParams[refIndex];
		generator.EmitCallArgument(args[i].get(), byRef);
	}
}

void BytecodeGenerator::AccessEmitter::EmitContextArguments(const std::vector<std::string>& contextParams) const
{
	for (const auto& contextName : contextParams)
	{
		generator.EmitGetVariable(contextName);
	}
}

bool BytecodeGenerator::AccessEmitter::TryEmitDirectTypeConstructorCall(const CallNode& node) const
{
	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node.callee.get()))
	{
		if (const auto structName = generator.EnsureStructMetadata(identifier->name))
		{
			const auto it = generator.m_metadata.structLayouts.find(*structName);
			EmitCallArguments(node.args);
			generator.CurrentChunk().Write(OpCode::OP_BUILD_STRUCT);
			generator.CurrentChunk().code.push_back(static_cast<uint8_t>(it->second.size()));
			if (const auto invariantName = generator.m_metadata.structInvariantChecks.find(*structName);
				invariantName != generator.m_metadata.structInvariantChecks.end())
			{
				const std::string hiddenInvariantName = invariantName->second;
				generator.CurrentChunk().Write(OpCode::OP_DUP);
				const uint16_t fnIndex = generator.CurrentChunk().AddConstant(
					std::make_shared<const std::string>(hiddenInvariantName));
				generator.CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
				generator.CurrentChunk().WriteOperand(OpCode::OP_GET_GLOBAL, fnIndex);
				generator.CurrentChunk().Write(OpCode::OP_SWAP);
				generator.CurrentChunk().Write(OpCode::OP_CALL);
				generator.CurrentChunk().code.push_back(1);
				generator.CurrentChunk().Write(OpCode::OP_POP);
			}
			return true;
		}

		if (const auto it = generator.m_metadata.enumVariants.find(generator.QualifyName(identifier->name));
			it != generator.m_metadata.enumVariants.end())
		{
			EmitCallArguments(node.args);
			generator.CurrentChunk().Write(OpCode::OP_BUILD_ENUM);
			generator.CurrentChunk().code.push_back(it->second.tag);
			generator.CurrentChunk().code.push_back(it->second.argCount);
			return true;
		}

		if (const auto actorName = generator.EnsureActorMetadata(identifier->name))
		{
			const auto actorIt = generator.m_metadata.actorLayouts.find(*actorName);
			EmitCallArguments(node.args);
			if (const auto defaultsIt = generator.m_metadata.actorFieldDefaults.find(*actorName);
				defaultsIt != generator.m_metadata.actorFieldDefaults.end())
			{
				for (size_t i = node.args.size(); i < defaultsIt->second.size(); ++i)
				{
					if (defaultsIt->second[i])
					{
						defaultsIt->second[i]->Accept(generator);
					}
					else
					{
						generator.CurrentChunk().WriteConstant(std::monostate{});
					}
				}
			}
			const uint16_t blueprintIndex = generator.CurrentChunk().AddConstant(
				std::make_shared<const std::string>(*actorName));
			generator.CurrentChunk().Write(OpCode::OP_BUILD_ACTOR);
			generator.CurrentChunk().WriteOperand(OpCode::OP_BUILD_ACTOR, blueprintIndex);
			generator.CurrentChunk().code.push_back(static_cast<uint8_t>(actorIt->second.size()));
			return true;
		}
	}

	if (const auto* memberAccess = dynamic_cast<const MemberAccessNode*>(node.callee.get()))
	{
		if (const auto* moduleIdentifier = dynamic_cast<const IdentifierNode*>(memberAccess->object.get()))
		{
			if (const auto aliasIt = generator.m_metadata.importAliases.find(moduleIdentifier->name);
				aliasIt != generator.m_metadata.importAliases.end())
			{
				const std::string qualifiedName = aliasIt->second + "." + memberAccess->member;
				if (const auto structName = generator.EnsureStructMetadata(qualifiedName))
				{
					const auto it = generator.m_metadata.structLayouts.find(*structName);
					EmitCallArguments(node.args);
					generator.CurrentChunk().Write(OpCode::OP_BUILD_STRUCT);
					generator.CurrentChunk().code.push_back(static_cast<uint8_t>(it->second.size()));
					if (const auto invariantName = generator.m_metadata.structInvariantChecks.find(*structName);
						invariantName != generator.m_metadata.structInvariantChecks.end())
					{
						const std::string hiddenInvariantName = invariantName->second;
						generator.CurrentChunk().Write(OpCode::OP_DUP);
						const uint16_t fnIndex = generator.CurrentChunk().AddConstant(
							std::make_shared<const std::string>(hiddenInvariantName));
						generator.CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
						generator.CurrentChunk().WriteOperand(OpCode::OP_GET_GLOBAL, fnIndex);
						generator.CurrentChunk().Write(OpCode::OP_SWAP);
						generator.CurrentChunk().Write(OpCode::OP_CALL);
						generator.CurrentChunk().code.push_back(1);
						generator.CurrentChunk().Write(OpCode::OP_POP);
					}
					return true;
				}

				if (const auto it = generator.m_metadata.enumVariants.find(qualifiedName);
					it != generator.m_metadata.enumVariants.end())
				{
					EmitCallArguments(node.args);
					generator.CurrentChunk().Write(OpCode::OP_BUILD_ENUM);
					generator.CurrentChunk().code.push_back(it->second.tag);
					generator.CurrentChunk().code.push_back(it->second.argCount);
					return true;
				}

				if (const auto actorName = generator.EnsureActorMetadata(qualifiedName))
				{
					const auto actorIt = generator.m_metadata.actorLayouts.find(*actorName);
					EmitCallArguments(node.args);
					if (const auto defaultsIt = generator.m_metadata.actorFieldDefaults.find(*actorName);
						defaultsIt != generator.m_metadata.actorFieldDefaults.end())
					{
						for (size_t i = node.args.size(); i < defaultsIt->second.size(); ++i)
						{
							if (defaultsIt->second[i])
							{
								defaultsIt->second[i]->Accept(generator);
							}
							else
							{
								generator.CurrentChunk().WriteConstant(std::monostate{});
							}
						}
					}
					const uint16_t blueprintIndex = generator.CurrentChunk().AddConstant(
						std::make_shared<const std::string>(*actorName));
					generator.CurrentChunk().Write(OpCode::OP_BUILD_ACTOR);
					generator.CurrentChunk().WriteOperand(OpCode::OP_BUILD_ACTOR, blueprintIndex);
					generator.CurrentChunk().code.push_back(static_cast<uint8_t>(actorIt->second.size()));
					return true;
				}
			}
		}
	}

	return false;
}

bool BytecodeGenerator::AccessEmitter::TryEmitMemberCall(const CallNode& node) const
{
	const auto* memberAccess = dynamic_cast<const MemberAccessNode*>(node.callee.get());
	if (!memberAccess)
	{
		return false;
	}

	if (const auto hiddenMethod = generator.ResolveStructMethod(memberAccess->object.get(),
			memberAccess->member))
	{
		const uint16_t fnIndex = generator.CurrentChunk().AddConstant(
			std::make_shared<const std::string>(*hiddenMethod));
		generator.CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
		generator.CurrentChunk().WriteOperand(OpCode::OP_GET_GLOBAL, fnIndex);
		memberAccess->object->Accept(generator);
		const auto signatureIt = generator.m_metadata.functionSignatures.find(*hiddenMethod);
		const auto contextParams = signatureIt != generator.m_metadata.functionSignatures.end()
			? signatureIt->second.contextParams
			: std::vector<std::string>{};
		EmitContextArguments(contextParams);
		const auto refParams = generator.m_metadata.functionSignatures.contains(*hiddenMethod)
			? generator.m_metadata.functionSignatures.at(*hiddenMethod).refParams
			: std::vector<bool>{};
		EmitCallArguments(node.args, refParams, 1 + contextParams.size());
		if (const auto it = generator.m_metadata.functionSignatures.find(*hiddenMethod);
			it != generator.m_metadata.functionSignatures.end())
		{
			const size_t hiddenPrefixCount = 1 + contextParams.size();
			const size_t fixedSupplied = std::min(hiddenPrefixCount + node.args.size(), it->second.defaultArgs.size());
			for (size_t i = fixedSupplied; i < it->second.defaultArgs.size(); ++i)
			{
				if (!it->second.defaultArgs[i])
				{
					throw std::runtime_error("Missing default argument metadata for method: " + *hiddenMethod);
				}
				it->second.defaultArgs[i]->Accept(generator);
			}
		}
		generator.CurrentChunk().Write(OpCode::OP_CALL);
		const auto it = generator.m_metadata.functionSignatures.find(*hiddenMethod);
		const size_t totalArgs = it != generator.m_metadata.functionSignatures.end()
			? (it->second.variadic
					  ? node.args.size() + 1 + contextParams.size()
					  : it->second.defaultArgs.size() + contextParams.size())
			: node.args.size() + 1 + contextParams.size();
		generator.CurrentChunk().code.push_back(static_cast<uint8_t>(totalArgs));
		return true;
	}

	if (generator.IsFunctionBackedInterfaceMethod(memberAccess->object.get(),
			memberAccess->member))
	{
		memberAccess->object->Accept(generator);
		EmitCallArguments(node.args);
		generator.CurrentChunk().Write(OpCode::OP_CALL);
		generator.CurrentChunk().code.push_back(static_cast<uint8_t>(node.args.size()));
		return true;
	}

	if (generator.ResolveActorMethod(memberAccess->object.get(), memberAccess->member))
	{
		memberAccess->object->Accept(generator);
		const auto actorType = generator.InferActorTypeName(memberAccess->object.get());
		std::vector<std::string> contextParams;
		if (actorType)
		{
			const auto methodIt = generator.m_metadata.actorMethodNames.find(*actorType);
			if (methodIt != generator.m_metadata.actorMethodNames.end())
			{
				const auto hiddenIt = methodIt->second.find(memberAccess->member);
				if (hiddenIt != methodIt->second.end())
				{
					const auto sigIt = generator.m_metadata.functionSignatures.find(hiddenIt->second);
					if (sigIt != generator.m_metadata.functionSignatures.end())
					{
						contextParams = sigIt->second.contextParams;
					}
				}
			}
		}
		EmitContextArguments(contextParams);
		EmitCallArguments(node.args, {}, contextParams.size());
		generator.CurrentChunk().Write(
			generator.IsActorQueryMethod(memberAccess->object.get(), memberAccess->member)
				? OpCode::OP_ACTOR_QUERY
				: OpCode::OP_ACTOR_SEND);
		const uint16_t methodNameIndex = generator.CurrentChunk().AddConstant(
			std::make_shared<const std::string>(memberAccess->member));
		generator.CurrentChunk().WriteOperand(
			generator.IsActorQueryMethod(memberAccess->object.get(), memberAccess->member)
				? OpCode::OP_ACTOR_QUERY
				: OpCode::OP_ACTOR_SEND,
			methodNameIndex);
		generator.CurrentChunk().code.push_back(static_cast<uint8_t>(node.args.size() + contextParams.size()));
		return true;
	}

	return false;
}

void BytecodeGenerator::AccessEmitter::EmitCall(const CallNode& node) const
{
	auto emitMissingDefaults = [&](const std::string& functionName, const size_t suppliedCount, const size_t refOffset = 0) {
		if (const auto it = generator.m_metadata.functionSignatures.find(functionName);
			it != generator.m_metadata.functionSignatures.end())
		{
			if (suppliedCount < it->second.requiredArity
				|| (!it->second.variadic && suppliedCount > it->second.defaultArgs.size()))
			{
				throw std::runtime_error("Call arity does not match function signature: " + functionName);
			}
			const size_t fixedSupplied = std::min(suppliedCount, it->second.defaultArgs.size());
			for (size_t i = fixedSupplied; i < it->second.defaultArgs.size(); ++i)
			{
				const bool byRef = i + refOffset < it->second.refParams.size() && it->second.refParams[i + refOffset];
				if (byRef)
				{
					throw std::runtime_error("ref parameters cannot have default values");
				}
				if (!it->second.defaultArgs[i])
				{
					throw std::runtime_error("Missing default argument metadata for function: " + functionName);
				}
				it->second.defaultArgs[i]->Accept(generator);
			}
		}
	};

	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node.callee.get());
		identifier && identifier->name == "resume")
	{
		if (!generator.CurrentContext().isEffectHandler)
		{
			throw std::runtime_error("resume(value) can only be emitted inside an effect handler");
		}
		if (node.args.size() != 1)
		{
			throw std::runtime_error("resume(value) expects exactly one argument");
		}
		node.args[0]->Accept(generator);
		generator.CurrentChunk().Write(OpCode::OP_RETURN);
		return;
	}

	if (const auto effectOp = generator.ResolveEffectOperation(node.callee.get()))
	{
		EmitCallArguments(node.args);
		const uint16_t effectIndex = generator.CurrentChunk().AddConstant(
			std::make_shared<const std::string>(*effectOp));
		generator.CurrentChunk().Write(OpCode::OP_EFFECT_INVOKE);
		generator.CurrentChunk().WriteOperand(OpCode::OP_EFFECT_INVOKE, effectIndex);
		generator.CurrentChunk().code.push_back(static_cast<uint8_t>(node.args.size()));
		return;
	}

	if (TryEmitDirectTypeConstructorCall(node) || TryEmitMemberCall(node))
	{
		return;
	}

	node.callee->Accept(generator);
	const auto refParams = generator.ResolveFunctionRefParams(node.callee.get());
	std::vector<std::string> contextParams;
	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node.callee.get()))
	{
		if (const auto it = generator.m_metadata.functionSignatures.find(generator.QualifyName(identifier->name));
			it != generator.m_metadata.functionSignatures.end())
		{
			contextParams = it->second.contextParams;
		}
	}
	else if (const auto* memberAccess = dynamic_cast<const MemberAccessNode*>(node.callee.get()))
	{
		if (const auto* moduleIdentifier = dynamic_cast<const IdentifierNode*>(memberAccess->object.get()))
		{
			if (const auto aliasIt = generator.m_metadata.importAliases.find(moduleIdentifier->name);
				aliasIt != generator.m_metadata.importAliases.end())
			{
				if (const auto it = generator.m_metadata.functionSignatures.find(aliasIt->second + "." + memberAccess->member);
					it != generator.m_metadata.functionSignatures.end())
				{
					contextParams = it->second.contextParams;
				}
			}
		}
	}
	EmitContextArguments(contextParams);
	EmitCallArguments(node.args, refParams, contextParams.size());
	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node.callee.get()))
	{
		emitMissingDefaults(generator.QualifyName(identifier->name), node.args.size());
	}
	else if (const auto* memberAccess = dynamic_cast<const MemberAccessNode*>(node.callee.get()))
	{
		if (const auto* moduleIdentifier = dynamic_cast<const IdentifierNode*>(memberAccess->object.get()))
		{
			if (const auto aliasIt = generator.m_metadata.importAliases.find(moduleIdentifier->name);
				aliasIt != generator.m_metadata.importAliases.end())
			{
				emitMissingDefaults(aliasIt->second + "." + memberAccess->member, node.args.size());
			}
		}
	}
	generator.CurrentChunk().Write(OpCode::OP_CALL);
	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node.callee.get()))
	{
		const auto it = generator.m_metadata.functionSignatures.find(generator.QualifyName(identifier->name));
		generator.CurrentChunk().code.push_back(static_cast<uint8_t>(
			(it != generator.m_metadata.functionSignatures.end()
					? (it->second.variadic
							  ? node.args.size() + it->second.contextParams.size()
							  : it->second.defaultArgs.size() + it->second.contextParams.size())
					: node.args.size())));
	}
	else if (const auto* memberAccess = dynamic_cast<const MemberAccessNode*>(node.callee.get()))
	{
		if (const auto* moduleIdentifier = dynamic_cast<const IdentifierNode*>(memberAccess->object.get()))
		{
			if (const auto aliasIt = generator.m_metadata.importAliases.find(moduleIdentifier->name);
				aliasIt != generator.m_metadata.importAliases.end())
			{
				const auto it = generator.m_metadata.functionSignatures.find(aliasIt->second + "." + memberAccess->member);
				generator.CurrentChunk().code.push_back(static_cast<uint8_t>(
					(it != generator.m_metadata.functionSignatures.end()
							? (it->second.variadic
									  ? node.args.size() + it->second.contextParams.size()
									  : it->second.defaultArgs.size() + it->second.contextParams.size())
							: node.args.size())));
				return;
			}
		}
		generator.CurrentChunk().code.push_back(static_cast<uint8_t>(node.args.size()));
	}
	else
	{
		generator.CurrentChunk().code.push_back(static_cast<uint8_t>(node.args.size()));
	}
}

void BytecodeGenerator::AccessEmitter::EmitMemberAccess(MemberAccessNode& node) const
{
	if (generator.ResolveStructMethod(node.object.get(), node.member))
	{
		throw std::runtime_error("Struct methods can only be used in call position");
	}

	if (generator.ResolveActorMethod(node.object.get(), node.member))
	{
		throw std::runtime_error("Actor methods can only be used in call position");
	}

	if (generator.IsFunctionBackedInterfaceMethod(node.object.get(), node.member))
	{
		node.object->Accept(generator);
		return;
	}

	if (generator.IsEnumTagAccess(node.object.get(), node.member))
	{
		node.object->Accept(generator);
		generator.CurrentChunk().Write(OpCode::OP_GET_ENUM_TAG);
		return;
	}

	if (const auto fieldIndex = generator.ResolveStructFieldIndex(
			node.object.get(),
			node.member))
	{
		node.object->Accept(generator);
		generator.CurrentChunk().Write(OpCode::OP_MEMBER_GET);
		generator.CurrentChunk().code.push_back(*fieldIndex);
		return;
	}

	node.object->Accept(generator);
	const uint16_t memberIndex = generator.CurrentChunk().AddConstant(
		std::make_shared<const std::string>(node.member));
	generator.CurrentChunk().Write(OpCode::OP_GET_MODULE_MEMBER);
	generator.CurrentChunk().WriteOperand(OpCode::OP_GET_MODULE_MEMBER, memberIndex);
}

void BytecodeGenerator::AccessEmitter::EmitAssignment(const AssignmentNode& node) const
{
	if (node.object)
	{
		const auto fieldIndex = generator.ResolveStructFieldIndex(node.object.get(), node.member);
		if (!fieldIndex)
		{
			throw std::runtime_error("Unknown struct field for assignment: " + node.member);
		}

		node.object->Accept(generator);
		node.value->Accept(generator);
		generator.CurrentChunk().Write(OpCode::OP_MEMBER_SET);
		generator.CurrentChunk().code.push_back(*fieldIndex);
		return;
	}

	if (!node.name.empty() && (generator.CurrentContext().methodSelfStructType || generator.CurrentContext().actorSelfType))
	{
		if (const auto selfFieldIndex = generator.ResolveSelfFieldIndex(node.name))
		{
			generator.CurrentChunk().Write(OpCode::OP_GET_LOCAL);
			generator.CurrentChunk().code.push_back(0);
			node.value->Accept(generator);
			generator.CurrentChunk().Write(OpCode::OP_MEMBER_SET);
			generator.CurrentChunk().code.push_back(*selfFieldIndex);
			return;
		}
	}

	if (node.dereferenceTarget)
	{
		node.dereferenceTarget->Accept(generator);
		node.value->Accept(generator);
		generator.CurrentChunk().Write(OpCode::OP_DEREF_SET);
		return;
	}

	if (node.index)
	{
		generator.EmitGetVariable(node.name);
		node.index->Accept(generator);
		node.value->Accept(generator);
		generator.CurrentChunk().Write(OpCode::OP_INDEX_SET);
		return;
	}

	node.value->Accept(generator);
	generator.EmitSetVariable(node.name);

	if (const auto existingBinding = generator.ResolveInterfaceBinding(node.name))
	{
		if (const auto interfaceBinding = generator.InferInterfaceBinding(node.value.get(), existingBinding->interfaceName))
		{
			for (auto scopeIt = generator.CurrentContext().interfaceVarScopes.rbegin();
				scopeIt != generator.CurrentContext().interfaceVarScopes.rend();
				++scopeIt)
			{
				if (const auto it = scopeIt->find(node.name); it != scopeIt->end())
				{
					it->second = *interfaceBinding;
					break;
				}
			}
		}
	}
}
