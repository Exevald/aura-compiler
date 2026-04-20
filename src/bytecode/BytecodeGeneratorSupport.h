#pragma once

#include <functional>
#include <utility>

namespace BytecodeGeneratorDetail
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

inline bool ShouldPopStatementResult(const ASTNode* node)
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

} // namespace BytecodeGeneratorDetail
