#pragma once

#include "../../../src/Compiler.h"
#include "../../../src/vm/core/OpCode.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace VM::Core;

inline std::filesystem::path GrammarPath()
{
	for (auto dir = std::filesystem::current_path(); !dir.empty(); dir = dir.parent_path())
	{
		if (const auto candidate = dir / "grammar.md";
			std::filesystem::exists(candidate))
		{
			return candidate;
		}
		if (dir == dir.root_path())
		{
			break;
		}
	}
	return std::filesystem::weakly_canonical(std::filesystem::path(__FILE__))
			   .parent_path()
			   .parent_path()
			   .parent_path()
		/ "grammar.md";
}

inline std::string NormalizeGrammar(std::istream& input)
{
	std::stringstream raw;
	raw << input.rdbuf();

	std::stringstream lines(raw.str());
	std::stringstream normalized;
	std::string line;
	while (std::getline(lines, line))
	{
		if (line.rfind("```", 0) != 0)
		{
			normalized << line << '\n';
		}
	}
	return normalized.str();
}

inline const std::string& CachedGrammarText()
{
	static const std::string grammar = [] {
		std::ifstream file(GrammarPath());
		if (!file.is_open())
		{
			return std::string{};
		}
		return NormalizeGrammar(file);
	}();
	return grammar;
}

inline void WriteFile(const std::filesystem::path& path, const std::string& contents)
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream output(path);
	output << contents;
}

inline bool ChunkContainsOpcode(const VM::Execution::Chunk& chunk, const OpCode target)
{
	for (const auto byte : chunk.code)
	{
		if (byte == static_cast<uint8_t>(target))
		{
			return true;
		}
	}

	for (const auto& constant : chunk.constants)
	{
		if (std::holds_alternative<FunctionPtr>(constant))
		{
			if (const auto& fn = std::get<FunctionPtr>(constant);
				fn && ChunkContainsOpcode(*fn->chunk, target))
			{
				return true;
			}
		}
	}

	return false;
}
