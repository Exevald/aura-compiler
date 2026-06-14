#include "../SyncStaticAnalyzer.h"
#include "../common/SemanticAnalyzerSupport.h"
#include "SemanticAnalyzer.h"

#include <cctype>
#include <functional>
#include <ranges>
#include <stdexcept>

using SemanticAnalyzerDetail::FormatUndefined;
using SemanticAnalyzerDetail::ScopeExit;
using SemanticAnalyzerDetail::SplitGenericName;
using SemanticAnalyzerDetail::SubstituteTypeString;

void SemanticAnalyzer::EnvironmentState::Reset()
{
	scopes.clear();
	scopes.emplace_back();
	structMethodSelfStack.clear();
}

void SemanticAnalyzer::EnvironmentState::PushScope()
{
	scopes.emplace_back();
}

void SemanticAnalyzer::EnvironmentState::PopScope()
{
	if (scopes.empty())
	{
		throw std::runtime_error("Internal semantic analyzer error: empty environment");
	}
	scopes.pop_back();
}

void SemanticAnalyzer::EnvironmentState::PushStructMethodSelf(TypeInfo selfType)
{
	structMethodSelfStack.push_back(std::move(selfType));
}

void SemanticAnalyzer::EnvironmentState::PopStructMethodSelf()
{
	if (structMethodSelfStack.empty())
	{
		throw std::runtime_error("Internal semantic analyzer error: empty struct self stack");
	}
	structMethodSelfStack.pop_back();
}

void SemanticAnalyzer::EnvironmentState::Define(const std::string& name, TypeInfo type, const bool isConst)
{
	if (scopes.empty())
	{
		throw std::runtime_error("Internal semantic analyzer error: no environment scope");
	}

	scopes.back()[name] = { std::move(type), isConst };
}

void SemanticAnalyzer::EnvironmentState::EnsureDeclared(const std::string& name) const
{
	if (scopes.empty())
	{
		throw std::runtime_error("Internal semantic analyzer error: empty environment");
	}

	for (const auto& scope : std::ranges::reverse_view(scopes))
	{
		if (scope.contains(name))
		{
			return;
		}
	}

	throw std::runtime_error(FormatUndefined(name));
}

SemanticAnalyzer::TypeInfo SemanticAnalyzer::EnvironmentState::Resolve(const std::string& name) const
{
	for (const auto& scope : std::ranges::reverse_view(scopes))
	{
		if (scope.contains(name))
		{
			return scope.at(name).type;
		}
	}

	if (!structMethodSelfStack.empty())
	{
		const auto& selfType = structMethodSelfStack.back();
		if (selfType.fields && selfType.fields->contains(name))
		{
			return selfType.fields->at(name);
		}
	}

	throw std::runtime_error(FormatUndefined(name));
}

bool SemanticAnalyzer::EnvironmentState::ResolveIsConst(const std::string& name) const
{
	for (const auto& scope : std::ranges::reverse_view(scopes))
	{
		if (scope.contains(name))
		{
			return scope.at(name).isConst;
		}
	}

	throw std::runtime_error(FormatUndefined(name));
}

