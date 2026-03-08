#pragma once

#include <set>

enum class ActionType
{
	SHIFT,
	REDUCE,
	ACCEPT,
	ERROR
};

struct Action
{
	ActionType type = ActionType::ERROR;
	int value = 0;
};

struct LR0Item
{
	int ruleIndex;
	int dotPosition;
	bool operator<(const LR0Item& other) const
	{
		if (ruleIndex != other.ruleIndex)
		{
			return ruleIndex < other.ruleIndex;
		}
		return dotPosition < other.dotPosition;
	}
	bool operator==(const LR0Item& other) const
	{
		return ruleIndex == other.ruleIndex && dotPosition == other.dotPosition;
	}
};

using LR0State = std::set<LR0Item>;