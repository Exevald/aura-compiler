#pragma once
#include "AST.h"
#include <map>

class ASTBuilder
{
public:
	ASTNodePtr Build(ASTNodePtr node);

private:
	ASTNodePtr Simplify(ASTNodePtr node);
	static ASTNodePtr SimplifyLeaf(ASTNodePtr node, LeafNode& leaf);
	ASTNodePtr SimplifyRaw(ASTNodePtr node, RawNode& raw);

	ASTNodePtr SimplifyFuncDecl(RawNode& raw);
	ASTNodePtr SimplifyBlockChain(RawNode& raw);
	ASTNodePtr SimplifyVarDeclNoSemi(RawNode& raw);
	ASTNodePtr SimplifyBinaryChain(RawNode& raw);
	ASTNodePtr SimplifyIdentifierExpr(RawNode& raw);
	ASTNodePtr SimplifyIfStmt(RawNode& raw);
	ASTNodePtr SimplifyWhileStmt(RawNode& raw);
	ASTNodePtr SimplifyIterStmt(RawNode& raw);
	ASTNodePtr SimplifyArrayLit(RawNode& raw);
	ASTNodePtr SimplifyPrimaryComptime(RawNode& raw);
	ASTNodePtr SimplifyPrintStmt(RawNode& raw);

	static void FlattenParams(const RawNode* raw, std::vector<Parameter>& target);
	void FlattenArgs(RawNode* raw, std::vector<ASTNodePtr>& target);
};