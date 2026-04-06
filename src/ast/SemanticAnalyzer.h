#pragma once

#include "AST.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class SemanticAnalyzer : public ASTVisitor
{
public:
	void Analyze(ASTNode* root);

private:
	enum class TypeKind
	{
		Unknown,
		Void,
		Bool,
		Int,
		Float,
		String,
		Array,
		Pointer,
		Function,
		Module,
		Interface,
		Struct,
		Enum,
		EnumConstructor,
		TypeParameter
	};

	struct TypeInfo
	{
		TypeKind kind{ TypeKind::Unknown };
		std::shared_ptr<TypeInfo> element;

		std::vector<TypeInfo> params;
		std::shared_ptr<TypeInfo> ret;
		std::string name;
		std::shared_ptr<std::unordered_map<std::string, TypeInfo>> fields;
		std::vector<TypeInfo> variantArgs;
		int enumTag = -1;
		std::shared_ptr<std::unordered_map<std::string, TypeInfo>> methods;
		std::vector<TypeInfo> constraints;

		static TypeInfo Unknown() { return {}; }
		static TypeInfo Void() { return { TypeKind::Void }; }
		static TypeInfo Bool() { return { TypeKind::Bool }; }
		static TypeInfo Int() { return { TypeKind::Int }; }
		static TypeInfo Float() { return { TypeKind::Float }; }
		static TypeInfo String() { return { TypeKind::String }; }

		static TypeInfo Function(std::vector<TypeInfo> p, TypeInfo r)
		{
			TypeInfo t;
			t.kind = TypeKind::Function;
			t.params = std::move(p);
			t.ret = std::make_shared<TypeInfo>(std::move(r));
			return t;
		}

		static TypeInfo ArrayOf(TypeInfo elem)
		{
			TypeInfo t;
			t.kind = TypeKind::Array;
			t.element = std::make_shared<TypeInfo>(std::move(elem));
			return t;
		}

		static TypeInfo PointerTo(TypeInfo pointee)
		{
			TypeInfo t;
			t.kind = TypeKind::Pointer;
			t.element = std::make_shared<TypeInfo>(std::move(pointee));
			return t;
		}

		static TypeInfo Module()
		{
			return { TypeKind::Module };
		}

		static TypeInfo Module(std::string moduleName)
		{
			TypeInfo t;
			t.kind = TypeKind::Module;
			t.name = std::move(moduleName);
			return t;
		}

		static TypeInfo Interface(std::string interfaceName)
		{
			TypeInfo t;
			t.kind = TypeKind::Interface;
			t.name = std::move(interfaceName);
			return t;
		}

		static TypeInfo Interface(
			std::string interfaceName,
			std::unordered_map<std::string, TypeInfo> interfaceMethods)
		{
			TypeInfo t;
			t.kind = TypeKind::Interface;
			t.name = std::move(interfaceName);
			t.methods = std::make_shared<std::unordered_map<std::string, TypeInfo>>(std::move(interfaceMethods));
			return t;
		}

		static TypeInfo Struct(
			std::string structName,
			std::unordered_map<std::string, TypeInfo> structFields,
			std::unordered_map<std::string, TypeInfo> structMethods = {})
		{
			TypeInfo t;
			t.kind = TypeKind::Struct;
			t.name = std::move(structName);
			t.fields = std::make_shared<std::unordered_map<std::string, TypeInfo>>(std::move(structFields));
			t.methods = std::make_shared<std::unordered_map<std::string, TypeInfo>>(std::move(structMethods));
			return t;
		}

		static TypeInfo Enum(std::string enumName)
		{
			TypeInfo t;
			t.kind = TypeKind::Enum;
			t.name = std::move(enumName);
			return t;
		}

		static TypeInfo EnumConstructor(std::string enumName, int tag, std::vector<TypeInfo> args)
		{
			TypeInfo t;
			t.kind = TypeKind::EnumConstructor;
			t.name = std::move(enumName);
			t.enumTag = tag;
			t.variantArgs = std::move(args);
			return t;
		}

		static TypeInfo TypeParameter(std::string paramName, std::vector<TypeInfo> paramConstraints = {})
		{
			TypeInfo t;
			t.kind = TypeKind::TypeParameter;
			t.name = std::move(paramName);
			t.constraints = std::move(paramConstraints);
			return t;
		}

		[[nodiscard]] bool Equals(const TypeInfo& other) const
		{
			if (kind != other.kind)
			{
				return false;
			}
			if (kind == TypeKind::Array || kind == TypeKind::Pointer)
			{
				if (!element || !other.element)
				{
					return element == other.element;
				}
				return element->Equals(*other.element);
			}
			if (kind == TypeKind::Function)
			{
				if (params.size() != other.params.size())
				{
					return false;
				}
				for (size_t i = 0; i < params.size(); ++i)
				{
					if (!params[i].Equals(other.params[i]))
					{
						return false;
					}
				}
				if (!ret || !other.ret)
				{
					return ret == other.ret;
				}
				return ret->Equals(*other.ret);
			}
			if (kind == TypeKind::Struct || kind == TypeKind::Module || kind == TypeKind::Enum || kind == TypeKind::Interface)
			{
				return name == other.name;
			}
			if (kind == TypeKind::TypeParameter)
			{
				return name == other.name;
			}
			if (kind == TypeKind::EnumConstructor)
			{
				if (name != other.name || enumTag != other.enumTag || variantArgs.size() != other.variantArgs.size())
				{
					return false;
				}
				for (size_t i = 0; i < variantArgs.size(); ++i)
				{
					if (!variantArgs[i].Equals(other.variantArgs[i]))
					{
						return false;
					}
				}
				return true;
			}
			return true;
		}
	};

	struct SymbolInfo
	{
		TypeInfo type;
		bool isConst = false;
	};

	using Env = std::unordered_map<std::string, SymbolInfo>;

	struct EnvironmentState
	{
		std::vector<Env> scopes;
		std::vector<TypeInfo> structMethodSelfStack;

		void Reset();
		void PushScope();
		void PopScope();
		void PushStructMethodSelf(TypeInfo selfType);
		void PopStructMethodSelf();
		void Define(const std::string& name, TypeInfo type, bool isConst = false);
		void EnsureDeclared(const std::string& name) const;
		[[nodiscard]] TypeInfo Resolve(const std::string& name) const;
		[[nodiscard]] bool ResolveIsConst(const std::string& name) const;
	};

	struct GenericAliasInfo
	{
		std::vector<TypeParameterDecl> typeParams;
		std::string aliasedType;
	};

	struct TypeRegistry
	{
		std::unordered_map<std::string, TypeInfo> structs;
		std::unordered_map<std::string, TypeInfo> enums;
		std::unordered_map<std::string, TypeInfo> enumConstructors;
		std::unordered_map<std::string, TypeInfo> interfaces;
		std::unordered_map<std::string, std::string> aliases;
		std::unordered_map<std::string, GenericAliasInfo> genericAliases;

		void Clear()
		{
			structs.clear();
			enums.clear();
			enumConstructors.clear();
			interfaces.clear();
			aliases.clear();
			genericAliases.clear();
		}
	};

	struct ModuleRegistry
	{
		std::unordered_map<std::string, std::unordered_map<std::string, TypeInfo>> memberTypes;
		std::unordered_map<std::string, std::unordered_set<std::string>> exports;

		void Clear()
		{
			memberTypes.clear();
			exports.clear();
		}
	};

	struct TypeResolver
	{
		const SemanticAnalyzer& analyzer;

		[[nodiscard]] TypeInfo ResolveTypeName(const std::string& name) const;
		[[nodiscard]] static TypeInfo Substitute(
			const TypeInfo& type,
			const std::unordered_map<std::string, TypeInfo>& bindings);
		[[nodiscard]] bool Satisfies(
			const TypeInfo& actual,
			const std::vector<TypeInfo>& constraints) const;
		[[nodiscard]] bool Bind(
			const TypeInfo& expected,
			const TypeInfo& actual,
			std::unordered_map<std::string, TypeInfo>& bindings) const;
	};

	struct CallAnalyzer
	{
		SemanticAnalyzer& analyzer;

		void Analyze(const CallNode& node) const;

	private:
		void ValidateTypeCompatibility(const TypeInfo& expected, const TypeInfo& actual, const std::string& error) const;
		void AnalyzeStructConstructorCall(const TypeInfo& funcType, const std::vector<ASTNodePtr>& args) const;
		void AnalyzeEnumConstructorCall(const TypeInfo& funcType, const std::vector<ASTNodePtr>& args) const;
		void AnalyzeFunctionCall(const TypeInfo& funcType, const std::vector<ASTNodePtr>& args) const;
	};

	TypeInfo AnalyzeExpr(ASTNode* node);

	static bool IsPrimitiveNumeric(const TypeInfo& t);
	static bool IsTruthyBinaryOp(const std::string& op);
	void EnsureDeclared(const std::string& name) const;
	[[nodiscard]] TypeInfo Resolve(const std::string& name) const;
	[[nodiscard]] bool ResolveIsConst(const std::string& name) const;
	void Define(const std::string& name, TypeInfo type, bool isConst = false);

	void AnalyzeTypeForBinaryOp(const std::string& op, const TypeInfo& lhs, const TypeInfo& rhs);
	void AnalyzeAssignment(ASTNode* valueExpr, const std::string& name, ASTNode* indexExpr, bool hasIndex);
	[[nodiscard]] bool IsAssignable(const TypeInfo& expected, const TypeInfo& actual) const;
	[[nodiscard]] static bool IsFunctionLikeInterface(const TypeInfo& interfaceType);
	[[nodiscard]] std::optional<TypeInfo> ResolveModuleMemberType(const std::string& moduleName, const std::string& member) const;
	[[nodiscard]] TypeInfo InferFunctionBodyReturnType(ASTNode* body);
	[[nodiscard]] bool IsModuleMemberExported(const std::string& moduleName, const std::string& member) const;
	[[nodiscard]] bool ModuleDefinesMember(const std::string& moduleName, const std::string& member) const;

	[[nodiscard]] TypeInfo StringToType(const std::string& name) const;
	[[nodiscard]] std::string QualifyName(const std::string& name) const;
	[[nodiscard]] TypeInfo SubstituteTypeParameters(
		const TypeInfo& type,
		const std::unordered_map<std::string, TypeInfo>& bindings) const;
	[[nodiscard]] bool BindGenericType(
		const TypeInfo& expected,
		const TypeInfo& actual,
		std::unordered_map<std::string, TypeInfo>& bindings) const;
	[[nodiscard]] bool SatisfiesConstraints(const TypeInfo& actual, const std::vector<TypeInfo>& constraints) const;
	void RegisterBuiltinModules();

	TypeInfo m_lastType;
	EnvironmentState m_environment;
	TypeInfo m_currentExpectedReturn = TypeInfo::Unknown();
	TypeRegistry m_types;
	ModuleRegistry m_modules;
	std::string m_currentModule;
	std::vector<std::unordered_map<std::string, TypeInfo>> m_typeParamScopes;
	int m_unsafeDepth = 0;

	TypeInfo VisitAndGet(ASTNode* node);

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
	void Visit(LeafNode& node) override;
	void Visit(RawNode& node) override;
	void Visit(ComptimeNode& node) override;
};
