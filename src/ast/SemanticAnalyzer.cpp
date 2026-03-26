#include "SemanticAnalyzer.h"

#include <ranges>
#include <stdexcept>

namespace
{
std::string FormatUndefined(const std::string& name)
{
	return "Undefined variable: " + name;
}
} // namespace

void SemanticAnalyzer::Analyze(ASTNode* root)
{
	m_lastType = TypeInfo::Unknown();
	m_envStack.clear();
	m_envStack.emplace_back();

	if (root)
	{
		root->Accept(*this);
	}
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
	return t.kind == TypeKind::Bool || t.kind == TypeKind::Int || t.kind == TypeKind::Float;
}

bool SemanticAnalyzer::IsTruthyBinaryOp(const std::string& op)
{
	return op == "and" || op == "or" || op == "&&" || op == "||" || op == "mod" || op == "div";
}

void SemanticAnalyzer::EnsureDeclared(const std::string& name)
{
	if (m_envStack.empty())
	{
		throw std::runtime_error("Internal semantic analyzer error: empty environment");
	}

	for (auto& it : std::ranges::reverse_view(m_envStack))
	{
		if (it.contains(name))
		{
			return;
		}
	}

	throw std::runtime_error(FormatUndefined(name));
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::Resolve(const std::string& name) const
{
	for (const auto& it : std::ranges::reverse_view(m_envStack))
	{
		if (it.contains(name))
		{
			return it.at(name);
		}
	}

	throw std::runtime_error(FormatUndefined(name));
}

void SemanticAnalyzer::Define(const std::string& name, TypeInfo type)
{
	if (m_envStack.empty())
	{
		throw std::runtime_error("Internal semantic analyzer error: no environment scope");
	}

	m_envStack.back()[name] = std::move(type);
}

void SemanticAnalyzer::AnalyzeTypeForBinaryOp(const std::string& op, const TypeInfo& lhs, const TypeInfo& rhs)
{
	if (IsTruthyBinaryOp(op))
	{
		throw std::runtime_error("Unsupported binary operator for codegen: " + op);
	}

	if (op != "+"
		&& op != "-"
		&& op != "*"
		&& op != "/"
		&& op != "=="
		&& op != "!="
		&& op != "<"
		&& op != ">")
	{
		throw std::runtime_error("Unsupported binary operator for codegen: " + op);
	}

	if (op == "+")
	{
		if (lhs.kind == TypeKind::String || rhs.kind == TypeKind::String)
		{
			m_lastType = TypeInfo::String();
			return;
		}

		if (lhs.kind != TypeKind::Unknown && !IsPrimitiveNumeric(lhs))
		{
			throw std::runtime_error("Type mismatch for '+': lhs is not numeric/string");
		}
		if (rhs.kind != TypeKind::Unknown && !IsPrimitiveNumeric(rhs))
		{
			throw std::runtime_error("Type mismatch for '+': rhs is not numeric/string");
		}

		m_lastType = TypeInfo::Float();
		return;
	}

	if (op == "-" || op == "*" || op == "/")
	{
		if ((lhs.kind != TypeKind::Unknown && !IsPrimitiveNumeric(lhs))
			|| (rhs.kind != TypeKind::Unknown && !IsPrimitiveNumeric(rhs)))
		{
			throw std::runtime_error("Type mismatch for arithmetic operator: " + op);
		}

		m_lastType = TypeInfo::Float();
		return;
	}

	m_lastType = TypeInfo::Bool();
}

void SemanticAnalyzer::AnalyzeAssignment(ASTNode* valueExpr, const std::string& name, ASTNode* indexExpr, bool hasIndex)
{
	EnsureDeclared(name);

	auto valueType = VisitAndGet(valueExpr);

	if (!hasIndex)
	{
		Define(name, valueType);
		m_lastType = valueType;
		return;
	}

	auto containerType = Resolve(name);
	auto indexType = VisitAndGet(indexExpr);

	if (containerType.kind != TypeKind::Array && containerType.kind != TypeKind::Unknown)
	{
		throw std::runtime_error("Indexing requires array for assignment: " + name);
	}

	if (indexType.kind != TypeKind::Unknown && !IsPrimitiveNumeric(indexType))
	{
		throw std::runtime_error("Index must be a numeric type for assignment: " + name);
	}

	if (containerType.kind == TypeKind::Array && containerType.element && valueType.kind != TypeKind::Unknown)
	{
		*containerType.element = std::move(valueType);
		Define(name, containerType);
	}

	m_lastType = valueType;
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

void SemanticAnalyzer::Visit(BinaryExprNode& node)
{
	const auto lhs = VisitAndGet(node.left.get());
	const auto rhs = VisitAndGet(node.right.get());
	AnalyzeTypeForBinaryOp(node.op, lhs, rhs);
}

void SemanticAnalyzer::Visit(AssignmentNode& node)
{
	AnalyzeAssignment(node.value.get(), node.name, node.index.get(), node.index != nullptr);
}

void SemanticAnalyzer::Visit(VarDeclNode& node)
{
	TypeInfo initType = TypeInfo::Int();
	if (node.initializer)
	{
		initType = VisitAndGet(node.initializer.get());
	}

	Define(node.name, initType);
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
	Define(node.name, TypeInfo::Function());
	m_envStack.emplace_back();
	for (auto& p : node.params)
	{
		Define(p.name, TypeInfo::Unknown());
	}

	if (node.body)
	{
		node.body->Accept(*this);
	}

	m_envStack.pop_back();
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(CallNode& node)
{
	if (const auto [kind, element] = Resolve(node.callee);
		kind != TypeKind::Function && kind != TypeKind::Unknown)
	{
		throw std::runtime_error("Can only call functions: " + node.callee);
	}

	for (auto& arg : node.args)
	{
		VisitAndGet(arg.get());
	}

	m_lastType = TypeInfo::Unknown();
}

void SemanticAnalyzer::Visit(ReturnNode& node)
{
	if (node.value)
	{
		VisitAndGet(node.value.get());
	}
	m_lastType = TypeInfo::Void();
}

void SemanticAnalyzer::Visit(PrintNode& node)
{
	(void)VisitAndGet(node.value.get());
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
