#include "AST.h"
#include "ASTVisitor.h"

LeafNode::LeafNode(std::string t, std::string v)
	: type(std::move(t))
	, value(std::move(v))
{
}

void LeafNode::Accept(ASTVisitor& visitor)
{
	visitor.Visit(*this);
}

InternalNode::InternalNode(std::string lhs)
	: ruleLhs(std::move(lhs))
{
}

void InternalNode::AddChild(ASTNodePtr child)
{
	if (child)
	{
		children.push_back(std::move(child));
	}
}

void InternalNode::Accept(ASTVisitor& visitor)
{
	visitor.Visit(*this);
}