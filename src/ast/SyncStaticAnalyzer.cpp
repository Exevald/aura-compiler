#include "SyncStaticAnalyzer.h"

#include "AST.h"
#include "SyncRustBridge.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

std::string DefaultImportAlias(const std::string& qualifiedName)
{
	const auto pos = qualifiedName.find_last_of('.');
	return pos == std::string::npos ? qualifiedName : qualifiedName.substr(pos + 1);
}

class Analyzer
{
public:
	void Analyze(ASTNode* root)
	{
		Reset();
		PushScope();
		CollectCallables(root);
		m_scopes.clear();
		PushScope();
		m_importAliases.clear();
		m_currentModule.clear();
		AnalyzeNode(root);
	}

private:
	enum class ResourceKind
	{
		Unknown,
		Thread,
		Mutex
	};

	struct ResourceInfo
	{
		ResourceKind kind = ResourceKind::Unknown;
		std::string id;
	};

	struct ResourceRef
	{
		ResourceKind kind = ResourceKind::Unknown;
		std::string name;
		bool isParameter = false;
	};

	struct SummaryOp
	{
		enum class Kind
		{
			Lock,
			Unlock,
			Join,
			Call
		} kind;

		ResourceRef first;
		ResourceRef second;
		std::string calleeName;
		std::vector<ResourceRef> callArgs;
	};

	struct CallableSummary
	{
		std::string name;
		std::vector<std::string> params;
		std::vector<SummaryOp> ops;
	};

	struct Scope
	{
		std::unordered_map<std::string, ResourceInfo> resources;
		std::unordered_map<std::string, std::string> callables;
	};

	struct SummaryBuildState
	{
		std::string callableName;
		std::unordered_set<std::string> parameterNames;
		std::vector<SummaryOp> ops;
	};

	using HeldMap = std::unordered_map<std::string, std::unordered_set<std::string>>;

	void Reset()
	{
		m_importAliases.clear();
		m_scopes.clear();
		m_threadHeldLocks.clear();
		m_functionStack.clear();
		m_callables.clear();
		m_lambdaCallableIds.clear();
		m_summaryBuildStack.clear();
		m_activeSummaryCalls.clear();
		m_nextThreadId = 0;
		m_nextMutexId = 0;
		m_nextLambdaId = 0;
	}

	void PushScope()
	{
		m_scopes.emplace_back();
	}

	void PopScope()
	{
		if (!m_scopes.empty())
		{
			m_scopes.pop_back();
		}
	}

	void DefineResource(const std::string& name, ResourceInfo info)
	{
		if (m_scopes.empty())
		{
			PushScope();
		}
		m_scopes.back().resources[name] = std::move(info);
	}

	void DefineCallable(const std::string& name, const std::string& callableId)
	{
		if (m_scopes.empty())
		{
			PushScope();
		}
		m_scopes.back().callables[name] = callableId;
	}

	[[nodiscard]] ResourceInfo ResolveResource(const std::string& name) const
	{
		for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it)
		{
			if (const auto found = it->resources.find(name); found != it->resources.end())
			{
				return found->second;
			}
		}
		return {};
	}

	[[nodiscard]] std::optional<std::string> ResolveCallable(const std::string& name) const
	{
		for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it)
		{
			if (const auto found = it->callables.find(name); found != it->callables.end())
			{
				return found->second;
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] std::string CurrentContext() const
	{
		if (m_functionStack.empty())
		{
			return "top_level";
		}
		return m_functionStack.back();
	}

	[[nodiscard]] bool IsSyncAlias(const std::string& alias) const
	{
		if (const auto it = m_importAliases.find(alias); it != m_importAliases.end())
		{
			return it->second == "std.sync";
		}
		return false;
	}

	[[nodiscard]] std::string QualifyCallableName(const std::string& name) const
	{
		if (name.find('.') != std::string::npos)
		{
			return name;
		}
		if (!m_currentModule.empty())
		{
			return m_currentModule + "." + name;
		}
		return name;
	}

	struct SyncCallInfo
	{
		std::string method;
	};

	[[nodiscard]] std::optional<SyncCallInfo> GetSyncCallInfo(const CallNode& call) const
	{
		const auto* member = dynamic_cast<MemberAccessNode*>(call.callee.get());
		if (!member)
		{
			return std::nullopt;
		}

		const auto* object = dynamic_cast<IdentifierNode*>(member->object.get());
		if (!object || !IsSyncAlias(object->name))
		{
			return std::nullopt;
		}

		return SyncCallInfo{ member->member };
	}

	[[nodiscard]] std::string NewThreadId()
	{
		return "thread#" + std::to_string(++m_nextThreadId);
	}

	[[nodiscard]] std::string NewMutexId()
	{
		return "mutex#" + std::to_string(++m_nextMutexId);
	}

	[[nodiscard]] std::string NewLambdaId()
	{
		return "<lambda#" + std::to_string(++m_nextLambdaId) + ">";
	}

	[[nodiscard]] ResourceInfo InferResource(ASTNode* node)
	{
		if (!node)
		{
			return {};
		}

		if (const auto* identifier = dynamic_cast<IdentifierNode*>(node))
		{
			return ResolveResource(identifier->name);
		}

		if (const auto* call = dynamic_cast<CallNode*>(node))
		{
			if (const auto syncCall = GetSyncCallInfo(*call))
			{
				if (syncCall->method == "spawn")
				{
					return { ResourceKind::Thread, NewThreadId() };
				}
				if (syncCall->method == "current_thread")
				{
					return { ResourceKind::Thread, "main_thread" };
				}
				if (syncCall->method == "mutex")
				{
					return { ResourceKind::Mutex, NewMutexId() };
				}
			}
		}

		return {};
	}

	[[nodiscard]] ResourceRef MakeRef(ASTNode* node) const
	{
		if (!node)
		{
			return {};
		}

		if (const auto* identifier = dynamic_cast<IdentifierNode*>(node))
		{
			const bool isParam = !m_summaryBuildStack.empty()
				&& m_summaryBuildStack.back().parameterNames.contains(identifier->name);
			if (isParam)
			{
				return { ResourceKind::Unknown, identifier->name, true };
			}

			const auto resource = ResolveResource(identifier->name);
			return { resource.kind, resource.id, false };
		}

		const auto resource = const_cast<Analyzer*>(this)->InferResource(node);
		return { resource.kind, resource.id, false };
	}

	void CollectCallables(ASTNode* node)
	{
		if (!node)
		{
			return;
		}

		if (auto* block = dynamic_cast<BlockNode*>(node))
		{
			for (auto& statement : block->statements)
			{
				CollectCallables(statement.get());
			}
			return;
		}

		if (auto* importDecl = dynamic_cast<ImportDeclNode*>(node))
		{
			const std::string alias = importDecl->alias.empty() ? DefaultImportAlias(importDecl->qualifiedName) : importDecl->alias;
			m_importAliases[alias] = importDecl->qualifiedName;
			return;
		}

		if (auto* moduleDecl = dynamic_cast<ModuleDeclNode*>(node))
		{
			m_currentModule = moduleDecl->qualifiedName;
			m_scopes.clear();
			PushScope();
			m_importAliases.clear();
			return;
		}

		if (auto* exportDecl = dynamic_cast<ExportDeclNode*>(node))
		{
			if (exportDecl->declaration)
			{
				CollectCallables(exportDecl->declaration.get());
			}
			return;
		}

		if (auto* functionDecl = dynamic_cast<FunctionDeclNode*>(node))
		{
			BuildNamedFunctionSummary(*functionDecl);
			return;
		}

		if (const auto* varDecl = dynamic_cast<VarDeclNode*>(node))
		{
			if (auto* functionExpr = dynamic_cast<FunctionExprNode*>(varDecl->initializer.get()))
			{
				const std::string lambdaId = NewLambdaId();
				BuildLambdaSummary(lambdaId, *functionExpr);
				DefineCallable(varDecl->name, lambdaId);
			}
		}
	}

	void BuildNamedFunctionSummary(const FunctionDeclNode& functionDecl)
	{
		const std::string qualifiedName = QualifyCallableName(functionDecl.name);
		const auto existing = m_callables.find(qualifiedName);
		if (existing != m_callables.end())
		{
			return;
		}

		CallableSummary summary;
		summary.name = qualifiedName;
		for (const auto& param : functionDecl.params)
		{
			summary.params.push_back(param.name);
		}
		m_callables.emplace(qualifiedName, std::move(summary));

		auto& stored = m_callables.at(qualifiedName);
		m_summaryBuildStack.push_back({ qualifiedName, {}, {} });
		for (const auto& param : functionDecl.params)
		{
			m_summaryBuildStack.back().parameterNames.insert(param.name);
		}

		PushScope();
		for (const auto& param : functionDecl.params)
		{
			DefineResource(param.name, {});
		}
		m_functionStack.push_back(qualifiedName);
		BuildSummaryNode(functionDecl.body.get());
		m_functionStack.pop_back();
		PopScope();

		stored.ops = std::move(m_summaryBuildStack.back().ops);
		m_summaryBuildStack.pop_back();
		DefineCallable(functionDecl.name, qualifiedName);
	}

	void BuildLambdaSummary(const std::string& lambdaId, const FunctionExprNode& functionExpr)
	{
		m_lambdaCallableIds[&functionExpr] = lambdaId;

		CallableSummary summary;
		summary.name = lambdaId;
		for (const auto& [name, typeName] : functionExpr.params)
		{
			summary.params.push_back(name);
		}
		m_callables.emplace(lambdaId, std::move(summary));

		auto& stored = m_callables.at(lambdaId);
		m_summaryBuildStack.push_back({ lambdaId, {}, {} });
		for (const auto& param : functionExpr.params)
		{
			m_summaryBuildStack.back().parameterNames.insert(param.name);
		}

		PushScope();
		for (const auto& [name, typeName] : functionExpr.params)
		{
			DefineResource(name, {});
		}
		m_functionStack.push_back(lambdaId);
		BuildSummaryNode(functionExpr.body.get());
		m_functionStack.pop_back();
		PopScope();

		stored.ops = std::move(m_summaryBuildStack.back().ops);
		m_summaryBuildStack.pop_back();
	}

	void BuildSummaryNode(ASTNode* node)
	{
		if (!node)
		{
			return;
		}

		if (const auto* block = dynamic_cast<BlockNode*>(node))
		{
			PushScope();
			for (auto& statement : block->statements)
			{
				BuildSummaryNode(statement.get());
			}
			PopScope();
			return;
		}
		if (const auto* exportDecl = dynamic_cast<ExportDeclNode*>(node))
		{
			if (exportDecl->declaration)
			{
				BuildSummaryNode(exportDecl->declaration.get());
			}
			return;
		}
		if (const auto* moduleDecl = dynamic_cast<ModuleDeclNode*>(node))
		{
			m_currentModule = moduleDecl->qualifiedName;
			m_scopes.clear();
			PushScope();
			m_importAliases.clear();
			return;
		}
		if (auto* varDecl = dynamic_cast<VarDeclNode*>(node))
		{
			if (auto* lambda = dynamic_cast<FunctionExprNode*>(varDecl->initializer.get()))
			{
				const std::string lambdaId = NewLambdaId();
				BuildLambdaSummary(lambdaId, *lambda);
				DefineCallable(varDecl->name, lambdaId);
			}
			else
			{
				BuildSummaryNode(varDecl->initializer.get());
			}
			return;
		}
		if (auto* assignment = dynamic_cast<AssignmentNode*>(node))
		{
			BuildSummaryNode(assignment->value.get());
			if (!assignment->name.empty())
			{
				if (auto* lambda = dynamic_cast<FunctionExprNode*>(assignment->value.get()))
				{
					const std::string lambdaId = NewLambdaId();
					BuildLambdaSummary(lambdaId, *lambda);
					DefineCallable(assignment->name, lambdaId);
				}
				else if (auto* identifier = dynamic_cast<IdentifierNode*>(assignment->value.get()))
				{
					if (const auto callable = ResolveCallable(identifier->name))
					{
						DefineCallable(assignment->name, *callable);
					}
				}
			}
			return;
		}
		if (auto* ifStmt = dynamic_cast<IfStatementNode*>(node))
		{
			BuildSummaryNode(ifStmt->condition.get());
			BuildSummaryNode(ifStmt->thenBlock.get());
			BuildSummaryNode(ifStmt->elseBlock.get());
			return;
		}
		if (auto* whileStmt = dynamic_cast<WhileStatementNode*>(node))
		{
			BuildSummaryNode(whileStmt->condition.get());
			BuildSummaryNode(whileStmt->body.get());
			return;
		}
		if (auto* unsafeNode = dynamic_cast<UnsafeNode*>(node))
		{
			BuildSummaryNode(unsafeNode->body.get());
			return;
		}
		if (auto* returnNode = dynamic_cast<ReturnNode*>(node))
		{
			BuildSummaryNode(returnNode->value.get());
			return;
		}
		if (auto* printNode = dynamic_cast<PrintNode*>(node))
		{
			BuildSummaryNode(printNode->value.get());
			return;
		}
		if (auto* functionDecl = dynamic_cast<FunctionDeclNode*>(node))
		{
			BuildNamedFunctionSummary(*functionDecl);
			return;
		}
		if (auto* functionExpr = dynamic_cast<FunctionExprNode*>(node))
		{
			const std::string lambdaId = NewLambdaId();
			BuildLambdaSummary(lambdaId, *functionExpr);
			return;
		}
		if (auto* callNode = dynamic_cast<CallNode*>(node))
		{
			RecordSummaryCall(*callNode);
			return;
		}
		if (auto* member = dynamic_cast<MemberAccessNode*>(node))
		{
			BuildSummaryNode(member->object.get());
			return;
		}
		if (auto* binary = dynamic_cast<BinaryExprNode*>(node))
		{
			BuildSummaryNode(binary->left.get());
			BuildSummaryNode(binary->right.get());
			return;
		}
		if (auto* unary = dynamic_cast<UnaryExprNode*>(node))
		{
			BuildSummaryNode(unary->operand.get());
			return;
		}
		if (auto* index = dynamic_cast<IndexNode*>(node))
		{
			BuildSummaryNode(index->container.get());
			BuildSummaryNode(index->index.get());
			return;
		}
		if (auto* array = dynamic_cast<ArrayLiteralNode*>(node))
		{
			for (auto& element : array->elements)
			{
				BuildSummaryNode(element.get());
			}
		}
	}

	void RecordSummaryCall(CallNode& call)
	{
		if (m_summaryBuildStack.empty())
		{
			return;
		}

		for (auto& arg : call.args)
		{
			BuildSummaryNode(arg.get());
		}

		if (const auto syncCall = GetSyncCallInfo(call))
		{
			if ((syncCall->method == "lock" || syncCall->method == "unlock" || syncCall->method == "join")
				&& call.args.size() == 2)
			{
				SummaryOp op{
					syncCall->method == "lock"
						? SummaryOp::Kind::Lock
						: (syncCall->method == "unlock" ? SummaryOp::Kind::Unlock : SummaryOp::Kind::Join),
					MakeRef(call.args[0].get()),
					MakeRef(call.args[1].get()),
					{},
					{},
				};
				m_summaryBuildStack.back().ops.push_back(std::move(op));
			}
			return;
		}

		if (const auto calleeName = ResolveCalleeName(call.callee.get()))
		{
			SummaryOp op{
				SummaryOp::Kind::Call,
				{},
				{},
				*calleeName,
				{}
			};
			for (auto& arg : call.args)
			{
				op.callArgs.push_back(MakeRef(arg.get()));
			}
			m_summaryBuildStack.back().ops.push_back(std::move(op));
		}
	}

	[[nodiscard]] std::optional<std::string> ResolveCalleeName(ASTNode* callee) const
	{
		if (const auto* identifier = dynamic_cast<IdentifierNode*>(callee))
		{
			if (const auto callable = ResolveCallable(identifier->name))
			{
				return *callable;
			}
			return QualifyCallableName(identifier->name);
		}

		if (const auto* member = dynamic_cast<MemberAccessNode*>(callee))
		{
			const auto* object = dynamic_cast<IdentifierNode*>(member->object.get());
			if (!object)
			{
				return std::nullopt;
			}

			if (const auto it = m_importAliases.find(object->name); it != m_importAliases.end())
			{
				if (it->second == "std.sync")
				{
					return std::nullopt;
				}
				return it->second + "." + member->member;
			}

			if (const auto callable = ResolveCallable(object->name))
			{
				return *callable + "." + member->member;
			}
		}

		return std::nullopt;
	}

	[[nodiscard]] static ResourceInfo ResolveRef(
		const ResourceRef& ref,
		const std::unordered_map<std::string, ResourceInfo>& bindings)
	{
		if (ref.isParameter)
		{
			if (const auto it = bindings.find(ref.name); it != bindings.end())
			{
				return it->second;
			}
			return {};
		}
		return { ref.kind, ref.name };
	}

	void ReplayCallable(
		const std::string& callableId,
		const std::unordered_map<std::string, ResourceInfo>& bindings,
		const std::string& context)
	{
		if (m_activeSummaryCalls.contains(callableId))
		{
			return;
		}

		const auto it = m_callables.find(callableId);
		if (it == m_callables.end())
		{
			return;
		}

		m_activeSummaryCalls.insert(callableId);
		for (const auto& op : it->second.ops)
		{
			switch (op.kind)
			{
			case SummaryOp::Kind::Lock: {
				const auto thread = ResolveRef(op.first, bindings);
				const auto mutex = ResolveRef(op.second, bindings);
				ApplyLock(thread, mutex, context);
				break;
			}
			case SummaryOp::Kind::Unlock: {
				const auto thread = ResolveRef(op.first, bindings);
				const auto mutex = ResolveRef(op.second, bindings);
				ApplyUnlock(thread, mutex, context);
				break;
			}
			case SummaryOp::Kind::Join: {
				const auto waitingThread = ResolveRef(op.first, bindings);
				const auto targetThread = ResolveRef(op.second, bindings);
				ApplyJoin(waitingThread, targetThread, context);
				break;
			}
			case SummaryOp::Kind::Call: {
				const auto nested = m_callables.find(op.calleeName);
				if (nested == m_callables.end())
				{
					break;
				}

				std::unordered_map<std::string, ResourceInfo> nestedBindings;
				for (size_t i = 0; i < nested->second.params.size() && i < op.callArgs.size(); ++i)
				{
					nestedBindings[nested->second.params[i]] = ResolveRef(op.callArgs[i], bindings);
				}
				ReplayCallable(op.calleeName, nestedBindings, context + " -> " + op.calleeName);
				break;
			}
			}
		}
		m_activeSummaryCalls.erase(callableId);
	}

	void ApplyLock(const ResourceInfo& thread, const ResourceInfo& mutex, const std::string& context)
	{
		if (thread.kind != ResourceKind::Thread || mutex.kind != ResourceKind::Mutex)
		{
			return;
		}

		auto& heldLocks = m_threadHeldLocks[thread.id];
		if (heldLocks.contains(mutex.id))
		{
			throw std::runtime_error(
				"Potential deadlock detected at compile time: thread '"
				+ thread.id + "' tries to re-lock mutex '" + mutex.id + "' in " + context);
		}

		for (const auto& heldMutex : heldLocks)
		{
			if (!m_rustGraph.AddLockEdge(heldMutex, mutex.id, context))
			{
				throw std::runtime_error(m_rustGraph.LastError());
			}
		}
		heldLocks.insert(mutex.id);
	}

	void ApplyUnlock(const ResourceInfo& thread, const ResourceInfo& mutex, const std::string& context)
	{
		if (thread.kind != ResourceKind::Thread || mutex.kind != ResourceKind::Mutex)
		{
			return;
		}

		auto& heldLocks = m_threadHeldLocks[thread.id];
		if (!heldLocks.contains(mutex.id))
		{
			throw std::runtime_error(
				"Potential sync hazard detected at compile time: thread '"
				+ thread.id + "' unlocks mutex '" + mutex.id
				+ "' without proven ownership in " + context);
		}
		heldLocks.erase(mutex.id);
	}

	void ApplyJoin(
		const ResourceInfo& waitingThread,
		const ResourceInfo& targetThread,
		const std::string& context) const
	{
		if (waitingThread.kind != ResourceKind::Thread || targetThread.kind != ResourceKind::Thread)
		{
			return;
		}
		if (!m_rustGraph.AddJoinEdge(waitingThread.id, targetThread.id, context))
		{
			throw std::runtime_error(m_rustGraph.LastError());
		}
	}

	void AnalyzeNode(ASTNode* node)
	{
		if (!node)
		{
			return;
		}

		if (auto* block = dynamic_cast<BlockNode*>(node))
		{
			AnalyzeBlock(*block, true);
			return;
		}
		if (auto* importDecl = dynamic_cast<ImportDeclNode*>(node))
		{
			const std::string alias = importDecl->alias.empty() ? DefaultImportAlias(importDecl->qualifiedName) : importDecl->alias;
			m_importAliases[alias] = importDecl->qualifiedName;
			return;
		}
		if (auto* moduleDecl = dynamic_cast<ModuleDeclNode*>(node))
		{
			m_currentModule = moduleDecl->qualifiedName;
			m_scopes.clear();
			PushScope();
			m_importAliases.clear();
			return;
		}
		if (auto* exportDecl = dynamic_cast<ExportDeclNode*>(node))
		{
			if (exportDecl->declaration)
			{
				AnalyzeNode(exportDecl->declaration.get());
			}
			return;
		}
		if (auto* varDecl = dynamic_cast<VarDeclNode*>(node))
		{
			if (varDecl->initializer)
			{
				AnalyzeExpr(varDecl->initializer.get());
			}
			DefineResource(varDecl->name, InferResource(varDecl->initializer.get()));
			if (auto* functionExpr = dynamic_cast<FunctionExprNode*>(varDecl->initializer.get()))
			{
				if (const auto callable = ResolveCallableBindingForLambda(functionExpr))
				{
					DefineCallable(varDecl->name, *callable);
				}
			}
			else if (auto* identifier = dynamic_cast<IdentifierNode*>(varDecl->initializer.get()))
			{
				if (const auto callable = ResolveCallable(identifier->name))
				{
					DefineCallable(varDecl->name, *callable);
				}
			}
			return;
		}
		if (auto* assignment = dynamic_cast<AssignmentNode*>(node))
		{
			if (!assignment->name.empty())
			{
				AnalyzeExpr(assignment->value.get());
				DefineResource(assignment->name, InferResource(assignment->value.get()));
				if (auto* identifier = dynamic_cast<IdentifierNode*>(assignment->value.get()))
				{
					if (const auto callable = ResolveCallable(identifier->name))
					{
						DefineCallable(assignment->name, *callable);
					}
				}
			}
			else
			{
				if (assignment->object)
				{
					AnalyzeExpr(assignment->object.get());
				}
				if (assignment->dereferenceTarget)
				{
					AnalyzeExpr(assignment->dereferenceTarget.get());
				}
				if (assignment->index)
				{
					AnalyzeExpr(assignment->index.get());
				}
				AnalyzeExpr(assignment->value.get());
			}
			return;
		}
		if (auto* functionDecl = dynamic_cast<FunctionDeclNode*>(node))
		{
			DefineCallable(functionDecl->name, QualifyCallableName(functionDecl->name));
			return;
		}
		if (auto* functionExpr = dynamic_cast<FunctionExprNode*>(node))
		{
			(void)functionExpr;
			return;
		}
		if (auto* ifStmt = dynamic_cast<IfStatementNode*>(node))
		{
			AnalyzeExpr(ifStmt->condition.get());
			const auto before = m_threadHeldLocks;

			AnalyzeNode(ifStmt->thenBlock.get());
			const auto thenState = m_threadHeldLocks;

			m_threadHeldLocks = before;
			if (ifStmt->elseBlock)
			{
				AnalyzeNode(ifStmt->elseBlock.get());
			}
			const auto elseState = m_threadHeldLocks;

			m_threadHeldLocks = MergeHeldState(thenState, elseState);
			return;
		}
		if (auto* whileStmt = dynamic_cast<WhileStatementNode*>(node))
		{
			AnalyzeExpr(whileStmt->condition.get());
			const auto before = m_threadHeldLocks;
			AnalyzeNode(whileStmt->body.get());
			m_threadHeldLocks = before;
			return;
		}
		if (auto* unsafeNode = dynamic_cast<UnsafeNode*>(node))
		{
			AnalyzeNode(unsafeNode->body.get());
			return;
		}
		if (auto* returnNode = dynamic_cast<ReturnNode*>(node))
		{
			AnalyzeExpr(returnNode->value.get());
			return;
		}
		if (auto* printNode = dynamic_cast<PrintNode*>(node))
		{
			AnalyzeExpr(printNode->value.get());
			return;
		}
		if (auto* callNode = dynamic_cast<CallNode*>(node))
		{
			AnalyzeCall(*callNode);
			return;
		}
		if (auto* member = dynamic_cast<MemberAccessNode*>(node))
		{
			AnalyzeExpr(member->object.get());
			return;
		}
		if (auto* binary = dynamic_cast<BinaryExprNode*>(node))
		{
			AnalyzeExpr(binary->left.get());
			AnalyzeExpr(binary->right.get());
			return;
		}
		if (auto* unary = dynamic_cast<UnaryExprNode*>(node))
		{
			AnalyzeExpr(unary->operand.get());
			return;
		}
		if (auto* index = dynamic_cast<IndexNode*>(node))
		{
			AnalyzeExpr(index->container.get());
			AnalyzeExpr(index->index.get());
			return;
		}
		if (auto* array = dynamic_cast<ArrayLiteralNode*>(node))
		{
			for (auto& element : array->elements)
			{
				AnalyzeExpr(element.get());
			}
		}
	}

	[[nodiscard]] std::optional<std::string> ResolveCallableBindingForLambda(
		const FunctionExprNode* lambda) const
	{
		if (const auto it = m_lambdaCallableIds.find(lambda); it != m_lambdaCallableIds.end())
		{
			return it->second;
		}
		return std::nullopt;
	}

	void AnalyzeExpr(ASTNode* node)
	{
		AnalyzeNode(node);
	}

	void AnalyzeBlock(const BlockNode& block, const bool createScope)
	{
		if (createScope)
		{
			PushScope();
		}

		for (auto& statement : block.statements)
		{
			AnalyzeNode(statement.get());
		}

		if (createScope)
		{
			PopScope();
		}
	}

	[[nodiscard]] static HeldMap MergeHeldState(const HeldMap& lhs, const HeldMap& rhs)
	{
		HeldMap result;
		for (const auto& [threadId, lhsLocks] : lhs)
		{
			const auto rhsIt = rhs.find(threadId);
			if (rhsIt == rhs.end())
			{
				continue;
			}

			std::unordered_set<std::string> common;
			for (const auto& lockId : lhsLocks)
			{
				if (rhsIt->second.contains(lockId))
				{
					common.insert(lockId);
				}
			}
			if (!common.empty())
			{
				result.emplace(threadId, std::move(common));
			}
		}
		return result;
	}

	void AnalyzeCall(const CallNode& call)
	{
		AnalyzeExpr(call.callee.get());
		for (auto& arg : call.args)
		{
			AnalyzeExpr(arg.get());
		}

		if (const auto syncCall = GetSyncCallInfo(call))
		{
			if (syncCall->method == "lock" && call.args.size() == 2)
			{
				ApplyLock(
					InferResource(call.args[0].get()),
					InferResource(call.args[1].get()),
					CurrentContext());
				return;
			}

			if (syncCall->method == "unlock" && call.args.size() == 2)
			{
				ApplyUnlock(
					InferResource(call.args[0].get()),
					InferResource(call.args[1].get()),
					CurrentContext());
				return;
			}

			if (syncCall->method == "join" && call.args.size() == 2)
			{
				ApplyJoin(
					InferResource(call.args[0].get()),
					InferResource(call.args[1].get()),
					CurrentContext());
				return;
			}

			return;
		}

		if (const auto calleeName = ResolveCalleeName(call.callee.get()))
		{
			const auto callableIt = m_callables.find(*calleeName);
			if (callableIt == m_callables.end())
			{
				return;
			}

			std::unordered_map<std::string, ResourceInfo> bindings;
			for (size_t i = 0; i < callableIt->second.params.size() && i < call.args.size(); ++i)
			{
				bindings[callableIt->second.params[i]] = InferResource(call.args[i].get());
			}
			ReplayCallable(*calleeName, bindings, CurrentContext() + " -> " + *calleeName);
		}
	}

	SyncRustBridge m_rustGraph;
	std::unordered_map<std::string, std::string> m_importAliases;
	std::vector<Scope> m_scopes;
	HeldMap m_threadHeldLocks;
	std::vector<std::string> m_functionStack;
	std::unordered_map<std::string, CallableSummary> m_callables;
	std::unordered_map<const FunctionExprNode*, std::string> m_lambdaCallableIds;
	std::vector<SummaryBuildState> m_summaryBuildStack;
	std::unordered_set<std::string> m_activeSummaryCalls;
	std::string m_currentModule;
	size_t m_nextThreadId = 0;
	size_t m_nextMutexId = 0;
	size_t m_nextLambdaId = 0;
};

} // namespace

void SyncStaticAnalyzer::Analyze(ASTNode* root)
{
	Analyzer analyzer;
	analyzer.Analyze(root);
}
