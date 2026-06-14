#include "../../ast/sematic/SemanticAnalyzer.h"
#include "../../vm/core/values/ValueHelper.h"
#include "../BytecodeGenerator.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

using namespace VM::Core;

#include "../BytecodeGeneratorSupport.h"

using BytecodeGeneratorDetail::ScopeExit;
using BytecodeGeneratorDetail::ShouldPopStatementResult;

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

std::string EffectNameFromKey(const std::string& effectKey)
{
	if (const auto pos = effectKey.rfind('.'); pos != std::string::npos)
	{
		return effectKey.substr(0, pos);
	}
	return effectKey;
}
}

class ComptimeEvaluator
{
public:
	struct Result
	{
		Value value{ std::monostate{} };
		bool didReturn = false;
	};

	explicit ComptimeEvaluator(BytecodeGenerator& generator)
		: m_generator(generator)
	{
		m_structBindings.emplace_back();
	}

	Result Eval(ASTNode* node)
	{
		if (!node)
		{
			return {};
		}
		if (const auto* integer = dynamic_cast<IntegerLiteralNode*>(node))
		{
			return { integer->value, false };
		}
		if (const auto* number = dynamic_cast<FloatLiteralNode*>(node))
		{
			return { number->value, false };
		}
		if (const auto* text = dynamic_cast<StringLiteralNode*>(node))
		{
			return { std::make_shared<const std::string>(text->value), false };
		}
		if (const auto* leaf = dynamic_cast<LeafNode*>(node))
		{
			if (leaf->type == "true")
			{
				return { true, false };
			}
			if (leaf->type == "false")
			{
				return { false, false };
			}
			return {};
		}
		if (const auto* identifier = dynamic_cast<IdentifierNode*>(node))
		{
			return { Resolve(identifier->name), false };
		}
		if (const auto* unary = dynamic_cast<UnaryExprNode*>(node))
		{
			const auto operand = Eval(unary->operand.get()).value;
			if (unary->op == "-")
			{
				return { ValueHelper::Negate(operand), false };
			}
			if (unary->op == "!" || unary->op == "not")
			{
				return { ValueHelper::PerformUnaryLogic(operand), false };
			}
			return { operand, false };
		}
		if (const auto* binary = dynamic_cast<BinaryExprNode*>(node))
		{
			const auto lhs = Eval(binary->left.get()).value;
			const auto rhs = Eval(binary->right.get()).value;
			if (binary->op == "+")
			{
				return { ValueHelper::Add(lhs, rhs), false };
			}
			if (binary->op == "-")
			{
				return { ValueHelper::Subtract(lhs, rhs), false };
			}
			if (binary->op == "*")
			{
				return { ValueHelper::Multiply(lhs, rhs), false };
			}
			if (binary->op == "/")
			{
				return { ValueHelper::Divide(lhs, rhs), false };
			}
			if (binary->op == "div")
			{
				return { ValueHelper::DivideInt(lhs, rhs), false };
			}
			if (binary->op == "mod")
			{
				return { ValueHelper::Modulo(lhs, rhs), false };
			}
			if (binary->op == "==")
			{
				return { ValueHelper::Equal(lhs, rhs), false };
			}
			if (binary->op == "!=")
			{
				return { !ValueHelper::Equal(lhs, rhs), false };
			}
			if (binary->op == ">")
			{
				return { ValueHelper::Greater(lhs, rhs), false };
			}
			if (binary->op == "<")
			{
				return { ValueHelper::Less(lhs, rhs), false };
			}
			if (binary->op == ">=")
			{
				return { !ValueHelper::As<bool>(ValueHelper::Less(lhs, rhs)), false };
			}
			if (binary->op == "<=")
			{
				return { !ValueHelper::As<bool>(ValueHelper::Greater(lhs, rhs)), false };
			}
			if (binary->op == "and")
			{
				return { ValueHelper::PerformBinaryLogic(lhs, rhs, std::logical_and<bool>{}), false };
			}
			if (binary->op == "or")
			{
				return { ValueHelper::PerformBinaryLogic(lhs, rhs, std::logical_or<bool>{}), false };
			}
			throw std::runtime_error("Unsupported comptime binary operator: " + binary->op);
		}
		if (const auto* array = dynamic_cast<ArrayLiteralNode*>(node))
		{
			auto result = std::make_shared<Array>();
			for (const auto& element : array->elements)
			{
				result->elements.push_back(Eval(element.get()).value);
			}
			return { result, false };
		}
		if (const auto* varDecl = dynamic_cast<VarDeclNode*>(node))
		{
			const Value value = varDecl->initializer ? Eval(varDecl->initializer.get()).value : Value(std::monostate{});
			AssignLocal(varDecl->name, value);
			if (const auto structType = m_generator.InferStructTypeName(varDecl->initializer.get()))
			{
				m_structBindings.back()[varDecl->name] = *structType;
			}
			return { value, false };
		}
		if (const auto* returnNode = dynamic_cast<ReturnNode*>(node))
		{
			return { returnNode->value ? Eval(returnNode->value.get()).value : Value(std::monostate{}), true };
		}
		if (const auto* block = dynamic_cast<BlockNode*>(node))
		{
			PushScope();
			ScopeExit scope([this]() { PopScope(); });
			Result last;
			for (const auto& stmt : block->statements)
			{
				last = Eval(stmt.get());
				if (last.didReturn)
				{
					return last;
				}
			}
			return last;
		}
		if (const auto* ifStmt = dynamic_cast<IfStatementNode*>(node))
		{
			if (ValueHelper::As<bool>(Eval(ifStmt->condition.get()).value))
			{
				return Eval(ifStmt->thenBlock.get());
			}
			return ifStmt->elseBlock ? Eval(ifStmt->elseBlock.get()) : Result{};
		}
		if (const auto* whileStmt = dynamic_cast<WhileStatementNode*>(node))
		{
			size_t iterations = 0;
			while (ValueHelper::As<bool>(Eval(whileStmt->condition.get()).value))
			{
				if (++iterations > 10000)
				{
					throw std::runtime_error("comptime loop exceeded iteration limit");
				}
				const auto bodyResult = Eval(whileStmt->body.get());
				if (bodyResult.didReturn)
				{
					return bodyResult;
				}
			}
			return {};
		}
		if (const auto* assignment = dynamic_cast<AssignmentNode*>(node))
		{
			const auto value = Eval(assignment->value.get()).value;
			if (!assignment->name.empty())
			{
				Assign(assignment->name, value);
				return { value, false };
			}
			throw std::runtime_error("Unsupported comptime assignment target");
		}
		if (const auto* call = dynamic_cast<CallNode*>(node))
		{
			return { EvalCall(*call), false };
		}
		if (const auto* member = dynamic_cast<MemberAccessNode*>(node))
		{
			return { EvalMemberAccess(*member), false };
		}
		if (const auto* index = dynamic_cast<IndexNode*>(node))
		{
			return { EvalIndex(*index), false };
		}
		if (const auto* comptimeNode = dynamic_cast<ComptimeNode*>(node))
		{
			return Eval(comptimeNode->body.get());
		}
		throw std::runtime_error("Unsupported node in comptime evaluation");
	}

private:
	Value EvalCall(const CallNode& call)
	{
		auto resolveCallTarget = [&]() -> std::pair<std::string, std::string> {
			if (const auto* identifier = dynamic_cast<const IdentifierNode*>(call.callee.get()))
			{
				return { identifier->name, m_generator.QualifyName(identifier->name) };
			}
			if (const auto* memberAccess = dynamic_cast<const MemberAccessNode*>(call.callee.get()))
			{
				if (const auto* moduleIdentifier = dynamic_cast<const IdentifierNode*>(memberAccess->object.get()))
				{
					if (const auto aliasIt = m_generator.m_metadata.importAliases.find(moduleIdentifier->name);
						aliasIt != m_generator.m_metadata.importAliases.end())
					{
						return {
							memberAccess->member,
							aliasIt->second + "." + memberAccess->member
						};
					}
				}
			}
			throw std::runtime_error("Unsupported comptime call target");
		};

		const auto [displayName, qualifiedName] = resolveCallTarget();
		const auto it = m_generator.m_comptimeFunctions.find(qualifiedName);
		const auto plainIt = m_generator.m_comptimeFunctions.find(displayName);
		const FunctionDeclNode* function = it != m_generator.m_comptimeFunctions.end()
			? it->second
			: (plainIt != m_generator.m_comptimeFunctions.end() ? plainIt->second : nullptr);
		if (function)
		{
			PushScope();
			ScopeExit scope([this]() { PopScope(); });
			for (const auto& context : function->contextRequirements)
			{
				AssignLocal(context.name, Resolve(context.name));
			}
			size_t i = 0;
			for (; i < call.args.size(); ++i)
			{
				AssignLocal(function->params[i].name, Eval(call.args[i].get()).value);
			}
			for (; i < function->params.size(); ++i)
			{
				if (!function->params[i].defaultValue)
				{
					throw std::runtime_error("Missing comptime argument");
				}
				AssignLocal(function->params[i].name, Eval(function->params[i].defaultValue.get()).value);
			}
			return Eval(function->body.get()).value;
		}

		if (m_generator.m_metadata.structLayouts.contains(qualifiedName)
			|| m_generator.m_metadata.structLayouts.contains(GenericBaseName(qualifiedName)))
		{
			const auto layoutName = m_generator.m_metadata.structLayouts.contains(qualifiedName)
				? qualifiedName
				: GenericBaseName(qualifiedName);
			auto instance = std::make_shared<Instance>();
			instance->fields.resize(m_generator.m_metadata.structLayouts.at(layoutName).size());
			for (size_t i = 0; i < call.args.size(); ++i)
			{
				instance->fields[i] = Eval(call.args[i].get()).value;
			}
			return instance;
		}

		if (const auto enumIt = m_generator.m_metadata.enumVariants.find(qualifiedName);
			enumIt != m_generator.m_metadata.enumVariants.end())
		{
			auto value = std::make_shared<EnumVariant>();
			value->tag = enumIt->second.tag;
			for (const auto& arg : call.args)
			{
				value->args.push_back(Eval(arg.get()).value);
			}
			return value;
		}

		throw std::runtime_error("Unsupported comptime call target: " + displayName);
	}

	Value EvalMemberAccess(const MemberAccessNode& member)
	{
		const Value object = Eval(member.object.get()).value;
		if (std::holds_alternative<InstancePtr>(object))
		{
			const auto instance = std::get<InstancePtr>(object);
			const auto fieldIndex = ResolveStructFieldIndex(member.object.get(), member.member);
			if (!fieldIndex || !instance || *fieldIndex >= instance->fields.size())
			{
				throw std::runtime_error("Unsupported comptime member access: " + member.member);
			}
			return instance->fields[*fieldIndex];
		}
		if (std::holds_alternative<EnumPtr>(object))
		{
			if (member.member == "tag")
			{
				return static_cast<long long>(std::get<EnumPtr>(object)->tag);
			}
		}
		throw std::runtime_error("Unsupported comptime member access: " + member.member);
	}

	Value EvalIndex(const IndexNode& index)
	{
		const Value container = Eval(index.container.get()).value;
		const Value rawIndex = Eval(index.index.get()).value;
		const auto elementIndex = ValueHelper::As<int64_t>(rawIndex);
		if (elementIndex < 0)
		{
			throw std::runtime_error("Unsupported comptime negative index access");
		}

		if (std::holds_alternative<ArrayPtr>(container))
		{
			const auto array = std::get<ArrayPtr>(container);
			if (!array || static_cast<size_t>(elementIndex) >= array->elements.size())
			{
				throw std::runtime_error("Unsupported comptime array index access");
			}
			return array->elements[static_cast<size_t>(elementIndex)];
		}
		if (std::holds_alternative<EnumPtr>(container))
		{
			const auto enumValue = std::get<EnumPtr>(container);
			if (!enumValue || static_cast<size_t>(elementIndex) >= enumValue->args.size())
			{
				throw std::runtime_error("Unsupported comptime enum index access");
			}
			return enumValue->args[static_cast<size_t>(elementIndex)];
		}
		throw std::runtime_error("Unsupported comptime index access");
	}

	Value Resolve(const std::string& name) const
	{
		for (auto it = m_generator.m_comptimeBindings.rbegin(); it != m_generator.m_comptimeBindings.rend(); ++it)
		{
			if (const auto bindingIt = it->find(name); bindingIt != it->end())
			{
				return bindingIt->second;
			}
		}
		throw std::runtime_error("Unknown comptime binding: " + name);
	}

	void Assign(const std::string& name, const Value& value)
	{
		for (auto it = m_generator.m_comptimeBindings.rbegin(); it != m_generator.m_comptimeBindings.rend(); ++it)
		{
			if (const auto bindingIt = it->find(name); bindingIt != it->end())
			{
				bindingIt->second = value;
				return;
			}
		}
		AssignLocal(name, value);
	}

	void AssignLocal(const std::string& name, const Value& value)
	{
		if (m_generator.m_comptimeBindings.empty())
		{
			m_generator.m_comptimeBindings.emplace_back();
		}
		m_generator.m_comptimeBindings.back()[name] = value;
	}

	void PushScope()
	{
		m_generator.m_comptimeBindings.emplace_back();
		m_structBindings.emplace_back();
	}

	void PopScope()
	{
		m_generator.m_comptimeBindings.pop_back();
		m_structBindings.pop_back();
	}

	std::optional<uint8_t> ResolveStructFieldIndex(const ASTNode* object, const std::string& member) const
	{
		if (const auto* identifier = dynamic_cast<const IdentifierNode*>(object))
		{
			for (auto it = m_structBindings.rbegin(); it != m_structBindings.rend(); ++it)
			{
				if (const auto bindingIt = it->find(identifier->name); bindingIt != it->end())
				{
					const auto layoutIt = m_generator.m_metadata.structLayouts.find(bindingIt->second);
					if (layoutIt == m_generator.m_metadata.structLayouts.end())
					{
						return std::nullopt;
					}
					for (size_t i = 0; i < layoutIt->second.size(); ++i)
					{
						if (layoutIt->second[i] == member)
						{
							return static_cast<uint8_t>(i);
						}
					}
				}
			}
		}
		return m_generator.ResolveStructFieldIndex(object, member);
	}

BytecodeGenerator& m_generator;
	std::vector<std::unordered_map<std::string, std::string>> m_structBindings;
};

void BytecodeGenerator::Visit(BlockNode& node)
{
	uint8_t varsInBlock = 0;
	PushContextScope(CurrentContext());
	ScopeExit scopeExit([this, &varsInBlock]() {
		for (uint8_t i = 0; i < varsInBlock; ++i)
		{
			CurrentChunk().Write(OpCode::OP_POP);
		}
		PopContextScope(CurrentContext());
	});

	for (const auto& stmt : node.statements)
	{
		if (!stmt)
		{
			continue;
		}

		if (dynamic_cast<VarDeclNode*>(stmt.get()))
		{
			++varsInBlock;
		}

		stmt->Accept(*this);

		if (ShouldPopStatementResult(stmt.get()))
		{
			CurrentChunk().Write(OpCode::OP_POP);
		}
	}
}

void BytecodeGenerator::Visit(ExportDeclNode& node)
{
	if (node.declaration)
	{
		node.declaration->Accept(*this);
	}
}

void BytecodeGenerator::Visit(IfStatementNode& node)
{
	node.condition->Accept(*this);
	const size_t elseJump = EmitJump(OpCode::OP_JUMP_IF_FALSE);

	node.thenBlock->Accept(*this);

	if (node.elseBlock)
	{
		const size_t endJump = EmitJump(OpCode::OP_JUMP);
		PatchJump(elseJump);
		node.elseBlock->Accept(*this);
		PatchJump(endJump);
	}
	else
	{
		PatchJump(elseJump);
	}
}

void BytecodeGenerator::Visit(WhileStatementNode& node)
{
	const size_t loopStart = CurrentChunk().code.size();
	node.condition->Accept(*this);
	const size_t exitJump = EmitJump(OpCode::OP_JUMP_IF_FALSE);

	node.body->Accept(*this);

	const auto offset = static_cast<uint16_t>(CurrentChunk().code.size() - loopStart + 3);
	CurrentChunk().Write(OpCode::OP_LOOP);
	CurrentChunk().code.push_back((offset >> 8) & 0xff);
	CurrentChunk().code.push_back(offset & 0xff);

	PatchJump(exitJump);
}

void BytecodeGenerator::Visit(FunctionDeclNode& node)
{
	if (node.isComptime)
	{
		m_comptimeFunctions[QualifyName(node.name)] = &node;
		m_comptimeFunctions[node.name] = &node;
		return;
	}
	const std::string exportedName = QualifyName(node.name);
	RegisterFunctionSignature(exportedName, node.params, node.contextRequirements, node.returnType);
	std::vector<ContractNode*> contracts;
	contracts.reserve(node.contracts.size());
	for (const auto& contract : node.contracts)
	{
		contracts.push_back(contract.get());
	}
	CompileFunctionBody(
		exportedName,
		node.params,
		node.contextRequirements,
		node.raisedEffects,
		contracts,
		node.body.get(),
		[this, &exportedName]() {
			const uint16_t nameIndex = CurrentChunk().AddConstant(
				std::make_shared<const std::string>(exportedName));
			CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
			CurrentChunk().WriteOperand(OpCode::OP_DEFINE_GLOBAL, nameIndex);
		});
}

void BytecodeGenerator::Visit(FunctionExprNode& node)
{
	std::string name = node.name;
	if (name.empty())
	{
		name = "<lambda_" + std::to_string(++m_lambdaCounter) + ">";
	}

	CompileFunctionBody(name, node.params, {}, node.raisedEffects, {}, node.body.get(), []() {});
}

void BytecodeGenerator::Visit(CallNode& node)
{
	AccessEmitter{ *this }.EmitCall(node);
}

void BytecodeGenerator::Visit(GoExprNode& node)
{
	CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
	const uint16_t goIndex = CurrentChunk().AddConstant(
		std::make_shared<const std::string>("__task_go"));
	CurrentChunk().WriteOperand(OpCode::OP_GET_GLOBAL, goIndex);

	auto* call = dynamic_cast<CallNode*>(node.call.get());
	if (!call)
	{
		throw std::runtime_error("go expects a function call");
	}

	call->callee->Accept(*this);
	for (const auto& arg : call->args)
	{
		arg->Accept(*this);
	}

	CurrentChunk().Write(OpCode::OP_CALL);
	CurrentChunk().code.push_back(static_cast<uint8_t>(call->args.size() + 1));
}

void BytecodeGenerator::Visit(AwaitExprNode& node)
{
	CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
	const uint16_t awaitIndex = CurrentChunk().AddConstant(
		std::make_shared<const std::string>("__task_await"));
	CurrentChunk().WriteOperand(OpCode::OP_GET_GLOBAL, awaitIndex);

	node.operand->Accept(*this);
	CurrentChunk().Write(OpCode::OP_CALL);
	CurrentChunk().code.push_back(1);
}

void BytecodeGenerator::Visit(MemberAccessNode& node)
{
	AccessEmitter{ *this }.EmitMemberAccess(node);
}

void BytecodeGenerator::Visit(ModuleDeclNode& node)
{
	m_currentModule = node.qualifiedName;
}

void BytecodeGenerator::Visit(ImportDeclNode& node)
{
	m_metadata.importAliases[node.alias.empty()
			? DefaultImportAlias(node.qualifiedName)
			: node.alias] = node.qualifiedName;

	auto module = std::make_shared<Module>();
	module->name = node.qualifiedName;
	CurrentChunk().WriteConstant(module);

	const uint8_t localIndex = CurrentContext().symbols.Define(
		node.alias.empty() ? DefaultImportAlias(node.qualifiedName) : node.alias);
	CurrentChunk().Write(OpCode::OP_SET_LOCAL);
	CurrentChunk().code.push_back(localIndex);
}

void BytecodeGenerator::Visit(ReturnNode& node)
{
	if (node.value)
	{
		node.value->Accept(*this);
	}
	else
	{
		CurrentChunk().WriteConstant(std::monostate{});
	}
	if (!m_activeFunctionContracts.empty() && !m_activeFunctionContracts.back().empty())
	{
		const auto& contracts = m_activeFunctionContracts.back();
		const auto resultSlot = m_activeContractResultSlots.empty() ? std::nullopt : m_activeContractResultSlots.back();
		if (resultSlot.has_value())
		{
			CurrentChunk().Write(OpCode::OP_DUP);
			CurrentChunk().Write(OpCode::OP_SET_LOCAL);
			CurrentChunk().code.push_back(*resultSlot);
			CurrentChunk().Write(OpCode::OP_POP);
		}
		for (const auto* contract : contracts)
		{
			if (!contract || contract->kind != ContractKind::Ensures || !contract->expression)
			{
				continue;
			}
			contract->expression->Accept(*this);
			EmitContractAssertion("Contract ensures failed");
		}
	}
	if (CurrentContext().methodSelfStructType)
	{
		const auto invariantIt = m_metadata.structInvariantChecks.find(*CurrentContext().methodSelfStructType);
		if (invariantIt != m_metadata.structInvariantChecks.end())
		{
			const uint16_t fnIndex = CurrentChunk().AddConstant(
				std::make_shared<const std::string>(invariantIt->second));
			CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
			CurrentChunk().WriteOperand(OpCode::OP_GET_GLOBAL, fnIndex);
			CurrentChunk().Write(OpCode::OP_GET_LOCAL);
			CurrentChunk().code.push_back(0);
			CurrentChunk().Write(OpCode::OP_CALL);
			CurrentChunk().code.push_back(1);
			CurrentChunk().Write(OpCode::OP_POP);
		}
	}
	CurrentChunk().Write(OpCode::OP_RETURN);
}

void BytecodeGenerator::Visit(PrintNode& node)
{
	node.value->Accept(*this);
	CurrentChunk().Write(OpCode::OP_PRINT);
}

void BytecodeGenerator::Visit(UnsafeNode& node)
{
	if (node.body)
	{
		node.body->Accept(*this);
	}
}

void BytecodeGenerator::Visit(TransactionNode& node)
{
	if (node.regions.empty())
	{
		throw std::runtime_error("transaction requires at least one region");
	}
	for (const auto& region : node.regions)
	{
		if (region.usesShared)
		{
			EmitGetVariable("__txn_mutex." + region.name);
		}
		else
		{
			EmitGetVariable(region.name);
		}
	}
	if (node.regions.size() > 1)
	{
		CurrentChunk().Write(OpCode::OP_BUILD_ARRAY);
		CurrentChunk().code.push_back(static_cast<uint8_t>(node.regions.size()));
	}
	CurrentChunk().Write(OpCode::OP_BEGIN_TXN);
	if (node.body)
	{
		node.body->Accept(*this);
	}
	CurrentChunk().Write(OpCode::OP_END_TXN);
}

void BytecodeGenerator::Visit(HandleNode& node)
{
	auto resolveHandlerEffectKey = [this](const std::string& operationName) {
		std::vector<std::string> candidates;
		auto collectMatches = [&](const auto& scopeStack) {
			for (auto activeIt = scopeStack.rbegin(); activeIt != scopeStack.rend(); ++activeIt)
			{
				for (const auto& effectName : *activeIt)
				{
					if (const auto operationsIt = m_metadata.effectOperations.find(effectName);
						operationsIt != m_metadata.effectOperations.end()
						&& operationsIt->second.contains(operationName))
					{
						candidates.push_back(effectName);
					}
				}
			}
		};

		collectMatches(m_activeRaisedEffects);
		collectMatches(m_activeHandledEffects);
		if (candidates.empty())
		{
			for (const auto& [effectName, operations] : m_metadata.effectOperations)
			{
				if (operations.contains(operationName))
				{
					candidates.push_back(effectName);
				}
			}
		}

		std::ranges::sort(candidates);
		candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
		if (candidates.size() == 1)
		{
			return candidates.front() + "." + operationName;
		}
		if (candidates.size() > 1)
		{
			throw std::runtime_error("Ambiguous handled effect operation for codegen: " + operationName);
		}
		return operationName;
	};

	for (const auto& handler : node.handlers)
	{
		std::string handlerName = "<handler_"
			+ handler.effectName
			+ "_"
			+ std::to_string(++m_lambdaCounter)
			+ ">";
		std::vector<Parameter> params = handler.params;
		CompileFunctionBody(handlerName, params, {}, {}, {}, handler.body.get(), []() {}, true);

		const std::string effectKey = handler.effectKey.empty()
			? resolveHandlerEffectKey(handler.effectName)
			: handler.effectKey;
		const uint16_t opIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(effectKey));
		CurrentChunk().Write(OpCode::OP_CONSTANT);
		CurrentChunk().WriteOperand(OpCode::OP_CONSTANT, opIndex);
	}

	CurrentChunk().Write(OpCode::OP_BUILD_HANDLER);
	CurrentChunk().code.push_back(static_cast<uint8_t>(node.handlers.size()));
	CurrentChunk().Write(OpCode::OP_PUSH_HANDLER);
	std::unordered_set<std::string> handledEffects;
	for (const auto& handler : node.handlers)
	{
		const std::string effectKey = handler.effectKey.empty()
			? resolveHandlerEffectKey(handler.effectName)
			: handler.effectKey;
		handledEffects.insert(EffectNameFromKey(effectKey));
	}
	m_activeHandledEffects.push_back(std::move(handledEffects));
	ScopeExit handledScope([this]() {
		m_activeHandledEffects.pop_back();
	});
	if (node.expression)
	{
		node.expression->Accept(*this);
	}
	else
	{
		CurrentChunk().WriteConstant(std::monostate{});
	}
	CurrentChunk().Write(OpCode::OP_POP_HANDLER);
}

void BytecodeGenerator::Visit(ArrayLiteralNode& node)
{
	for (const auto& el : node.elements)
	{
		el->Accept(*this);
	}
	CurrentChunk().Write(OpCode::OP_BUILD_ARRAY);
	CurrentChunk().code.push_back(static_cast<uint8_t>(node.elements.size()));
}

void BytecodeGenerator::Visit(MapLiteralNode& node)
{
	for (const auto& [key, value] : node.entries)
	{
		key->Accept(*this);
		value->Accept(*this);
	}
	CurrentChunk().Write(OpCode::OP_BUILD_MAP);
	CurrentChunk().code.push_back(static_cast<uint8_t>(node.entries.size()));
}

void BytecodeGenerator::Visit(IndexNode& node)
{
	if (InferEnumTypeName(node.container.get()))
	{
		const auto* literalIndex = dynamic_cast<const IntegerLiteralNode*>(node.index.get());
		if (!literalIndex || literalIndex->value < 0 || literalIndex->value > 255)
		{
			throw std::runtime_error("Enum argument access requires a non-negative integer literal index");
		}

		node.container->Accept(*this);
		CurrentChunk().Write(OpCode::OP_GET_ENUM_ARG);
		CurrentChunk().code.push_back(static_cast<uint8_t>(literalIndex->value));
		return;
	}

	node.container->Accept(*this);
	node.index->Accept(*this);
	CurrentChunk().Write(OpCode::OP_INDEX_GET);
}

void BytecodeGenerator::Visit(IterNode& node)
{
	PushContextScope(CurrentContext());
	ScopeExit scopeExit([this]() {
		CurrentChunk().Write(OpCode::OP_POP);
		CurrentChunk().Write(OpCode::OP_POP);
		PopContextScope(CurrentContext());
	});

	node.collection->Accept(*this);
	CurrentChunk().Write(OpCode::OP_MAKE_ITER);
	const uint8_t iteratorSlot = CurrentContext().symbols.Define("__iter");
	CurrentChunk().Write(OpCode::OP_SET_LOCAL);
	CurrentChunk().code.push_back(iteratorSlot);

	CurrentChunk().WriteConstant(std::monostate{});
	const uint8_t varIdx = CurrentContext().symbols.Define(node.varName);
	CurrentChunk().Write(OpCode::OP_SET_LOCAL);
	CurrentChunk().code.push_back(varIdx);

	for (const auto& adapter : node.adapters)
	{
		CurrentChunk().Write(OpCode::OP_GET_LOCAL);
		CurrentChunk().code.push_back(iteratorSlot);
		switch (adapter.kind)
		{
		case IterAdapterKind::Drop:
			adapter.argument->Accept(*this);
			CurrentChunk().Write(OpCode::OP_ITER_DROP);
			break;
		case IterAdapterKind::Take:
			adapter.argument->Accept(*this);
			CurrentChunk().Write(OpCode::OP_ITER_TAKE);
			break;
		case IterAdapterKind::Reverse:
			CurrentChunk().Write(OpCode::OP_ITER_REVERSE);
			break;
		case IterAdapterKind::Filter:
			adapter.argument->Accept(*this);
			CurrentChunk().Write(OpCode::OP_ITER_FILTER);
			break;
		case IterAdapterKind::Transform:
			adapter.argument->Accept(*this);
			CurrentChunk().Write(OpCode::OP_ITER_TRANSFORM);
			break;
		}
		CurrentChunk().Write(OpCode::OP_SET_LOCAL);
		CurrentChunk().code.push_back(iteratorSlot);
		CurrentChunk().Write(OpCode::OP_POP);
	}

	const size_t loopStart = CurrentChunk().code.size();
	CurrentChunk().Write(OpCode::OP_GET_LOCAL);
	CurrentChunk().code.push_back(iteratorSlot);
	CurrentChunk().Write(OpCode::OP_ITER_NEXT);
	const size_t exitJump = CurrentChunk().code.size();
	CurrentChunk().code.push_back(0xff);

	CurrentChunk().Write(OpCode::OP_SET_LOCAL);
	CurrentChunk().code.push_back(varIdx);
	CurrentChunk().Write(OpCode::OP_POP);
	CurrentChunk().Write(OpCode::OP_POP);

	node.body->Accept(*this);

	const auto offset = static_cast<uint16_t>(CurrentChunk().code.size() - loopStart + 3);
	CurrentChunk().Write(OpCode::OP_LOOP);
	CurrentChunk().code.push_back((offset >> 8) & 0xff);
	CurrentChunk().code.push_back(offset & 0xff);

	const auto exitDistance = CurrentChunk().code.size() - exitJump - 1;
	if (exitDistance > 0xff)
	{
		throw std::runtime_error("iter body is too large for OP_ITER_NEXT jump");
	}
	CurrentChunk().code[exitJump] = static_cast<uint8_t>(exitDistance);
	CurrentChunk().Write(OpCode::OP_POP);
}

void BytecodeGenerator::Visit(ComptimeNode& node)
{
	CurrentChunk().WriteConstant(EvaluateComptime(node.body.get()));
}

void BytecodeGenerator::Visit(ContractNode& node)
{
	if (node.expression)
	{
		node.expression->Accept(*this);
	}
}

void BytecodeGenerator::Visit(LeafNode& node)
{
	if (node.type == "true")
	{
		CurrentChunk().WriteConstant(true);
		return;
	}
	if (node.type == "false")
	{
		CurrentChunk().WriteConstant(false);
		return;
	}
	if (node.type == "null")
	{
		CurrentChunk().WriteConstant(std::monostate{});
	}
}

void BytecodeGenerator::Visit(RawNode& node)
{
	for (auto& child : node.children)
	{
		if (child)
		{
			child->Accept(*this);
		}
	}
}
