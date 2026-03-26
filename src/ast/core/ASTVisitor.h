#pragma once

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
class ComptimeNode;

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
