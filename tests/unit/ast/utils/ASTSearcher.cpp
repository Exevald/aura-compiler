#include "ASTSearcher.h"

void ASTSearcher::Check(ASTNode& node)
{
	if (targetType && typeid(node) == *targetType)
	{
		foundType = true;
	}
}

void ASTSearcher::Visit(IntegerLiteralNode& n)
{
	Check(n);
	if (std::to_string(n.value) == targetValue)
	{
		foundValue = true;
	}
}

void ASTSearcher::Visit(FloatLiteralNode& n)
{
	Check(n);
	if (std::to_string(n.value) == targetValue)
	{
		foundValue = true;
	}
}

void ASTSearcher::Visit(StringLiteralNode& n)
{
	Check(n);
	if (n.value == targetValue)
	{
		foundValue = true;
	}
}

void ASTSearcher::Visit(IdentifierNode& n)
{
	Check(n);
	if (n.name == targetIdentifier || n.name == targetValue)
	{
		foundIdentifier = foundValue = true;
	}
}

void ASTSearcher::Visit(UnaryExprNode& n)
{
	Check(n);
	if (n.op == targetValue)
	{
		foundValue = true;
	}
	if (n.operand)
	{
		n.operand->Accept(*this);
	}
}

void ASTSearcher::Visit(BinaryExprNode& n)
{
	Check(n);
	n.left->Accept(*this);
	n.right->Accept(*this);
}

void ASTSearcher::Visit(AssignmentNode& n)
{
	Check(n);
	if (n.name == targetValue)
	{
		foundValue = true;
	}
	if (n.member == targetValue)
	{
		foundValue = true;
	}
	if (n.object)
	{
		n.object->Accept(*this);
	}
	if (n.value)
	{
		n.value->Accept(*this);
	}
	if (n.index)
	{
		n.index->Accept(*this);
	}
}

void ASTSearcher::Visit(VarDeclNode& n)
{
	Check(n);
	if (n.name == targetValue)
	{
		foundValue = true;
	}
	if (n.explicitType == targetValue)
	{
		foundValue = true;
	}
	if (n.initializer)
	{
		n.initializer->Accept(*this);
	}
}

void ASTSearcher::Visit(TypeAliasNode& n)
{
	Check(n);
	if (n.name == targetValue || n.aliasedType == targetValue)
	{
		foundValue = true;
	}
	for (const auto& typeParam : n.typeParams)
	{
		if (typeParam.name == targetValue)
		{
			foundValue = true;
		}
		for (const auto& constraint : typeParam.constraints)
		{
			if (constraint == targetValue)
			{
				foundValue = true;
			}
		}
	}
}

void ASTSearcher::Visit(InterfaceDeclNode& n)
{
	Check(n);
	if (n.name == targetValue)
	{
		foundValue = true;
	}
	for (const auto& typeParam : n.typeParams)
	{
		if (typeParam.name == targetValue)
		{
			foundValue = true;
		}
	}
	for (const auto& method : n.methods)
	{
		if (method.name == targetValue || method.returnType == targetValue)
		{
			foundValue = true;
		}
		for (const auto& param : method.params)
		{
			if (param.name == targetValue || param.typeName == targetValue)
			{
				foundValue = true;
			}
			if (param.defaultValue)
			{
				param.defaultValue->Accept(*this);
			}
		}
	}
}

void ASTSearcher::Visit(EffectDeclNode& n)
{
	Check(n);
	if (n.name == targetValue)
	{
		foundValue = true;
	}
	for (const auto& op : n.operations)
	{
		if (op.name == targetValue || op.returnType == targetValue)
		{
			foundValue = true;
		}
	}
}

void ASTSearcher::Visit(ActorDeclNode& n)
{
	Check(n);
	if (n.name == targetValue)
	{
		foundValue = true;
	}
	for (const auto& typeParam : n.typeParams)
	{
		if (typeParam.name == targetValue)
		{
			foundValue = true;
		}
	}
	for (const auto& field : n.fields)
	{
		if (field.name == targetValue || field.typeName == targetValue)
		{
			foundValue = true;
		}
		if (field.initializer)
		{
			field.initializer->Accept(*this);
		}
	}
	for (const auto& method : n.methods)
	{
		if (method.name == targetValue || method.returnType == targetValue)
		{
			foundValue = true;
		}
		for (const auto& param : method.params)
		{
			if (param.name == targetValue || param.typeName == targetValue)
			{
				foundValue = true;
			}
			if (param.defaultValue)
			{
				param.defaultValue->Accept(*this);
			}
		}
		for (const auto& contract : method.contracts)
		{
			if (contract)
			{
				contract->Accept(*this);
			}
		}
		if (method.body)
		{
			method.body->Accept(*this);
		}
	}
}

void ASTSearcher::Visit(StructDeclNode& n)
{
	Check(n);
	if (n.name == targetValue)
	{
		foundValue = true;
	}
	for (const auto& typeParam : n.typeParams)
	{
		if (typeParam.name == targetValue)
		{
			foundValue = true;
		}
	}
	for (const auto& [name, typeName] : n.fields)
	{
		if (name == targetValue || typeName == targetValue)
		{
			foundValue = true;
		}
	}
	for (const auto& interfaceName : n.implementedInterfaces)
	{
		if (interfaceName == targetValue)
		{
			foundValue = true;
		}
	}
	for (const auto& method : n.methods)
	{
		if (method.name == targetValue || method.returnType == targetValue)
		{
			foundValue = true;
		}
		for (const auto& param : method.params)
		{
			if (param.name == targetValue || param.typeName == targetValue)
			{
				foundValue = true;
			}
			if (param.defaultValue)
			{
				param.defaultValue->Accept(*this);
			}
		}
		for (const auto& contract : method.contracts)
		{
			if (contract)
			{
				contract->Accept(*this);
			}
		}
		if (method.body)
		{
			method.body->Accept(*this);
		}
	}
	for (const auto& contract : n.contracts)
	{
		if (contract)
		{
			contract->Accept(*this);
		}
	}
}

void ASTSearcher::Visit(EnumDeclNode& n)
{
	Check(n);
	if (n.name == targetValue)
	{
		foundValue = true;
	}
	for (const auto& typeParam : n.typeParams)
	{
		if (typeParam.name == targetValue)
		{
			foundValue = true;
		}
	}
	for (const auto& [name, argTypes] : n.variants)
	{
		if (name == targetValue)
		{
			foundValue = true;
		}
		for (const auto& argType : argTypes)
		{
			if (argType == targetValue)
			{
				foundValue = true;
			}
		}
	}
}

void ASTSearcher::Visit(BlockNode& n)
{
	Check(n);
	for (auto& s : n.statements)
	{
		if (s)
		{
			s->Accept(*this);
		}
	}
}

void ASTSearcher::Visit(ExportDeclNode& n)
{
	Check(n);
	if (n.exportedName == targetValue)
	{
		foundValue = true;
	}
	if (n.declaration)
	{
		n.declaration->Accept(*this);
	}
}

void ASTSearcher::Visit(IfStatementNode& n)
{
	Check(n);
	n.condition->Accept(*this);
	n.thenBlock->Accept(*this);
	if (n.elseBlock)
	{
		n.elseBlock->Accept(*this);
	}
}

void ASTSearcher::Visit(WhileStatementNode& n)
{
	Check(n);
	n.condition->Accept(*this);
	n.body->Accept(*this);
}

void ASTSearcher::Visit(FunctionDeclNode& n)
{
	Check(n);
	if (n.name == targetValue)
	{
		foundValue = true;
	}
	for (const auto& param : n.params)
	{
		if (param.name == targetValue || param.typeName == targetValue)
		{
			foundValue = true;
		}
		if (param.defaultValue)
		{
			param.defaultValue->Accept(*this);
		}
	}
	if (n.body)
	{
		n.body->Accept(*this);
	}
	for (const auto& typeParam : n.typeParams)
	{
		if (typeParam.name == targetValue)
		{
			foundValue = true;
		}
		for (const auto& constraint : typeParam.constraints)
		{
			if (constraint == targetValue)
			{
				foundValue = true;
			}
		}
	}
	for (const auto& contract : n.contracts)
	{
		if (contract)
		{
			contract->Accept(*this);
		}
	}
}

void ASTSearcher::Visit(FunctionExprNode& n)
{
	Check(n);
	for (const auto& param : n.params)
	{
		if (param.name == targetValue || param.typeName == targetValue)
		{
			foundValue = true;
		}
		if (param.defaultValue)
		{
			param.defaultValue->Accept(*this);
		}
	}
	if (n.body)
	{
		n.body->Accept(*this);
	}
}

void ASTSearcher::Visit(CallNode& n)
{
	Check(n);
	if (n.callee)
	{
		n.callee->Accept(*this);
	}
	for (const auto& a : n.args)
	{
		a->Accept(*this);
	}
}

void ASTSearcher::Visit(GoExprNode& n)
{
	Check(n);
	if (n.call)
	{
		n.call->Accept(*this);
	}
}

void ASTSearcher::Visit(AwaitExprNode& n)
{
	Check(n);
	if (n.operand)
	{
		n.operand->Accept(*this);
	}
}

void ASTSearcher::Visit(MemberAccessNode& n)
{
	Check(n);
	if (n.object)
	{
		n.object->Accept(*this);
	}
	if (n.member == targetValue)
	{
		foundValue = true;
	}
}

void ASTSearcher::Visit(ModuleDeclNode& n)
{
	Check(n);
	if (n.qualifiedName == targetValue)
	{
		foundValue = true;
	}
}

void ASTSearcher::Visit(ImportDeclNode& n)
{
	Check(n);
	if (n.qualifiedName == targetValue || n.alias == targetValue)
	{
		foundValue = true;
	}
}

void ASTSearcher::Visit(ReturnNode& n)
{
	Check(n);
	if (n.value)
	{
		n.value->Accept(*this);
	}
}

void ASTSearcher::Visit(PrintNode& n)
{
	Check(n);
	if (n.value)
	{
		n.value->Accept(*this);
	}
}

void ASTSearcher::Visit(UnsafeNode& n)
{
	Check(n);
	if (n.body)
	{
		n.body->Accept(*this);
	}
	if (targetValue == "unsafe")
	{
		foundValue = true;
	}
}

void ASTSearcher::Visit(ArrayLiteralNode& n)
{
	Check(n);
	for (auto& e : n.elements)
	{
		e->Accept(*this);
	}
}

void ASTSearcher::Visit(MapLiteralNode& n)
{
	Check(n);
	if (n.keyTypeName == targetValue || n.valueTypeName == targetValue)
	{
		foundValue = true;
	}
	for (auto& [key, value] : n.entries)
	{
		key->Accept(*this);
		value->Accept(*this);
	}
}

void ASTSearcher::Visit(IndexNode& n)
{
	Check(n);
	n.container->Accept(*this);
	n.index->Accept(*this);
}

void ASTSearcher::Visit(IterNode& n)
{
	Check(n);
	if (n.varName == targetValue)
	{
		foundValue = true;
	}
	if (n.collection)
	{
		n.collection->Accept(*this);
	}
	for (auto& adapter : n.adapters)
	{
		if (adapter.argument)
		{
			adapter.argument->Accept(*this);
		}
	}
	if (n.body)
	{
		n.body->Accept(*this);
	}
}

void ASTSearcher::Visit(TransactionNode& n)
{
	Check(n);
	for (const auto& region : n.regions)
	{
		if (region.name == targetValue)
		{
			foundValue = true;
			break;
		}
	}
	if (n.body)
	{
		n.body->Accept(*this);
	}
}

void ASTSearcher::Visit(HandleNode& n)
{
	Check(n);
	if (n.expression)
	{
		n.expression->Accept(*this);
	}
	for (const auto& handler : n.handlers)
	{
		if (handler.effectName == targetValue)
		{
			foundValue = true;
		}
		if (handler.body)
		{
			handler.body->Accept(*this);
		}
	}
}

void ASTSearcher::Visit(RawNode& n)
{
	Check(n);
	if (n.ruleName == targetRule)
	{
		foundRule = true;
	}
	for (auto& c : n.children)
	{
		if (c)
		{
			c->Accept(*this);
		}
	}
}

void ASTSearcher::Visit(LeafNode& n)
{
	if (n.value == targetValue)
	{
		foundValue = true;
	}
}

void ASTSearcher::Visit(ComptimeNode& n)
{
	Check(n);
	if (n.body)
	{
		n.body->Accept(*this);
	}
	foundValue = (targetValue == "comptime");
}

void ASTSearcher::Visit(ContractNode& n)
{
	Check(n);
	if ((n.kind == ContractKind::Requires && targetValue == "requires")
		|| (n.kind == ContractKind::Ensures && targetValue == "ensures")
		|| (n.kind == ContractKind::Invariant && targetValue == "invariant"))
	{
		foundValue = true;
	}
	if (n.expression)
	{
		n.expression->Accept(*this);
	}
}
