#pragma once

#include "../ast/AST.h"
#include "../ast/SymbolTable.h"
#include "../vm/core/OpCode.h"
#include "../vm/execution/Chunk.h"

#include <unordered_map>
#include <vector>

class BytecodeGenerator : public ASTVisitor
{
public:
	VM::Execution::Chunk Compile(ASTNode* root);

	void Visit(IntegerLiteralNode& node) override;
	void Visit(FloatLiteralNode& node) override;
	void Visit(StringLiteralNode& node) override;
	void Visit(IdentifierNode& node) override;
	void Visit(UnaryExprNode& node) override;
	void Visit(BinaryExprNode& node) override;
	void Visit(AssignmentNode& node) override;
	void Visit(VarDeclNode& node) override;
	void Visit(TypeAliasNode& node) override;
	void Visit(StructDeclNode& node) override;
	void Visit(EnumDeclNode& node) override;
	void Visit(InterfaceDeclNode& node) override;
	void Visit(BlockNode& node) override;
	void Visit(ExportDeclNode& node) override;
	void Visit(IfStatementNode& node) override;
	void Visit(WhileStatementNode& node) override;
	void Visit(FunctionDeclNode& node) override;
	void Visit(FunctionExprNode& node) override;
	void Visit(CallNode& node) override;
	void Visit(MemberAccessNode& node) override;
	void Visit(ModuleDeclNode& node) override;
	void Visit(ImportDeclNode& node) override;
	void Visit(ReturnNode& node) override;
	void Visit(PrintNode& node) override;
	void Visit(UnsafeNode& node) override;
	void Visit(ArrayLiteralNode& node) override;
	void Visit(IndexNode& node) override;
	void Visit(IterNode& node) override;

	void Visit(ComptimeNode& node) override;
	void Visit(LeafNode& node) override;
	void Visit(RawNode& node) override;

private:
	struct UpvalueInfo
	{
		std::string name;
		bool sourceIsLocal = false;
		uint8_t sourceIndex = 0;
	};

	struct FunctionContext
	{
		struct InterfaceBinding
		{
			std::string interfaceName;
			std::string structType;
			enum class RuntimeKind
			{
				Unknown,
				Module,
				Function,
				Struct
			} runtimeKind = RuntimeKind::Unknown;
		};

		std::shared_ptr<VM::Core::Function> function;
		SymbolTable symbols;
		std::vector<UpvalueInfo> upvalues;
		std::unordered_map<std::string, uint8_t> upvalueLookup;
		std::vector<std::unordered_map<std::string, std::string>> structVarScopes;
		std::vector<std::unordered_map<std::string, std::string>> enumVarScopes;
		std::vector<std::unordered_map<std::string, InterfaceBinding>> interfaceVarScopes;
		std::optional<std::string> methodSelfStructType;
	};

	struct MetadataRegistry
	{
		struct EnumVariantInfo
		{
			uint8_t tag = 0;
			uint8_t argCount = 0;
			std::string enumName;
		};

		std::unordered_map<std::string, std::string> importAliases;
		std::unordered_map<std::string, std::vector<std::string>> structLayouts;
		std::unordered_map<std::string, std::unordered_map<std::string, std::string>> structFieldTypes;
		std::unordered_map<std::string, std::unordered_map<std::string, std::string>> structMethodNames;
		std::unordered_map<std::string, std::vector<std::string>> interfaceMethods;
		std::unordered_map<std::string, EnumVariantInfo> enumVariants;

		void Clear()
		{
			importAliases.clear();
			structLayouts.clear();
			structFieldTypes.clear();
			structMethodNames.clear();
			interfaceMethods.clear();
			enumVariants.clear();
		}
	};

	struct MetadataResolver
	{
		const BytecodeGenerator& generator;

		[[nodiscard]] std::optional<uint8_t> ResolveStructFieldIndex(const ASTNode* object, const std::string& member) const;
		[[nodiscard]] std::optional<std::string> InferStructTypeName(const ASTNode* node) const;
		[[nodiscard]] std::optional<std::string> ResolveStructVariableType(const std::string& name) const;
		[[nodiscard]] std::optional<std::string> InferEnumTypeName(const ASTNode* node) const;
		[[nodiscard]] std::optional<std::string> ResolveEnumVariableType(const std::string& name) const;
		[[nodiscard]] std::optional<FunctionContext::InterfaceBinding> ResolveInterfaceBinding(const std::string& name) const;
		[[nodiscard]] std::optional<FunctionContext::InterfaceBinding> InferInterfaceBinding(
			const ASTNode* node,
			const std::string& explicitType = "") const;
		[[nodiscard]] bool IsFunctionBackedInterfaceMethod(const ASTNode* object, const std::string& member) const;
		[[nodiscard]] std::optional<std::string> ResolveStructMethod(const ASTNode* object, const std::string& member) const;
		[[nodiscard]] bool IsEnumTagAccess(const ASTNode* object, const std::string& member) const;
	};

	struct AccessEmitter
	{
		BytecodeGenerator& generator;

		void EmitAssignment(const AssignmentNode& node) const;
		void EmitCall(const CallNode& node) const;
		void EmitMemberAccess(MemberAccessNode& node) const;

	private:
		void EmitCallArguments(const std::vector<ASTNodePtr>& args) const;
		bool TryEmitDirectTypeConstructorCall(const CallNode& node) const;
		bool TryEmitMemberCall(const CallNode& node) const;
	};

	void EmitBinaryOp(const std::string& op) const;
	[[nodiscard]] size_t EmitJump(VM::Core::OpCode opcode) const;
	void PatchJump(size_t jumpAddr) const;
	[[nodiscard]] VM::Execution::Chunk& CurrentChunk() const;
	[[nodiscard]] FunctionContext& CurrentContext();
	[[nodiscard]] const FunctionContext& CurrentContext() const;
	static void InitializeFunctionContext(FunctionContext& context, const std::string& name);
	static void PushContextScope(FunctionContext& context);
	static void PopContextScope(FunctionContext& context);
	[[nodiscard]] std::string QualifyName(const std::string& name) const;
	[[nodiscard]] std::optional<uint8_t> ResolveLocal(const std::string& name) const;
	[[nodiscard]] std::optional<uint8_t> ResolveUpvalue(size_t contextIndex, const std::string& name);
	[[nodiscard]] std::optional<uint8_t> ResolveSelfFieldIndex(const std::string& member) const;
	void EmitGetVariable(const std::string& name);
	void EmitSetVariable(const std::string& name);
	void EmitAddressOf(ASTNode* operand);
	void EmitFunctionObject(const std::shared_ptr<VM::Core::Function>& function, const std::vector<UpvalueInfo>& upvalues) const;
	void CompileFunctionBody(
		const std::string& name,
		const std::vector<Parameter>& params,
		ASTNode* body,
		const std::function<void()>& emitResult);
	[[nodiscard]] static std::string DefaultImportAlias(const std::string& qualifiedName);
	[[nodiscard]] std::optional<uint8_t> ResolveStructFieldIndex(const ASTNode* object, const std::string& member) const;
	[[nodiscard]] std::optional<std::string> InferStructTypeName(const ASTNode* node) const;
	[[nodiscard]] std::optional<std::string> ResolveStructVariableType(const std::string& name) const;
	[[nodiscard]] std::optional<std::string> InferEnumTypeName(const ASTNode* node) const;
	[[nodiscard]] std::optional<std::string> ResolveEnumVariableType(const std::string& name) const;
	[[nodiscard]] bool IsEnumTagAccess(const ASTNode* object, const std::string& member) const;
	[[nodiscard]] std::optional<FunctionContext::InterfaceBinding> ResolveInterfaceBinding(const std::string& name) const;
	[[nodiscard]] std::optional<FunctionContext::InterfaceBinding> InferInterfaceBinding(const ASTNode* node, const std::string& explicitType = "") const;
	[[nodiscard]] bool IsFunctionBackedInterfaceMethod(const ASTNode* object, const std::string& member) const;
	[[nodiscard]] std::optional<std::string> ResolveStructMethod(const ASTNode* object, const std::string& member) const;

	std::vector<FunctionContext> m_contexts;
	std::string m_currentModule;
	MetadataRegistry m_metadata;
	size_t m_lambdaCounter = 0;
};