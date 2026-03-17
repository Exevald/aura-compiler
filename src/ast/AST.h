#pragma once

#include <string>
#include <vector>
#include <memory>

class ASTVisitor;

class ASTNode {
public:
	virtual ~ASTNode() = default;
	virtual void Accept(ASTVisitor& visitor) = 0;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

class LeafNode : public ASTNode {
public:
	std::string type;
	std::string value;

	LeafNode(std::string t, std::string v);

	void Accept(ASTVisitor& visitor) override;
};

class InternalNode : public ASTNode {
public:
	std::string ruleLhs;
	std::vector<ASTNodePtr> children;

	explicit InternalNode(std::string lhs);

	void AddChild(ASTNodePtr child);
	void Accept(ASTVisitor& visitor) override;
};