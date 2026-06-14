#pragma once

#include "../builder/ASTBuilder.h"

#include <string>

namespace ASTBuilderDetail
{

inline std::string ExtractQualifiedId(ASTNode* node);
inline std::string ExtractQualifiedId(const ASTNode* node);

inline std::string ExtractType(const ASTNode* node)
{
	if (!node)
	{
		return "auto";
	}
	if (auto* leaf = dynamic_cast<const LeafNode*>(node))
	{
		return leaf->value;
	}
	auto* raw = dynamic_cast<const RawNode*>(node);
	if (!raw)
	{
		return "auto";
	}
	if (raw->ruleName == "type_guide_opt" && raw->children.size() >= 2)
	{
		return ExtractType(raw->children[1].get());
	}
	if (raw->ruleName == "dataType")
	{
		if (raw->children.empty())
		{
			return "auto";
		}

		const std::string left = ExtractType(raw->children[0].get());
		if (raw->children.size() < 2)
		{
			return left;
		}

		const auto* tail = dynamic_cast<const RawNode*>(raw->children[1].get());
		if (!tail || tail->children.empty())
		{
			return left;
		}

		if (tail->children.size() >= 2)
		{
			if (left.find(',') != std::string::npos
				&& !(left.size() >= 2 && left.front() == '(' && left.back() == ')'))
			{
				return "(" + left + ")->" + ExtractType(tail->children[1].get());
			}
			return left + "->" + ExtractType(tail->children[1].get());
		}
		return left;
	}
	if (raw->ruleName == "base_dataType" && !raw->children.empty())
	{
		if (auto* qualifiedRaw = dynamic_cast<RawNode*>(raw->children[0].get());
			qualifiedRaw && qualifiedRaw->ruleName == "qualified_id")
		{
			std::string result = ExtractQualifiedId(qualifiedRaw);
			if (raw->children.size() >= 2)
			{
				const std::string typeArgs = ExtractType(raw->children[1].get());
				if (!typeArgs.empty())
				{
					result += typeArgs;
				}
			}
			return result;
		}
		if (auto* leaf = dynamic_cast<LeafNode*>(raw->children[0].get()))
		{
			if (leaf->value == "[")
			{
				return "[" + ExtractType(raw->children[1].get()) + "]";
			}
			if (leaf->value == "map" && raw->children.size() >= 4)
			{
				return "map<" + ExtractType(raw->children[2].get()) + "," + ExtractType(raw->children[4].get()) + ">";
			}
			if ((leaf->value == "ptr" || leaf->value == "ref") && raw->children.size() >= 3)
			{
				return leaf->value + "<" + ExtractType(raw->children[2].get()) + ">";
			}
			if (leaf->type == "identifier")
			{
				std::string result = leaf->value;
				if (raw->children.size() >= 2)
				{
					const std::string typeArgs = ExtractType(raw->children[1].get());
					if (!typeArgs.empty())
					{
						result += typeArgs;
					}
				}
				return result;
			}
			if (leaf->value == "(" && raw->children.size() >= 2)
			{
				const std::string inner = ExtractType(raw->children[1].get());
				if (inner.find(',') != std::string::npos)
				{
					return "(" + inner + ")";
				}
				return inner;
			}
			return leaf->value;
		}
	}
	if (raw->ruleName == "type_args_opt")
	{
		if (raw->children.empty())
		{
			return {};
		}
		if (raw->children.size() >= 2)
		{
			return "<" + ExtractType(raw->children[1].get()) + ">";
		}
		return {};
	}
	if (raw->ruleName == "dataType_list" || raw->ruleName == "dataType_list_tail")
	{
		if (raw->children.empty())
		{
			return {};
		}
		const int offset = raw->ruleName == "dataType_list" ? 0 : 1;
		std::string result = ExtractType(raw->children[offset].get());
		if (raw->children.size() > offset + 1)
		{
			const std::string tail = ExtractType(raw->children.back().get());
			if (!tail.empty())
			{
				result += "," + tail;
			}
		}
		return result;
	}
	if (raw->ruleName == "type_constraint")
	{
		if (raw->children.empty())
		{
			return {};
		}
		std::string result = ExtractType(raw->children[0].get());
		if (raw->children.size() > 1)
		{
			const std::string tail = ExtractType(raw->children[1].get());
			if (!tail.empty())
			{
				result += "+" + tail;
			}
		}
		return result;
	}
	if (raw->ruleName == "type_constraint_tail")
	{
		if (raw->children.size() < 2)
		{
			return {};
		}
		std::string result = ExtractType(raw->children[1].get());
		if (raw->children.size() > 2)
		{
			const std::string tail = ExtractType(raw->children[2].get());
			if (!tail.empty())
			{
				result += "+" + tail;
			}
		}
		return result;
	}
	return "auto";
}

inline std::string ExtractQualifiedId(ASTNode* node)
{
	if (!node)
	{
		return {};
	}

	if (auto* leaf = dynamic_cast<LeafNode*>(node))
	{
		return leaf->value;
	}

	auto* raw = dynamic_cast<RawNode*>(node);
	if (!raw)
	{
		return {};
	}

	if (raw->ruleName == "qualified_id" && raw->children.size() >= 2)
	{
		std::string result = ExtractQualifiedId(raw->children[0].get());
		const std::string tail = ExtractQualifiedId(raw->children[1].get());
		if (!tail.empty())
		{
			result += tail;
		}
		return result;
	}

	if (raw->ruleName == "qualified_id_tail")
	{
		if (raw->children.empty())
		{
			return {};
		}

		std::string result;
		if (raw->children.size() >= 2)
		{
			result = "." + ExtractQualifiedId(raw->children[1].get());
		}
		if (raw->children.size() >= 3)
		{
			result += ExtractQualifiedId(raw->children[2].get());
		}
		return result;
	}

	for (auto& child : raw->children)
	{
		const std::string value = ExtractQualifiedId(child.get());
		if (!value.empty())
		{
			return value;
		}
	}

	return {};
}

inline std::string ExtractQualifiedId(const ASTNode* node)
{
	return ExtractQualifiedId(const_cast<ASTNode*>(node));
}

} // namespace ASTBuilderDetail
