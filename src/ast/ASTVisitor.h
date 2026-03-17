#pragma once

class LeafNode;
class InternalNode;

class ASTVisitor {
public:
	virtual void Visit(LeafNode& node) = 0;
	virtual void Visit(InternalNode& node) = 0;

	virtual ~ASTVisitor() = default;
};