#pragma once

#include <memory>
#include <string>
#include <vector>

class ComptimeNode;
class IntegerLiteralNode;
class FloatLiteralNode;
class StringLiteralNode;
class IdentifierNode;
class BinaryExprNode;
class AssignmentNode;
class VarDeclNode;
class BlockNode;
class IfStatementNode;
class WhileStatementNode;
class FunctionDeclNode;
class CallNode;
class ReturnNode;
class PrintNode;
class ArrayLiteralNode;
class IndexNode;
class IterNode;
class RawNode;
class LeafNode;

class ASTVisitor
{
public:
	virtual ~ASTVisitor() = default;
	virtual void Visit(IntegerLiteralNode& node) = 0;
	virtual void Visit(FloatLiteralNode& node) = 0;
	virtual void Visit(StringLiteralNode& node) = 0;
	virtual void Visit(IdentifierNode& node) = 0;
	virtual void Visit(BinaryExprNode& node) = 0;
	virtual void Visit(AssignmentNode& node) = 0;
	virtual void Visit(VarDeclNode& node) = 0;
	virtual void Visit(BlockNode& node) = 0;
	virtual void Visit(IfStatementNode& node) = 0;
	virtual void Visit(WhileStatementNode& node) = 0;
	virtual void Visit(FunctionDeclNode& node) = 0;
	virtual void Visit(CallNode& node) = 0;
	virtual void Visit(ReturnNode& node) = 0;
	virtual void Visit(PrintNode& node) = 0;
	virtual void Visit(ArrayLiteralNode& node) = 0;
	virtual void Visit(IndexNode& node) = 0;
	virtual void Visit(IterNode& node) = 0;
	virtual void Visit(LeafNode& node) = 0;
	virtual void Visit(RawNode& node) = 0;
	virtual void Visit(ComptimeNode& node) = 0;
};

class ASTNode
{
public:
	virtual ~ASTNode() = default;
	virtual void Accept(ASTVisitor& visitor) = 0;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

class IntegerLiteralNode : public ASTNode
{
public:
	int64_t value;
	explicit IntegerLiteralNode(int64_t v)
		: value(v)
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class FloatLiteralNode : public ASTNode
{
public:
	double value;
	explicit FloatLiteralNode(double v)
		: value(v)
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class StringLiteralNode : public ASTNode
{
public:
	std::string value;
	explicit StringLiteralNode(std::string v)
		: value(std::move(v))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class IdentifierNode : public ASTNode
{
public:
	std::string name;
	explicit IdentifierNode(std::string n)
		: name(std::move(n))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class BinaryExprNode : public ASTNode
{
public:
	ASTNodePtr left;
	std::string op;
	ASTNodePtr right;
	BinaryExprNode(ASTNodePtr l, std::string o, ASTNodePtr r)
		: left(std::move(l))
		, op(std::move(o))
		, right(std::move(r))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class AssignmentNode : public ASTNode
{
public:
	std::string name;
	ASTNodePtr value;
	ASTNodePtr index;
	AssignmentNode(std::string n, ASTNodePtr v, ASTNodePtr idx = nullptr)
		: name(std::move(n))
		, value(std::move(v))
		, index(std::move(idx))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class BlockNode : public ASTNode
{
public:
	std::vector<ASTNodePtr> statements;
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class IfStatementNode : public ASTNode
{
public:
	ASTNodePtr condition;
	ASTNodePtr thenBlock;
	ASTNodePtr elseBlock;
	IfStatementNode(ASTNodePtr c, ASTNodePtr t, ASTNodePtr e = nullptr)
		: condition(std::move(c))
		, thenBlock(std::move(t))
		, elseBlock(std::move(e))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class WhileStatementNode : public ASTNode
{
public:
	ASTNodePtr condition;
	ASTNodePtr body;
	WhileStatementNode(ASTNodePtr c, ASTNodePtr b)
		: condition(std::move(c))
		, body(std::move(b))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

struct Parameter
{
	std::string name;
};

class FunctionDeclNode : public ASTNode
{
public:
	std::string name;
	std::vector<Parameter> params;
	ASTNodePtr body;
	std::vector<ASTNodePtr> metadata;

	FunctionDeclNode(std::string n, std::vector<Parameter> p, ASTNodePtr b)
		: name(std::move(n))
		, params(std::move(p))
		, body(std::move(b))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class CallNode : public ASTNode
{
public:
	std::string callee;
	std::vector<ASTNodePtr> args;
	CallNode(std::string c, std::vector<ASTNodePtr> a)
		: callee(std::move(c))
		, args(std::move(a))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class VarDeclNode : public ASTNode
{
public:
	std::string name;
	ASTNodePtr initializer;
	VarDeclNode(std::string n, ASTNodePtr i)
		: name(std::move(n))
		, initializer(std::move(i))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class ReturnNode : public ASTNode
{
public:
	ASTNodePtr value;
	explicit ReturnNode(ASTNodePtr v = nullptr)
		: value(std::move(v))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class PrintNode : public ASTNode
{
public:
	ASTNodePtr value;
	explicit PrintNode(ASTNodePtr v)
		: value(std::move(v))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class ArrayLiteralNode : public ASTNode
{
public:
	std::vector<ASTNodePtr> elements;
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class IndexNode : public ASTNode
{
public:
	ASTNodePtr container;
	ASTNodePtr index;
	IndexNode(ASTNodePtr c, ASTNodePtr i)
		: container(std::move(c))
		, index(std::move(i))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class IterNode : public ASTNode
{
public:
	std::string varName;
	ASTNodePtr collection;
	ASTNodePtr body;
	IterNode(std::string n, ASTNodePtr coll, ASTNodePtr b)
		: varName(std::move(n))
		, collection(std::move(coll))
		, body(std::move(b))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class LeafNode : public ASTNode
{
public:
	std::string type;
	std::string value;
	LeafNode(std::string t, std::string v)
		: type(std::move(t))
		, value(std::move(v))
	{
	}
	void Accept(ASTVisitor& v) override {}
};

class RawNode : public ASTNode
{
public:
	std::string ruleName;
	std::vector<ASTNodePtr> children;
	explicit RawNode(std::string name)
		: ruleName(std::move(name))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class ComptimeNode : public ASTNode
{
public:
	ASTNodePtr body;
	explicit ComptimeNode(ASTNodePtr b)
		: body(std::move(b))
	{
	}
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};