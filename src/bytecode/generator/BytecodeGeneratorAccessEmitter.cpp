#include "../BytecodeGenerator.h"

#include <memory>
#include <stdexcept>

using namespace VM::Core;

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

bool BytecodeGenerator::AccessEmitter::TryEmitDirectTypeConstructorCall(const CallNode& node) const
{
	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node.callee.get()))
	{
		const std::string structName = generator.QualifyName(identifier->name);
		if (const auto it = generator.m_metadata.structLayouts.find(structName);
			it != generator.m_metadata.structLayouts.end())
		{
			EmitCallArguments(node.args);
			generator.CurrentChunk().Write(OpCode::OP_BUILD_STRUCT);
			generator.CurrentChunk().code.push_back(static_cast<uint8_t>(it->second.size()));
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

		if (const auto it = generator.m_metadata.actorLayouts.find(structName);
			it != generator.m_metadata.actorLayouts.end())
		{
			EmitCallArguments(node.args);
			const uint8_t blueprintIndex = generator.CurrentChunk().AddConstant(
				std::make_shared<const std::string>(structName));
			generator.CurrentChunk().Write(OpCode::OP_BUILD_ACTOR);
			generator.CurrentChunk().code.push_back(blueprintIndex);
			generator.CurrentChunk().code.push_back(static_cast<uint8_t>(it->second.size()));
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
				if (const auto it = generator.m_metadata.structLayouts.find(qualifiedName);
					it != generator.m_metadata.structLayouts.end())
				{
					EmitCallArguments(node.args);
					generator.CurrentChunk().Write(OpCode::OP_BUILD_STRUCT);
					generator.CurrentChunk().code.push_back(static_cast<uint8_t>(it->second.size()));
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

				if (const auto it = generator.m_metadata.actorLayouts.find(qualifiedName);
					it != generator.m_metadata.actorLayouts.end())
				{
					EmitCallArguments(node.args);
					const uint8_t blueprintIndex = generator.CurrentChunk().AddConstant(
						std::make_shared<const std::string>(qualifiedName));
					generator.CurrentChunk().Write(OpCode::OP_BUILD_ACTOR);
					generator.CurrentChunk().code.push_back(blueprintIndex);
					generator.CurrentChunk().code.push_back(static_cast<uint8_t>(it->second.size()));
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
		const uint8_t fnIndex = generator.CurrentChunk().AddConstant(
			std::make_shared<const std::string>(*hiddenMethod));
		generator.CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
		generator.CurrentChunk().code.push_back(fnIndex);
		memberAccess->object->Accept(generator);
		const auto refParams = generator.m_metadata.functionSignatures.contains(*hiddenMethod)
			? generator.m_metadata.functionSignatures.at(*hiddenMethod).refParams
			: std::vector<bool>{};
		EmitCallArguments(node.args, refParams, 1);
		generator.CurrentChunk().Write(OpCode::OP_CALL);
		generator.CurrentChunk().code.push_back(static_cast<uint8_t>(node.args.size() + 1));
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
		EmitCallArguments(node.args);
		generator.CurrentChunk().Write(
			generator.IsActorQueryMethod(memberAccess->object.get(), memberAccess->member)
				? OpCode::OP_ACTOR_QUERY
				: OpCode::OP_ACTOR_SEND);
		const uint8_t methodNameIndex = generator.CurrentChunk().AddConstant(
			std::make_shared<const std::string>(memberAccess->member));
		generator.CurrentChunk().code.push_back(methodNameIndex);
		generator.CurrentChunk().code.push_back(static_cast<uint8_t>(node.args.size()));
		return true;
	}

	return false;
}

void BytecodeGenerator::AccessEmitter::EmitCall(const CallNode& node) const
{
	if (const auto effectOp = generator.ResolveEffectOperation(node.callee.get()))
	{
		EmitCallArguments(node.args);
		const uint8_t effectIndex = generator.CurrentChunk().AddConstant(
			std::make_shared<const std::string>(*effectOp));
		generator.CurrentChunk().Write(OpCode::OP_EFFECT_INVOKE);
		generator.CurrentChunk().code.push_back(effectIndex);
		generator.CurrentChunk().code.push_back(static_cast<uint8_t>(node.args.size()));
		return;
	}

	if (TryEmitDirectTypeConstructorCall(node) || TryEmitMemberCall(node))
	{
		return;
	}

	node.callee->Accept(generator);
	EmitCallArguments(node.args, generator.ResolveFunctionRefParams(node.callee.get()));
	generator.CurrentChunk().Write(OpCode::OP_CALL);
	generator.CurrentChunk().code.push_back(static_cast<uint8_t>(node.args.size()));
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
	const uint8_t memberIndex = generator.CurrentChunk().AddConstant(
		std::make_shared<const std::string>(node.member));
	generator.CurrentChunk().Write(OpCode::OP_GET_MODULE_MEMBER);
	generator.CurrentChunk().code.push_back(memberIndex);
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
