#include "BytecodeGenerator.h"
#include "../ast/SemanticAnalyzer.h"

#include <algorithm>
#include <functional>

using namespace VM::Core;

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

// This translation unit intentionally keeps bytecode emission out-of-line to minimize rebuild fan-out.

bool ShouldPopStatementResult(const ASTNode* node)
{
	return dynamic_cast<const BinaryExprNode*>(node)
		|| dynamic_cast<const AssignmentNode*>(node)
		|| dynamic_cast<const CallNode*>(node)
		|| dynamic_cast<const IndexNode*>(node)
		|| dynamic_cast<const IdentifierNode*>(node)
		|| dynamic_cast<const UnaryExprNode*>(node)
		|| dynamic_cast<const IntegerLiteralNode*>(node)
		|| dynamic_cast<const FloatLiteralNode*>(node)
		|| dynamic_cast<const StringLiteralNode*>(node)
		|| dynamic_cast<const FunctionExprNode*>(node)
		|| dynamic_cast<const MemberAccessNode*>(node)
		|| dynamic_cast<const ArrayLiteralNode*>(node);
}

} // namespace

VM::Execution::Chunk BytecodeGenerator::Compile(ASTNode* root)
{
	SemanticAnalyzer analyzer;
	analyzer.Analyze(root);

	m_contexts.clear();
	m_metadata.Clear();
	m_currentModule.clear();
	m_lambdaCounter = 0;

	FunctionContext context;
	context.function = std::make_shared<Function>();
	context.function->name = "top_level";
	context.symbols.Reset();
	context.structVarScopes.emplace_back();
	context.enumVarScopes.emplace_back();
	context.interfaceVarScopes.emplace_back();
	m_contexts.push_back(std::move(context));

	if (root)
	{
		root->Accept(*this);
	}

	CurrentChunk().Write(OpCode::OP_RETURN);
	return *CurrentContext().function->chunk;
}

VM::Execution::Chunk& BytecodeGenerator::CurrentChunk() const
{
	return *CurrentContext().function->chunk;
}

BytecodeGenerator::FunctionContext& BytecodeGenerator::CurrentContext()
{
	return m_contexts.back();
}

const BytecodeGenerator::FunctionContext& BytecodeGenerator::CurrentContext() const
{
	return m_contexts.back();
}

void BytecodeGenerator::InitializeFunctionContext(FunctionContext& context, const std::string& name)
{
	context.function = std::make_shared<Function>();
	context.function->name = name;
	context.symbols.Reset();
	PushContextScope(context);
}

void BytecodeGenerator::PushContextScope(FunctionContext& context)
{
	context.symbols.PushScope();
	context.structVarScopes.emplace_back();
	context.enumVarScopes.emplace_back();
	context.interfaceVarScopes.emplace_back();
}

void BytecodeGenerator::PopContextScope(FunctionContext& context)
{
	context.symbols.PopScope();
	context.structVarScopes.pop_back();
	context.enumVarScopes.pop_back();
	context.interfaceVarScopes.pop_back();
}

size_t BytecodeGenerator::EmitJump(const OpCode opcode) const
{
	CurrentChunk().Write(opcode);
	CurrentChunk().code.push_back(0xff);
	CurrentChunk().code.push_back(0xff);
	return CurrentChunk().code.size() - 2;
}

void BytecodeGenerator::PatchJump(const size_t jumpAddr) const
{
	const auto jumpDist = static_cast<uint16_t>(CurrentChunk().code.size() - jumpAddr - 2);
	CurrentChunk().code[jumpAddr] = (jumpDist >> 8) & 0xff;
	CurrentChunk().code[jumpAddr + 1] = jumpDist & 0xff;
}

std::string BytecodeGenerator::QualifyName(const std::string& name) const
{
	if (name.find('.') != std::string::npos)
	{
		return name;
	}
	if (const auto it = m_metadata.importAliases.find(name); it != m_metadata.importAliases.end())
	{
		return it->second;
	}
	if (!m_currentModule.empty())
	{
		return m_currentModule + "." + name;
	}
	return name;
}

std::optional<uint8_t> BytecodeGenerator::ResolveLocal(const std::string& name) const
{
	return CurrentContext().symbols.Resolve(name);
}

std::optional<uint8_t> BytecodeGenerator::ResolveUpvalue(size_t contextIndex, const std::string& name)
{
	if (contextIndex == 0)
	{
		return std::nullopt;
	}

	auto& parent = m_contexts[contextIndex - 1];
	if (const auto local = parent.symbols.Resolve(name))
	{
		auto& current = m_contexts[contextIndex];
		if (const auto existing = current.upvalueLookup.find(name); existing != current.upvalueLookup.end())
		{
			return existing->second;
		}

		const uint8_t index = static_cast<uint8_t>(current.upvalues.size());
		current.upvalues.push_back({ name, true, *local });
		current.upvalueLookup[name] = index;
		current.function->captureNames.push_back(name);
		return index;
	}

	if (const auto parentUpvalue = ResolveUpvalue(contextIndex - 1, name))
	{
		auto& current = m_contexts[contextIndex];
		if (const auto existing = current.upvalueLookup.find(name); existing != current.upvalueLookup.end())
		{
			return existing->second;
		}

		const uint8_t index = static_cast<uint8_t>(current.upvalues.size());
		current.upvalues.push_back({ name, false, *parentUpvalue });
		current.upvalueLookup[name] = index;
		current.function->captureNames.push_back(name);
		return index;
	}

	return std::nullopt;
}

void BytecodeGenerator::EmitGetVariable(const std::string& name)
{
	if (const auto local = ResolveLocal(name))
	{
		CurrentChunk().Write(OpCode::OP_GET_LOCAL);
		CurrentChunk().code.push_back(*local);
		return;
	}

	if (m_contexts.size() > 1)
	{
		if (const auto upvalue = ResolveUpvalue(m_contexts.size() - 1, name))
		{
			CurrentChunk().Write(OpCode::OP_GET_UPVALUE);
			CurrentChunk().code.push_back(*upvalue);
			return;
		}
	}

	if (const auto selfFieldIndex = ResolveSelfFieldIndex(name))
	{
		CurrentChunk().Write(OpCode::OP_GET_LOCAL);
		CurrentChunk().code.push_back(0);
		CurrentChunk().Write(OpCode::OP_MEMBER_GET);
		CurrentChunk().code.push_back(*selfFieldIndex);
		return;
	}

	const uint8_t nameIndex = CurrentChunk().AddConstant(
		std::make_shared<const std::string>(QualifyName(name)));
	CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
	CurrentChunk().code.push_back(nameIndex);
}

void BytecodeGenerator::EmitSetVariable(const std::string& name)
{
	if (const auto local = ResolveLocal(name))
	{
		CurrentChunk().Write(OpCode::OP_SET_LOCAL);
		CurrentChunk().code.push_back(*local);
		return;
	}

	if (m_contexts.size() > 1)
	{
		if (const auto upvalue = ResolveUpvalue(m_contexts.size() - 1, name))
		{
			CurrentChunk().Write(OpCode::OP_SET_UPVALUE);
			CurrentChunk().code.push_back(*upvalue);
			return;
		}
	}

	if (const auto selfFieldIndex = ResolveSelfFieldIndex(name))
	{
		CurrentChunk().Write(OpCode::OP_GET_LOCAL);
		CurrentChunk().code.push_back(0);
		CurrentChunk().Write(OpCode::OP_MEMBER_SET);
		CurrentChunk().code.push_back(*selfFieldIndex);
		return;
	}

	const uint8_t nameIndex = CurrentChunk().AddConstant(
		std::make_shared<const std::string>(QualifyName(name)));
	CurrentChunk().Write(OpCode::OP_SET_GLOBAL);
	CurrentChunk().code.push_back(nameIndex);
}

std::optional<uint8_t> BytecodeGenerator::ResolveSelfFieldIndex(const std::string& member) const
{
	if (!CurrentContext().methodSelfStructType)
	{
		return std::nullopt;
	}

	const auto layoutIt = m_metadata.structLayouts.find(*CurrentContext().methodSelfStructType);
	if (layoutIt == m_metadata.structLayouts.end())
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

void BytecodeGenerator::EmitAddressOf(ASTNode* operand)
{
	if (const auto* identifier = dynamic_cast<IdentifierNode*>(operand))
	{
		if (const auto local = ResolveLocal(identifier->name))
		{
			CurrentChunk().Write(OpCode::OP_ADDR_LOCAL);
			CurrentChunk().code.push_back(*local);
			return;
		}
		if (m_contexts.size() > 1 && ResolveUpvalue(m_contexts.size() - 1, identifier->name))
		{
			const auto upvalue = ResolveUpvalue(
				m_contexts.size() - 1, identifier->name);
			CurrentChunk().Write(OpCode::OP_ADDR_UPVALUE);
			CurrentChunk().code.push_back(*upvalue);
			return;
		}

		const uint8_t nameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(QualifyName(identifier->name)));
		CurrentChunk().Write(OpCode::OP_ADDR_GLOBAL);
		CurrentChunk().code.push_back(nameIndex);
		return;
	}

	if (auto* memberAccess = dynamic_cast<MemberAccessNode*>(operand))
	{
		const auto fieldIndex = ResolveStructFieldIndex(memberAccess->object.get(), memberAccess->member);
		if (!fieldIndex)
		{
			throw std::runtime_error("Address-of member requires a struct field");
		}

		memberAccess->object->Accept(*this);
		CurrentChunk().WriteConstant(*fieldIndex);
		CurrentChunk().Write(OpCode::OP_ADDR_MEMBER);
		return;
	}

	if (const auto* indexNode = dynamic_cast<IndexNode*>(operand))
	{
		indexNode->container->Accept(*this);
		indexNode->index->Accept(*this);
		CurrentChunk().Write(OpCode::OP_ADDR_MEMBER);
		return;
	}

	throw std::runtime_error("Address-of requires an assignable operand");
}

void BytecodeGenerator::EmitFunctionObject(
	const std::shared_ptr<Function>& function,
	const std::vector<UpvalueInfo>& upvalues) const
{
	const uint8_t functionIndex = CurrentChunk().AddConstant(function);
	for (const auto& upvalue : upvalues)
	{
		if (upvalue.sourceIsLocal)
		{
			CurrentChunk().Write(OpCode::OP_GET_LOCAL);
		}
		else
		{
			CurrentChunk().Write(OpCode::OP_GET_UPVALUE);
		}
		CurrentChunk().code.push_back(upvalue.sourceIndex);
	}
	CurrentChunk().Write(OpCode::OP_CLOSURE);
	CurrentChunk().code.push_back(functionIndex);
}

void BytecodeGenerator::CompileFunctionBody(
	const std::string& name,
	const std::vector<Parameter>& params,
	ASTNode* body,
	const std::function<void()>& emitResult)
{
	FunctionContext context;
	InitializeFunctionContext(context, name);

	for (const auto& [_, typeName] : params)
	{
		context.symbols.Define(_);
		context.function->arity++;
		if (m_metadata.structLayouts.contains(QualifyName(typeName)))
		{
			context.structVarScopes.back()[_] = QualifyName(typeName);
		}
		else if (m_metadata.structLayouts.contains(typeName))
		{
			context.structVarScopes.back()[_] = typeName;
		}
		if (m_metadata.interfaceMethods.contains(QualifyName(typeName)))
		{
			context.interfaceVarScopes.back()[_] = FunctionContext::InterfaceBinding{
				QualifyName(typeName),
				"",
				FunctionContext::InterfaceBinding::RuntimeKind::Unknown
			};
		}
		else if (m_metadata.interfaceMethods.contains(typeName))
		{
			context.interfaceVarScopes.back()[_] = FunctionContext::InterfaceBinding{
				typeName,
				"",
				FunctionContext::InterfaceBinding::RuntimeKind::Unknown
			};
		}
	}

	m_contexts.push_back(std::move(context));
	if (body)
	{
		body->Accept(*this);
	}
	CurrentChunk().WriteConstant(std::monostate{});
	CurrentChunk().Write(OpCode::OP_RETURN);

	const auto completed = CurrentContext().function;
	const auto upvalues = CurrentContext().upvalues;
	m_contexts.pop_back();

	EmitFunctionObject(completed, upvalues);
	emitResult();
}

std::string BytecodeGenerator::DefaultImportAlias(const std::string& qualifiedName)
{
	const auto pos = qualifiedName.find_last_of('.');
	return (pos == std::string::npos) ? qualifiedName : qualifiedName.substr(pos + 1);
}

void BytecodeGenerator::Visit(IntegerLiteralNode& node)
{
	CurrentChunk().WriteConstant(node.value);
}

void BytecodeGenerator::Visit(FloatLiteralNode& node)
{
	CurrentChunk().WriteConstant(node.value);
}

void BytecodeGenerator::Visit(StringLiteralNode& node)
{
	CurrentChunk().WriteConstant(std::make_shared<const std::string>(node.value));
}

void BytecodeGenerator::Visit(IdentifierNode& node)
{
	EmitGetVariable(node.name);
}

void BytecodeGenerator::Visit(UnaryExprNode& node)
{
	if (node.op == "+")
	{
		node.operand->Accept(*this);
		return;
	}
	if (node.op == "-")
	{
		node.operand->Accept(*this);
		CurrentChunk().Write(OpCode::OP_NEGATE);
		return;
	}
	if (node.op == "!" || node.op == "not")
	{
		node.operand->Accept(*this);
		CurrentChunk().Write(OpCode::OP_NOT);
		return;
	}
	if (node.op == "*")
	{
		node.operand->Accept(*this);
		CurrentChunk().Write(OpCode::OP_DEREF_GET);
		return;
	}
	if (node.op == "&")
	{
		EmitAddressOf(node.operand.get());
		return;
	}

	throw std::runtime_error("Unsupported unary operator for codegen: " + node.op);
}

void BytecodeGenerator::Visit(BinaryExprNode& node)
{
	node.left->Accept(*this);
	node.right->Accept(*this);
	EmitBinaryOp(node.op);
}

void BytecodeGenerator::Visit(AssignmentNode& node)
{
	AccessEmitter{ *this }.EmitAssignment(node);
}

void BytecodeGenerator::Visit(VarDeclNode& node)
{
	if (node.initializer)
	{
		node.initializer->Accept(*this);
	}
	else
	{
		CurrentChunk().WriteConstant(std::monostate{});
	}

	const uint8_t localIndex = CurrentContext().symbols.Define(node.name);
	CurrentChunk().Write(OpCode::OP_SET_LOCAL);
	CurrentChunk().code.push_back(localIndex);

	if (m_contexts.size() == 1 && !m_currentModule.empty())
	{
		CurrentChunk().Write(OpCode::OP_GET_LOCAL);
		CurrentChunk().code.push_back(localIndex);
		const uint8_t nameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(QualifyName(node.name)));
		CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
		CurrentChunk().code.push_back(nameIndex);
	}

	if (const auto structType = InferStructTypeName(node.initializer.get()))
	{
		CurrentContext().structVarScopes.back()[node.name] = *structType;
	}
	else if (!node.explicitType.empty() && node.explicitType != "auto")
	{
		if (m_metadata.structLayouts.contains(QualifyName(node.explicitType)))
		{
			CurrentContext().structVarScopes.back()[node.name] = QualifyName(node.explicitType);
		}
		else if (m_metadata.structLayouts.contains(node.explicitType))
		{
			CurrentContext().structVarScopes.back()[node.name] = node.explicitType;
		}
	}

	if (const auto enumType = InferEnumTypeName(node.initializer.get()))
	{
		CurrentContext().enumVarScopes.back()[node.name] = *enumType;
	}

	if (const auto interfaceBinding = InferInterfaceBinding(node.initializer.get(), node.explicitType))
	{
		CurrentContext().interfaceVarScopes.back()[node.name] = *interfaceBinding;
	}
	else if (!node.explicitType.empty() && node.explicitType != "auto")
	{
		if (m_metadata.interfaceMethods.contains(QualifyName(node.explicitType)))
		{
			CurrentContext().interfaceVarScopes.back()[node.name] = FunctionContext::InterfaceBinding{
				QualifyName(node.explicitType),
				"",
				FunctionContext::InterfaceBinding::RuntimeKind::Unknown
			};
		}
		else if (m_metadata.interfaceMethods.contains(node.explicitType))
		{
			CurrentContext().interfaceVarScopes.back()[node.name] = FunctionContext::InterfaceBinding{
				node.explicitType,
				"",
				FunctionContext::InterfaceBinding::RuntimeKind::Unknown
			};
		}
	}
}

void BytecodeGenerator::Visit(TypeAliasNode& node)
{
	(void)node;
}

void BytecodeGenerator::Visit(InterfaceDeclNode& node)
{
	auto& methods = m_metadata.interfaceMethods[QualifyName(node.name)];
	methods.clear();
	for (const auto& method : node.methods)
	{
		methods.push_back(method.name);
	}
}

void BytecodeGenerator::Visit(StructDeclNode& node)
{
	const std::string qualifiedName = QualifyName(node.name);
	m_metadata.structLayouts[qualifiedName].clear();
	m_metadata.structFieldTypes[qualifiedName].clear();
	m_metadata.structMethodNames[qualifiedName].clear();
	auto& layout = m_metadata.structLayouts[qualifiedName];
	auto& fieldTypes = m_metadata.structFieldTypes[qualifiedName];
	layout.reserve(node.fields.size());
	for (const auto& field : node.fields)
	{
		layout.push_back(field.name);
		if (m_metadata.structLayouts.contains(QualifyName(field.typeName)))
		{
			fieldTypes[field.name] = QualifyName(field.typeName);
		}
		else
		{
			fieldTypes[field.name] = field.typeName;
		}
	}

	for (const auto& method : node.methods)
	{
		const std::string hiddenName = qualifiedName + "." + method.name;
		m_metadata.structMethodNames[qualifiedName][method.name] = hiddenName;

		std::vector<Parameter> loweredParams;
		loweredParams.push_back({ "self", qualifiedName });
		for (const auto& param : method.params)
		{
			loweredParams.push_back(param);
		}

		FunctionContext context;
		InitializeFunctionContext(context, hiddenName);
		context.methodSelfStructType = qualifiedName;
		context.symbols.Define("self");
		context.function->arity++;
		context.structVarScopes.back()["self"] = qualifiedName;
		for (const auto& param : method.params)
		{
			context.symbols.Define(param.name);
			context.function->arity++;
		}

		m_contexts.push_back(std::move(context));
		if (method.body)
		{
			method.body->Accept(*this);
		}
		CurrentChunk().WriteConstant(std::monostate{});
		CurrentChunk().Write(OpCode::OP_RETURN);

		auto completed = CurrentContext().function;
		auto upvalues = CurrentContext().upvalues;
		m_contexts.pop_back();

		EmitFunctionObject(completed, upvalues);
		const uint8_t nameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(hiddenName));
		CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
		CurrentChunk().code.push_back(nameIndex);
	}
}

void BytecodeGenerator::Visit(EnumDeclNode& node)
{
	const std::string enumName = QualifyName(node.name);
	for (size_t index = 0; index < node.variants.size(); ++index)
	{
		m_metadata.enumVariants[QualifyName(node.variants[index].name)] = {
			static_cast<uint8_t>(index),
			static_cast<uint8_t>(node.variants[index].argTypes.size()),
			enumName
		};
	}
}

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
	const std::string exportedName = QualifyName(node.name);
	CompileFunctionBody(exportedName, node.params, node.body.get(), [this, &exportedName]() {
		const uint8_t nameIndex = CurrentChunk().AddConstant(
			std::make_shared<const std::string>(exportedName));
		CurrentChunk().Write(OpCode::OP_DEFINE_GLOBAL);
		CurrentChunk().code.push_back(nameIndex);
	});
}

void BytecodeGenerator::Visit(FunctionExprNode& node)
{
	std::string name = node.name;
	if (name.empty())
	{
		name = "<lambda_" + std::to_string(++m_lambdaCounter) + ">";
	}

	CompileFunctionBody(name, node.params, node.body.get(), []() {});
}

void BytecodeGenerator::Visit(CallNode& node)
{
	AccessEmitter{ *this }.EmitCall(node);
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
	m_metadata.importAliases[node.alias.empty() ? DefaultImportAlias(node.qualifiedName) : node.alias] = node.qualifiedName;

	auto module = std::make_shared<VM::Core::Module>();
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

void BytecodeGenerator::Visit(ArrayLiteralNode& node)
{
	for (const auto& el : node.elements)
	{
		el->Accept(*this);
	}
	CurrentChunk().Write(OpCode::OP_BUILD_ARRAY);
	CurrentChunk().code.push_back(static_cast<uint8_t>(node.elements.size()));
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
	node.collection->Accept(*this);
	CurrentChunk().Write(OpCode::OP_MAKE_ITER);

	for (const auto& adapter : node.adapters)
	{
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
	}

	const size_t loopStart = CurrentChunk().code.size();
	const size_t exitJump = EmitJump(OpCode::OP_ITER_NEXT);

	const uint8_t varIdx = CurrentContext().symbols.Define(node.varName);
	CurrentChunk().Write(OpCode::OP_SET_LOCAL);
	CurrentChunk().code.push_back(varIdx);

	node.body->Accept(*this);

	const auto offset = static_cast<uint16_t>(CurrentChunk().code.size() - loopStart + 3);
	CurrentChunk().Write(OpCode::OP_LOOP);
	CurrentChunk().code.push_back((offset >> 8) & 0xff);
	CurrentChunk().code.push_back(offset & 0xff);

	PatchJump(exitJump);
	CurrentChunk().Write(OpCode::OP_POP);
}

void BytecodeGenerator::Visit(ComptimeNode& node)
{
	if (node.body)
	{
		node.body->Accept(*this);
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

bool BytecodeGenerator::IsEnumTagAccess(const ASTNode* object, const std::string& member) const
{
	return MetadataResolver{ *this }.IsEnumTagAccess(object, member);
}

void BytecodeGenerator::AccessEmitter::EmitCallArguments(const std::vector<ASTNodePtr>& args) const
{
	for (const auto& arg : args)
	{
		arg->Accept(generator);
	}
}

bool BytecodeGenerator::AccessEmitter::TryEmitDirectTypeConstructorCall(const CallNode& node) const
{
	if (const auto* identifier = dynamic_cast<const IdentifierNode*>(node.callee.get()))
	{
		const std::string structName = generator.QualifyName(identifier->name);
		if (const auto it = generator.m_metadata.structLayouts.find(structName); it != generator.m_metadata.structLayouts.end())
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
	}

	if (const auto* memberAccess = dynamic_cast<const MemberAccessNode*>(node.callee.get()))
	{
		if (const auto* moduleIdentifier = dynamic_cast<const IdentifierNode*>(memberAccess->object.get()))
		{
			if (const auto aliasIt = generator.m_metadata.importAliases.find(moduleIdentifier->name);
				aliasIt != generator.m_metadata.importAliases.end())
			{
				const std::string qualifiedName = aliasIt->second + "." + memberAccess->member;
				if (const auto it = generator.m_metadata.structLayouts.find(qualifiedName); it != generator.m_metadata.structLayouts.end())
				{
					EmitCallArguments(node.args);
					generator.CurrentChunk().Write(OpCode::OP_BUILD_STRUCT);
					generator.CurrentChunk().code.push_back(static_cast<uint8_t>(it->second.size()));
					return true;
				}

				if (const auto it = generator.m_metadata.enumVariants.find(qualifiedName); it != generator.m_metadata.enumVariants.end())
				{
					EmitCallArguments(node.args);
					generator.CurrentChunk().Write(OpCode::OP_BUILD_ENUM);
					generator.CurrentChunk().code.push_back(it->second.tag);
					generator.CurrentChunk().code.push_back(it->second.argCount);
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

	if (const auto hiddenMethod = generator.ResolveStructMethod(memberAccess->object.get(), memberAccess->member))
	{
		const uint8_t fnIndex = generator.CurrentChunk().AddConstant(
			std::make_shared<const std::string>(*hiddenMethod));
		generator.CurrentChunk().Write(OpCode::OP_GET_GLOBAL);
		generator.CurrentChunk().code.push_back(fnIndex);
		memberAccess->object->Accept(generator);
		EmitCallArguments(node.args);
		generator.CurrentChunk().Write(OpCode::OP_CALL);
		generator.CurrentChunk().code.push_back(static_cast<uint8_t>(node.args.size() + 1));
		return true;
	}

	if (generator.IsFunctionBackedInterfaceMethod(memberAccess->object.get(), memberAccess->member))
	{
		memberAccess->object->Accept(generator);
		EmitCallArguments(node.args);
		generator.CurrentChunk().Write(OpCode::OP_CALL);
		generator.CurrentChunk().code.push_back(static_cast<uint8_t>(node.args.size()));
		return true;
	}

	return false;
}

void BytecodeGenerator::AccessEmitter::EmitCall(const CallNode& node) const
{
	if (TryEmitDirectTypeConstructorCall(node) || TryEmitMemberCall(node))
	{
		return;
	}

	node.callee->Accept(generator);
	EmitCallArguments(node.args);
	generator.CurrentChunk().Write(OpCode::OP_CALL);
	generator.CurrentChunk().code.push_back(static_cast<uint8_t>(node.args.size()));
}

void BytecodeGenerator::AccessEmitter::EmitMemberAccess(MemberAccessNode& node) const
{
	if (generator.ResolveStructMethod(node.object.get(), node.member))
	{
		throw std::runtime_error("Struct methods can only be used in call position");
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

	if (const auto fieldIndex = generator.ResolveStructFieldIndex(node.object.get(), node.member))
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

	if (!node.name.empty() && generator.CurrentContext().methodSelfStructType)
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

void BytecodeGenerator::EmitBinaryOp(const std::string& op) const
{
	if (op == "+")
		CurrentChunk().Write(OpCode::OP_ADD);
	else if (op == "-")
		CurrentChunk().Write(OpCode::OP_SUBTRACT);
	else if (op == "*")
		CurrentChunk().Write(OpCode::OP_MULTIPLY);
	else if (op == "/")
		CurrentChunk().Write(OpCode::OP_DIVIDE);
	else if (op == "div")
		CurrentChunk().Write(OpCode::OP_DIV);
	else if (op == "mod")
		CurrentChunk().Write(OpCode::OP_MOD);
	else if (op == "and" || op == "&&")
		CurrentChunk().Write(OpCode::OP_AND);
	else if (op == "or" || op == "||")
		CurrentChunk().Write(OpCode::OP_OR);
	else if (op == "==")
		CurrentChunk().Write(OpCode::OP_EQUAL);
	else if (op == "!=")
		CurrentChunk().Write(OpCode::OP_NOT_EQUAL);
	else if (op == "<")
		CurrentChunk().Write(OpCode::OP_LESS);
	else if (op == ">")
		CurrentChunk().Write(OpCode::OP_GREATER);
	else if (op == "<=")
		CurrentChunk().Write(OpCode::OP_LESS_EQUAL);
	else if (op == ">=")
		CurrentChunk().Write(OpCode::OP_GREATER_EQUAL);
}
