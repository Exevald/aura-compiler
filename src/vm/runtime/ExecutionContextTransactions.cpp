#include "ExecutionContext.h"

#include <algorithm>
#include <ranges>
#include <unordered_set>

namespace VM::Execution
{

bool ExecutionContext::BeginTransaction(const Core::MutexPtr& mutex)
{
	if (!mutex)
	{
		RaiseError("Expected a mutex handle for transaction");
		return false;
	}
	return BeginTransaction(std::vector{ mutex });
}

bool ExecutionContext::BeginTransaction(const std::vector<Core::MutexPtr>& mutexes)
{
	if (mutexes.empty())
	{
		RaiseError("Transaction requires at least one mutex handle");
		return false;
	}

	std::vector<size_t> requestedMutexIds;
	requestedMutexIds.reserve(mutexes.size());
	for (const auto& mutex : mutexes)
	{
		if (!mutex)
		{
			RaiseError("Expected a mutex handle for transaction");
			return false;
		}
		requestedMutexIds.push_back(mutex->id);
	}
	std::ranges::sort(requestedMutexIds);
	requestedMutexIds.erase(std::unique(requestedMutexIds.begin(), requestedMutexIds.end()), requestedMutexIds.end());

	std::unordered_set<size_t> heldMutexIds;
	for (const auto& transaction : m_activeTransactions)
	{
		for (const auto mutexId : transaction.mutexIds)
		{
			heldMutexIds.insert(mutexId);
		}
	}

	std::vector<size_t> acquiredMutexIds;
	acquiredMutexIds.reserve(requestedMutexIds.size());
	for (const auto mutexId : requestedMutexIds)
	{
		if (heldMutexIds.contains(mutexId))
		{
			continue;
		}
		if (!TryLockMutex(m_currentThreadId, mutexId))
		{
			for (auto acquiredIt = acquiredMutexIds.rbegin(); acquiredIt != acquiredMutexIds.rend(); ++acquiredIt)
			{
				(void)UnlockMutex(m_currentThreadId, *acquiredIt);
			}
			return false;
		}
		acquiredMutexIds.push_back(mutexId);
	}

	ActiveTransaction transaction;
	transaction.mutexIds = std::move(requestedMutexIds);
	transaction.acquiredMutexIds = std::move(acquiredMutexIds);
	m_activeTransactions.push_back(std::move(transaction));
	return true;
}

bool ExecutionContext::EndTransaction()
{
	if (m_activeTransactions.empty())
	{
		RaiseError("Transaction stack underflow");
		return false;
	}

	auto transaction = std::move(m_activeTransactions.back());
	m_activeTransactions.pop_back();
	if (!m_activeTransactions.empty())
	{
		auto& parent = m_activeTransactions.back();
		for (const auto mutexId : transaction.mutexIds)
		{
			if (std::find(parent.mutexIds.begin(), parent.mutexIds.end(), mutexId) == parent.mutexIds.end())
			{
				parent.mutexIds.push_back(mutexId);
			}
		}
		for (const auto mutexId : transaction.acquiredMutexIds)
		{
			if (std::find(parent.acquiredMutexIds.begin(), parent.acquiredMutexIds.end(), mutexId)
				== parent.acquiredMutexIds.end())
			{
				parent.acquiredMutexIds.push_back(mutexId);
			}
		}
		for (auto& [name, value] : transaction.priorGlobals)
		{
			parent.priorGlobals.try_emplace(name, std::move(value));
		}
		for (auto& [name, value] : transaction.pendingGlobals)
		{
			parent.pendingGlobals[name] = std::move(value);
		}
		for (auto& [name, value] : transaction.priorThreadLocals)
		{
			parent.priorThreadLocals.try_emplace(name, std::move(value));
		}
		for (auto& [name, value] : transaction.pendingThreadLocals)
		{
			parent.pendingThreadLocals[name] = std::move(value);
		}
		for (auto& [arrayKey, elements] : transaction.priorArrayValues)
		{
			parent.priorArrayValues.try_emplace(arrayKey, std::move(elements));
		}
		for (auto& [arrayKey, elements] : transaction.pendingArrayValues)
		{
			parent.pendingArrayValues[arrayKey] = std::move(elements);
		}
		for (auto& [instanceKey, fields] : transaction.priorInstanceValues)
		{
			auto& parentFields = parent.priorInstanceValues[instanceKey];
			for (auto& [index, value] : fields)
			{
				parentFields.try_emplace(index, std::move(value));
			}
		}
		for (auto& [instanceKey, fields] : transaction.pendingInstanceValues)
		{
			auto& parentFields = parent.pendingInstanceValues[instanceKey];
			for (auto& [index, value] : fields)
			{
				parentFields[index] = std::move(value);
			}
		}
		return true;
	}

	for (auto& [name, value] : transaction.pendingGlobals)
	{
		m_runtime->SetGlobal(name, std::move(value));
	}
	for (auto& [name, value] : transaction.pendingThreadLocals)
	{
		m_threadLocalGlobals[name] = std::move(value);
	}
	for (auto& [arrayKey, elements] : transaction.pendingArrayValues)
	{
		auto* array = const_cast<Core::Array*>(static_cast<const Core::Array*>(arrayKey));
		if (array)
		{
			array->elements = std::move(elements);
		}
	}
	for (auto& [instanceKey, fields] : transaction.pendingInstanceValues)
	{
		auto* instance = const_cast<Core::Instance*>(static_cast<const Core::Instance*>(instanceKey));
		if (!instance)
		{
			continue;
		}
		for (auto& [index, value] : fields)
		{
			if (index < instance->fields.size())
			{
				instance->fields[index] = std::move(value);
			}
		}
	}
	for (auto acquiredIt = transaction.acquiredMutexIds.rbegin();
		acquiredIt != transaction.acquiredMutexIds.rend();
		++acquiredIt)
	{
		if (!UnlockMutex(m_currentThreadId, *acquiredIt))
		{
			return false;
		}
	}
	return true;
}

void ExecutionContext::UnwindTransactions(const size_t base)
{
	while (m_activeTransactions.size() > base)
	{
		auto transaction = std::move(m_activeTransactions.back());
		m_activeTransactions.pop_back();
		for (const auto& [instanceKey, fields] : transaction.priorInstanceValues)
		{
			auto* instance = const_cast<Core::Instance*>(static_cast<const Core::Instance*>(instanceKey));
			if (!instance)
			{
				continue;
			}
			for (const auto& [index, value] : fields)
			{
				if (index < instance->fields.size())
				{
					instance->fields[index] = value;
				}
			}
		}
		for (const auto& [arrayKey, elements] : transaction.priorArrayValues)
		{
			auto* array = const_cast<Core::Array*>(static_cast<const Core::Array*>(arrayKey));
			if (array)
			{
				array->elements = elements;
			}
		}
		for (const auto& [name, value] : transaction.priorThreadLocals)
		{
			m_threadLocalGlobals[name] = value;
		}
		for (const auto& [name, value] : transaction.priorGlobals)
		{
			m_runtime->SetGlobal(name, value);
		}
		for (auto acquiredIt = transaction.acquiredMutexIds.rbegin();
			acquiredIt != transaction.acquiredMutexIds.rend();
			++acquiredIt)
		{
			m_runtime->UnlockMutex(m_currentThreadId, *acquiredIt);
		}
	}
}

bool ExecutionContext::GetArraySize(const Core::ArrayPtr& array, size_t& outSize) const
{
	if (!array)
	{
		return false;
	}
	for (auto it = m_activeTransactions.rbegin(); it != m_activeTransactions.rend(); ++it)
	{
		if (const auto pendingIt = it->pendingArrayValues.find(array.get()); pendingIt != it->pendingArrayValues.end())
		{
			outSize = pendingIt->second.size();
			return true;
		}
	}
	outSize = array->elements.size();
	return true;
}

bool ExecutionContext::GetArrayElement(const Core::ArrayPtr& array, const size_t index, Core::Value& outValue) const
{
	if (!array)
	{
		return false;
	}
	for (auto it = m_activeTransactions.rbegin(); it != m_activeTransactions.rend(); ++it)
	{
		if (const auto arrayIt = it->pendingArrayValues.find(array.get());
			arrayIt != it->pendingArrayValues.end())
		{
			if (index >= arrayIt->second.size())
			{
				return false;
			}
			outValue = arrayIt->second[index];
			return true;
		}
	}
	if (index >= array->elements.size())
	{
		return false;
	}
	outValue = array->elements[index];
	return true;
}

bool ExecutionContext::SetArrayElement(const Core::ArrayPtr& array, const size_t index, Core::Value value)
{
	if (!array)
	{
		return false;
	}
	if (!m_activeTransactions.empty())
	{
		auto& transaction = m_activeTransactions.back();
		transaction.priorArrayValues.try_emplace(array.get(), array->elements);
		if (!transaction.pendingArrayValues.contains(array.get()))
		{
			transaction.pendingArrayValues.emplace(array.get(), array->elements);
		}
		auto& pending = transaction.pendingArrayValues.at(array.get());
		if (index >= pending.size())
		{
			return false;
		}
		pending[index] = std::move(value);
		return true;
	}
	if (index >= array->elements.size())
	{
		return false;
	}
	array->elements[index] = std::move(value);
	return true;
}

bool ExecutionContext::ReplaceArray(const Core::ArrayPtr& array, std::vector<Core::Value> elements)
{
	if (!array)
	{
		return false;
	}
	if (!m_activeTransactions.empty())
	{
		auto& transaction = m_activeTransactions.back();
		transaction.priorArrayValues.try_emplace(array.get(), array->elements);
		transaction.pendingArrayValues[array.get()] = std::move(elements);
		return true;
	}
	array->elements = std::move(elements);
	return true;
}

bool ExecutionContext::PushArrayElement(const Core::ArrayPtr& array, Core::Value value)
{
	if (!array)
	{
		return false;
	}
	if (!m_activeTransactions.empty())
	{
		auto& transaction = m_activeTransactions.back();
		transaction.priorArrayValues.try_emplace(array.get(), array->elements);
		if (!transaction.pendingArrayValues.contains(array.get()))
		{
			transaction.pendingArrayValues.emplace(array.get(), array->elements);
		}
		transaction.pendingArrayValues.at(array.get()).push_back(std::move(value));
		return true;
	}
	array->elements.push_back(std::move(value));
	return true;
}

bool ExecutionContext::PopArrayElement(const Core::ArrayPtr& array, Core::Value& outValue)
{
	if (!array)
	{
		return false;
	}
	if (!m_activeTransactions.empty())
	{
		auto& transaction = m_activeTransactions.back();
		transaction.priorArrayValues.try_emplace(array.get(), array->elements);
		if (!transaction.pendingArrayValues.contains(array.get()))
		{
			transaction.pendingArrayValues.emplace(array.get(), array->elements);
		}
		auto& pending = transaction.pendingArrayValues.at(array.get());
		if (pending.empty())
		{
			return false;
		}
		outValue = pending.back();
		pending.pop_back();
		return true;
	}
	if (array->elements.empty())
	{
		return false;
	}
	outValue = array->elements.back();
	array->elements.pop_back();
	return true;
}

void ExecutionContext::RecordArrayWrite(
	const Core::ArrayPtr& array,
	const size_t,
	const Core::Value& previousValue)
{
	(void)previousValue;
	if (!array || m_activeTransactions.empty())
	{
		return;
	}
	m_activeTransactions.back().priorArrayValues.try_emplace(array.get(), array->elements);
}

bool ExecutionContext::GetInstanceField(const Core::InstancePtr& instance, const size_t index, Core::Value& outValue) const
{
	if (!instance || index >= instance->fields.size())
	{
		return false;
	}
	for (auto it = m_activeTransactions.rbegin(); it != m_activeTransactions.rend(); ++it)
	{
		if (const auto instanceIt = it->pendingInstanceValues.find(instance.get());
			instanceIt != it->pendingInstanceValues.end())
		{
			if (const auto valueIt = instanceIt->second.find(index); valueIt != instanceIt->second.end())
			{
				outValue = valueIt->second;
				return true;
			}
		}
	}
	outValue = instance->fields[index];
	return true;
}

bool ExecutionContext::SetInstanceField(const Core::InstancePtr& instance, const size_t index, Core::Value value)
{
	if (!instance || index >= instance->fields.size())
	{
		return false;
	}
	if (!m_activeTransactions.empty())
	{
		RecordInstanceFieldWrite(instance, index, instance->fields[index]);
		m_activeTransactions.back().pendingInstanceValues[instance.get()][index] = std::move(value);
		return true;
	}
	instance->fields[index] = std::move(value);
	return true;
}

void ExecutionContext::RecordInstanceFieldWrite(
	const Core::InstancePtr& instance,
	const size_t index,
	const Core::Value& previousValue)
{
	if (!instance || m_activeTransactions.empty())
	{
		return;
	}
	if (auto& snapshots = m_activeTransactions.back().priorInstanceValues[instance.get()];
		!snapshots.contains(index))
	{
		snapshots.emplace(index, previousValue);
	}
}

void ExecutionContext::UnwindAllTransactions()
{
	UnwindTransactions(0);
}

} // namespace VM::Execution
