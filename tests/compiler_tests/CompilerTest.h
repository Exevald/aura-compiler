#pragma once

#include "../../src/ast/AST.h"
#include "../../src/lexer/Lexer.h"
#include "../../src/parser/Parser.h"
#include "../../src/vm/core/OpCode.h"
#include "Utils.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <unistd.h>

using namespace VM::Core;

class CompilerTest : public ::testing::Test
{
protected:
	std::filesystem::path m_tempRoot;

	void SetUp() override
	{
		m_tempRoot = std::filesystem::temp_directory_path()
			/ ("aura_compiler_tests_" + std::to_string(::getpid()));
		std::filesystem::remove_all(m_tempRoot);
		std::filesystem::create_directories(m_tempRoot);
	}

	void TearDown() override
	{
		std::filesystem::remove_all(m_tempRoot);
	}

	static ASTNodePtr ParseCode(const std::string& source)
	{
		if (CachedGrammarText().empty())
		{
			return nullptr;
		}
		Lexer lexer(source);
		SLRParser parser(lexer, CachedGrammarText());
		if (!parser.Parse())
		{
			return nullptr;
		}
		return parser.GetRoot();
	}
};