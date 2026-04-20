#pragma once

#include "../AST.h"

class ASTBuilder
{
public:
	ASTNodePtr Build(ASTNodePtr node);

private:
	ASTNodePtr Simplify(ASTNodePtr node);
	static ASTNodePtr SimplifyLeaf(ASTNodePtr node, LeafNode& leaf);
	ASTNodePtr SimplifyRaw(ASTNodePtr node, RawNode& raw);

	ASTNodePtr SimplifyFuncDecl(RawNode& raw);
	ASTNodePtr SimplifyArrowFunc(const RawNode& raw);
	ASTNodePtr SimplifyBlockChain(RawNode& raw);
	ASTNodePtr SimplifyExportDecl(RawNode& raw);
	ASTNodePtr SimplifyStructDecl(RawNode& raw);
	static ASTNodePtr SimplifyEnumDecl(const RawNode& raw);
	static ASTNodePtr SimplifyInterfaceDecl(const RawNode& raw);
	static ASTNodePtr SimplifyEffectDecl(const RawNode& raw);
	ASTNodePtr SimplifyActorDecl(const RawNode& raw);
	ASTNodePtr SimplifyVarDeclNoSemi(const RawNode& raw);
	ASTNodePtr SimplifyConstDeclNoSemi(RawNode& raw);
	static ASTNodePtr SimplifyTypeAliasNoSemi(const RawNode& raw);
	ASTNodePtr SimplifyBinaryChain(RawNode& raw);
	ASTNodePtr SimplifyIdentifierExpr(const RawNode& raw);
	ASTNodePtr SimplifyAssignmentExpr(RawNode& raw);
	static ASTNodePtr SimplifyModuleDecl(const RawNode& raw);
	static ASTNodePtr SimplifyImportDecl(const RawNode& raw);
	ASTNodePtr SimplifyIfStmt(RawNode& raw);
	ASTNodePtr SimplifyWhileStmt(RawNode& raw);
	ASTNodePtr SimplifyIterStmt(RawNode& raw);
	ASTNodePtr SimplifyArrayLit(const RawNode& raw);
	ASTNodePtr SimplifyPrimaryComptime(RawNode& raw);
	ASTNodePtr SimplifyPrintStmt(RawNode& raw);
	ASTNodePtr SimplifyUnary(RawNode& raw);
	ASTNodePtr SimplifyUnsafeStmt(RawNode& raw);
	ASTNodePtr SimplifyTransactionStmt(RawNode& raw);
	ASTNodePtr SimplifyHandleStmt(RawNode& raw);
	StructMethodDecl SimplifyStructMethod(RawNode* raw);
	ActorMethodDecl SimplifyActorMethod(RawNode* raw);

	static void FlattenParams(const RawNode* raw, std::vector<Parameter>& target);
	static void FlattenTypeParams(const RawNode* raw, std::vector<TypeParameterDecl>& target);
	static void FlattenContextBindings(const RawNode* raw, std::vector<ContextBinding>& target);
	static void FlattenRaisedEffects(const RawNode* raw, std::vector<std::string>& target);
	static void FlattenStructFields(const RawNode* raw, std::vector<StructField>& target);
	void FlattenStructMembers(
		const RawNode* raw,
		std::vector<StructField>& fields,
		std::vector<StructMethodDecl>& methods);
	void FlattenActorBody(
		const RawNode* raw,
		std::vector<ActorFieldDecl>& fields,
		std::vector<ActorMethodDecl>& methods);
	static void FlattenEnumVariants(const RawNode* raw, std::vector<EnumVariantDecl>& target);
	static void FlattenInterfaceMethods(const RawNode* raw, std::vector<InterfaceMethodSig>& target);
	static void FlattenQualifiedTypeList(const RawNode* raw, std::vector<std::string>& target);
	void FlattenArgs(RawNode* raw, std::vector<ASTNodePtr>& target);
	void FlattenIterAdapters(const RawNode* raw, std::vector<IterAdapter>& target);
	void FlattenHandlerCases(RawNode* raw, std::vector<EffectHandlerCase>& target);
};
