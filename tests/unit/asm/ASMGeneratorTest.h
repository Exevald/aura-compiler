#pragma once

#include "../../../src/asmGenerator/ASMGenerator.h"
#include "../../../src/lexer/Lexer.h"
#include "../../../src/parser/Parser.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <unistd.h>

class ASMGeneratorTest : public ::testing::Test
{
protected:
	std::filesystem::path m_tempRoot;

	void SetUp() override
	{
		m_tempRoot = std::filesystem::temp_directory_path()
			/ ("aura_asm_tests_" + std::to_string(::getpid()));
		std::filesystem::remove_all(m_tempRoot);
		std::filesystem::create_directories(m_tempRoot);
	}

	void TearDown() override
	{
		std::filesystem::remove_all(m_tempRoot);
	}

	static std::filesystem::path GrammarPath()
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

	static std::string NormalizeGrammar(std::istream& input)
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

	static ASTNodePtr ParseCode(const std::string& source)
	{
		std::ifstream file(GrammarPath());
		if (!file.is_open())
		{
			return nullptr;
		}

		Lexer lexer(source);
		SLRParser parser(lexer, NormalizeGrammar(file));
		if (!parser.Parse())
		{
			return nullptr;
		}
		return parser.GetRoot();
	}

	static void WriteFile(const std::filesystem::path& path, const std::string& contents)
	{
		std::filesystem::create_directories(path.parent_path());
		std::ofstream output(path);
		output << contents;
	}

	static std::string ReadFile(const std::filesystem::path& path)
	{
		std::ifstream input(path);
		std::stringstream buffer;
		buffer << input.rdbuf();
		return buffer.str();
	}

	std::string EmitAsm(const std::string& source, const std::string& fileName = "generator.asm")
	{
		auto root = ParseCode(source);
		EXPECT_NE(root, nullptr);
		if (!root)
		{
			return {};
		}

		const auto asmPath = m_tempRoot / fileName;
		{
			ASMGenerator generator{ std::ofstream(asmPath) };
			generator.Compile(root.get());
		}
		return ReadFile(asmPath);
	}

};
