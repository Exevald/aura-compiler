#include "../SyncStaticAnalyzer.h"
#include "../common/SemanticAnalyzerSupport.h"
#include "SemanticAnalyzer.h"

#include <cctype>
#include <functional>
#include <ranges>
#include <stdexcept>

using SemanticAnalyzerDetail::FormatUndefined;
using SemanticAnalyzerDetail::ScopeExit;
using SemanticAnalyzerDetail::SplitGenericName;
using SemanticAnalyzerDetail::SubstituteTypeString;

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
		if (operandType.kind == TypeKind::Ref)
		{
			m_lastType = operandType.element ? *operandType.element : TypeInfo::Unknown();
			return;
		}
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

void SemanticAnalyzer::Visit(GoExprNode& node)
{
	auto* call = dynamic_cast<CallNode*>(node.call.get());
	if (!call)
	{
		throw std::runtime_error("go expects a function call");
	}

	if (!dynamic_cast<IdentifierNode*>(call->callee.get())
		&& !dynamic_cast<MemberAccessNode*>(call->callee.get()))
	{
		throw std::runtime_error("go currently supports only named or module-qualified function calls");
	}

	const TypeInfo resultType = VisitAndGet(node.call.get());
	if (const auto* member = dynamic_cast<MemberAccessNode*>(call->callee.get()))
	{
		const TypeInfo receiverType = VisitAndGet(member->object.get());
		if (receiverType.kind != TypeKind::Module)
		{
			ValidateSendable(receiverType, "go receiver");
		}
	}
	for (const auto& arg : call->args)
	{
		ValidateSendable(VisitAndGet(arg.get()), "go argument");
	}

	m_lastType = TypeInfo::TaskOf(resultType);
}

void SemanticAnalyzer::Visit(AwaitExprNode& node)
{
	const TypeInfo operandType = VisitAndGet(node.operand.get());
	if (operandType.kind != TypeKind::Task)
	{
		throw std::runtime_error("await expects a task value");
	}

	m_lastType = operandType.element ? *operandType.element : TypeInfo::Unknown();
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
		const TypeInfo pointerType = VisitAndGet(node.dereferenceTarget.get());
		if (pointerType.kind == TypeKind::Pointer && m_unsafeDepth <= 0)
		{
			throw std::runtime_error("Dereference assignment requires an unsafe block");
		}
		if ((pointerType.kind != TypeKind::Pointer && pointerType.kind != TypeKind::Ref) || !pointerType.element)
		{
			throw std::runtime_error("Dereference assignment requires a pointer or ref target");
		}

		const TypeInfo actualType = VisitAndGet(node.value.get());
		const TypeInfo expectedType = *pointerType.element;
		if (expectedType.kind != TypeKind::Unknown
			&& actualType.kind != TypeKind::Unknown
			&& !IsAssignable(expectedType, actualType))
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
