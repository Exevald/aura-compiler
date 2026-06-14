#pragma once

#include "../AST.h"

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
		Any,
		Void,
		Bool,
		Int,
		Float,
		String,
		Never,
		Array,
		Map,
		Channel,
		Pointer,
		Ref,
		Function,
		Task,
		Module,
		Interface,
		Struct,
		Enum,
		EnumConstructor,
		TypeParameter,
		Actor,
		Effect
	};

	struct TypeInfo
	{
		TypeKind kind{ TypeKind::Unknown };
		std::shared_ptr<TypeInfo> element;
		std::shared_ptr<TypeInfo> key;

		std::vector<TypeInfo> params;
		std::shared_ptr<TypeInfo> ret;
		std::string name;
		size_t minArity = 0;
		bool variadic = false;
		std::shared_ptr<TypeInfo> variadicParam;
		std::shared_ptr<std::unordered_map<std::string, TypeInfo>> fields;
		std::vector<TypeInfo> variantArgs;
		int enumTag = -1;
		std::shared_ptr<std::unordered_map<std::string, TypeInfo>> methods;
		std::shared_ptr<std::unordered_set<std::string>> implementedInterfaces;
		std::shared_ptr<std::unordered_set<std::string>> raisedEffects;
		std::shared_ptr<std::vector<std::pair<std::string, TypeInfo>>> contextRequirements;
		std::vector<TypeInfo> constraints;
		bool isComptime = false;

		static TypeInfo Unknown() { return {}; }
		static TypeInfo Void() { return { TypeKind::Void }; }
		static TypeInfo Any() { return { TypeKind::Any }; }
		static TypeInfo Bool() { return { TypeKind::Bool }; }
		static TypeInfo Int() { return { TypeKind::Int }; }
		static TypeInfo Float() { return { TypeKind::Float }; }
		static TypeInfo String() { return { TypeKind::String }; }
		static TypeInfo Never() { return { TypeKind::Never }; }

		static TypeInfo Function(
			std::vector<TypeInfo> p,
			TypeInfo r,
			size_t requiredArity = static_cast<size_t>(-1),
			std::unordered_set<std::string> effects = {},
			std::vector<std::pair<std::string, TypeInfo>> contexts = {},
			bool comptimeOnly = false,
			bool isVariadic = false,
			std::optional<TypeInfo> variadicParameter = std::nullopt)
		{
			TypeInfo t;
			t.kind = TypeKind::Function;
			if (requiredArity == static_cast<size_t>(-1))
			{
				requiredArity = p.size();
			}
			t.params = std::move(p);
			t.minArity = requiredArity;
			t.variadic = isVariadic;
			t.isComptime = comptimeOnly;
			t.raisedEffects = std::make_shared<std::unordered_set<std::string>>(std::move(effects));
			t.contextRequirements = std::make_shared<std::vector<std::pair<std::string, TypeInfo>>>(std::move(contexts));
			if (variadicParameter.has_value())
			{
				t.variadicParam = std::make_shared<TypeInfo>(std::move(*variadicParameter));
			}
			t.ret = std::make_shared<TypeInfo>(std::move(r));
			return t;
		}

		static TypeInfo TaskOf(TypeInfo valueType)
		{
			TypeInfo t;
			t.kind = TypeKind::Task;
			t.element = std::make_shared<TypeInfo>(std::move(valueType));
			return t;
		}

		static TypeInfo ArrayOf(TypeInfo elem)
		{
			TypeInfo t;
			t.kind = TypeKind::Array;
			t.element = std::make_shared<TypeInfo>(std::move(elem));
			return t;
		}

		static TypeInfo MapOf(TypeInfo keyType, TypeInfo valueType)
		{
			TypeInfo t;
			t.kind = TypeKind::Map;
			t.key = std::make_shared<TypeInfo>(std::move(keyType));
			t.element = std::make_shared<TypeInfo>(std::move(valueType));
			return t;
		}

		static TypeInfo ChannelOf(TypeInfo elem)
		{
			TypeInfo t;
			t.kind = TypeKind::Channel;
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

		static TypeInfo RefTo(TypeInfo referent)
		{
			TypeInfo t;
			t.kind = TypeKind::Ref;
			t.element = std::make_shared<TypeInfo>(std::move(referent));
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
			std::unordered_map<std::string, TypeInfo> structMethods = {},
			std::unordered_set<std::string> interfaces = {})
		{
			TypeInfo t;
			t.kind = TypeKind::Struct;
			t.name = std::move(structName);
			t.fields = std::make_shared<std::unordered_map<std::string, TypeInfo>>(std::move(structFields));
			t.methods = std::make_shared<std::unordered_map<std::string, TypeInfo>>(std::move(structMethods));
			t.implementedInterfaces = std::make_shared<std::unordered_set<std::string>>(std::move(interfaces));
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

		static TypeInfo Actor(
			std::string actorName,
			std::unordered_map<std::string, TypeInfo> actorFields,
			std::unordered_map<std::string, TypeInfo> actorMethods)
		{
			TypeInfo t;
			t.kind = TypeKind::Actor;
			t.name = std::move(actorName);
			t.fields = std::make_shared<std::unordered_map<std::string, TypeInfo>>(std::move(actorFields));
			t.methods = std::make_shared<std::unordered_map<std::string, TypeInfo>>(std::move(actorMethods));
			return t;
		}

		static TypeInfo Effect(
			std::string effectName,
			std::unordered_map<std::string, TypeInfo> effectOps)
		{
			TypeInfo t;
			t.kind = TypeKind::Effect;
			t.name = std::move(effectName);
			t.methods = std::make_shared<std::unordered_map<std::string, TypeInfo>>(std::move(effectOps));
			return t;
		}

		[[nodiscard]] bool Equals(const TypeInfo& other) const
		{
			if (kind != other.kind)
			{
				return false;
			}
			if (kind == TypeKind::Array || kind == TypeKind::Channel || kind == TypeKind::Pointer
				|| kind == TypeKind::Ref || kind == TypeKind::Task)
			{
				if (!element || !other.element)
				{
					return element == other.element;
				}
				return element->Equals(*other.element);
			}
			if (kind == TypeKind::Map)
			{
				if (!key || !other.key || !element || !other.element)
				{
					return key == other.key && element == other.element;
				}
				return key->Equals(*other.key) && element->Equals(*other.element);
			}
			if (kind == TypeKind::Function)
			{
				if (variadic != other.variadic)
				{
					return false;
				}
				if (variadic)
				{
					if (!variadicParam || !other.variadicParam)
					{
						if (variadicParam != other.variadicParam)
						{
							return false;
						}
					}
					else if (!variadicParam->Equals(*other.variadicParam))
					{
						return false;
					}
				}
				if (params.size() != other.params.size())
				{
					return false;
				}
				if (minArity != other.minArity || isComptime != other.isComptime)
				{
					return false;
				}
				if ((!contextRequirements || !other.contextRequirements)
						? (contextRequirements != other.contextRequirements)
						: (contextRequirements->size() != other.contextRequirements->size()))
				{
					return false;
				}
				if (contextRequirements && other.contextRequirements)
				{
					for (size_t i = 0; i < contextRequirements->size(); ++i)
					{
						if ((*contextRequirements)[i].first != (*other.contextRequirements)[i].first
							|| !(*contextRequirements)[i].second.Equals((*other.contextRequirements)[i].second))
						{
							return false;
						}
					}
				}
				for (size_t i = 0; i < params.size(); ++i)
				{
					if (!params[i].Equals(other.params[i]))
					{
						return false;
					}
				}
				if (!raisedEffects || !other.raisedEffects
						? raisedEffects != other.raisedEffects
						: *raisedEffects != *other.raisedEffects)
				{
					return false;
				}
				if (!ret || !other.ret)
				{
					return ret == other.ret;
				}
				return ret->Equals(*other.ret);
			}
			if (kind == TypeKind::Struct
				|| kind == TypeKind::Module
				|| kind == TypeKind::Enum
				|| kind == TypeKind::Interface
				|| kind == TypeKind::Actor
				|| kind == TypeKind::Effect)
			{
				return name == other.name;
			}
			if (kind == TypeKind::TypeParameter)
			{
				return name == other.name;
			}
			if (kind == TypeKind::EnumConstructor)
			{
				if (name != other.name
					|| enumTag != other.enumTag
					|| variantArgs.size() != other.variantArgs.size())
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

	[[nodiscard]] bool HasVariadicParameter(const std::vector<Parameter>& params) const;
	[[nodiscard]] const Parameter* FindVariadicParameter(const std::vector<Parameter>& params) const;
	[[nodiscard]] std::vector<TypeInfo> BuildFixedParamTypes(const std::vector<Parameter>& params) const;
	[[nodiscard]] std::optional<TypeInfo> BuildVariadicParamType(const std::vector<Parameter>& params) const;
	[[nodiscard]] TypeInfo DeclaredParameterType(const Parameter& param) const;
	void ValidateParameterList(const std::vector<Parameter>& params, const std::string& ownerName) const;

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

	struct GenericNominalInfo
	{
		std::vector<TypeParameterDecl> typeParams;
		TypeInfo templatedType;
	};

	struct TypeRegistry
	{
		struct ConstructorSignature
		{
			std::vector<std::pair<std::string, TypeInfo>> params;
			size_t requiredArity = 0;
		};

		std::unordered_map<std::string, TypeInfo> structs;
		std::unordered_map<std::string, TypeInfo> enums;
		std::unordered_map<std::string, TypeInfo> enumConstructors;
		std::unordered_map<std::string, TypeInfo> interfaces;
		std::unordered_map<std::string, TypeInfo> actors;
		std::unordered_map<std::string, TypeInfo> effects;
		std::unordered_map<std::string, ConstructorSignature> constructorSignatures;
		std::unordered_map<std::string, std::string> aliases;
		std::unordered_map<std::string, GenericAliasInfo> genericAliases;
		std::unordered_map<std::string, GenericNominalInfo> genericStructs;
		std::unordered_map<std::string, GenericNominalInfo> genericEnums;
		std::unordered_map<std::string, GenericNominalInfo> genericInterfaces;
		std::unordered_map<std::string, GenericNominalInfo> genericActors;

		void Clear()
		{
			structs.clear();
			enums.clear();
			enumConstructors.clear();
			interfaces.clear();
			actors.clear();
			effects.clear();
			constructorSignatures.clear();
			aliases.clear();
			genericAliases.clear();
			genericStructs.clear();
			genericEnums.clear();
			genericInterfaces.clear();
			genericActors.clear();
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
		void ValidateContextRequirements(const TypeInfo& funcType) const;
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
	void ValidateRefArgument(const TypeInfo& expectedRef, ASTNode* arg, size_t argIndex);
	[[nodiscard]] TypeInfo AddressableValueType(ASTNode* node);
	[[nodiscard]] bool IsConstAddressable(ASTNode* node) const;
	[[nodiscard]] bool IsAssignable(const TypeInfo& expected, const TypeInfo& actual) const;
	[[nodiscard]] static bool IsFunctionLikeInterface(const TypeInfo& interfaceType);
	[[nodiscard]] std::optional<TypeInfo> ResolveModuleMemberType(const std::string& moduleName, const std::string& member) const;
	[[nodiscard]] TypeInfo InferFunctionBodyReturnType(ASTNode* body);
	[[nodiscard]] bool IsModuleMemberExported(const std::string& moduleName, const std::string& member) const;
	[[nodiscard]] bool ModuleDefinesMember(const std::string& moduleName, const std::string& member) const;
	[[nodiscard]] bool CurrentContextAllowsEffect(const std::string& effectName) const;
	[[nodiscard]] std::vector<std::string> ResolveAccessibleEffectsForOperation(const std::string& operationName) const;
	void ValidateContractList(
		const std::vector<std::unique_ptr<ContractNode>>& contracts,
		const std::optional<TypeInfo>& ensuresResult = std::nullopt,
		const std::optional<TypeInfo>& selfType = std::nullopt);
	void RejectRuntimeOnlyInComptime(const std::string& what) const;
	[[nodiscard]] bool IsUnsafeMemoryCall(const CallNode& node);
	[[nodiscard]] bool HasActiveTransaction(const std::string& regionName) const;
	[[nodiscard]] bool IsCurrentActorState(const std::string& name) const;
	[[nodiscard]] static bool IsSendable(const TypeInfo& type);
	void ValidateSendable(const TypeInfo& type, const std::string& what) const;

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
	void RegisterBuiltinTypes();
	void RegisterBuiltinModules();

	TypeInfo m_lastType;
	EnvironmentState m_environment;
	TypeInfo m_currentExpectedReturn = TypeInfo::Unknown();
	TypeRegistry m_types;
	ModuleRegistry m_modules;
	std::string m_currentModule;
	std::vector<std::unordered_map<std::string, TypeInfo>> m_typeParamScopes;
	std::vector<std::unordered_set<std::string>> m_activeHandledEffects;
	std::vector<std::unordered_set<std::string>> m_activeRaisedEffects;
	std::vector<TypeInfo> m_activeResumeTypes;
	std::vector<std::unordered_set<std::string>> m_actorStateScopes;
	std::vector<std::unordered_set<std::string>> m_activeTransactions;
	std::unordered_set<std::string> m_sharedVariables;
	int m_actorQueryDepth = 0;
	int m_unsafeDepth = 0;
	int m_comptimeDepth = 0;
	int m_functionDepth = 0;

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
	void Visit(EffectDeclNode& node) override;
	void Visit(ActorDeclNode& node) override;
	void Visit(BlockNode& node) override;
	void Visit(ExportDeclNode& node) override;
	void Visit(IfStatementNode& node) override;
	void Visit(WhileStatementNode& node) override;
	void Visit(FunctionDeclNode& node) override;
	void Visit(FunctionExprNode& node) override;
	void Visit(CallNode& node) override;
	void Visit(GoExprNode& node) override;
	void Visit(AwaitExprNode& node) override;
	void Visit(MemberAccessNode& node) override;
	void Visit(ModuleDeclNode& node) override;
	void Visit(ImportDeclNode& node) override;
	void Visit(ReturnNode& node) override;
	void Visit(PrintNode& node) override;
	void Visit(UnsafeNode& node) override;
	void Visit(ArrayLiteralNode& node) override;
	void Visit(MapLiteralNode& node) override;
	void Visit(IndexNode& node) override;
	void Visit(IterNode& node) override;
	void Visit(TransactionNode& node) override;
	void Visit(HandleNode& node) override;
	void Visit(LeafNode& node) override;
	void Visit(RawNode& node) override;
	void Visit(ComptimeNode& node) override;
	void Visit(ContractNode& node) override;
};
