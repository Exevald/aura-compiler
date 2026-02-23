#include "Parser.h"

#include <fstream>
#include <gtest/gtest.h>

bool CheckSyntax(const std::string& code)
{
	std::ifstream file("grammar.txt");
	std::stringstream buffer;
	buffer << file.rdbuf();
	file.close();

	Lexer lexer(code);
	SLRParser parser(lexer, buffer.str());
	return parser.Parse();
}

TEST(ParserFullCoverage, ModulesAndImports)
{
	EXPECT_TRUE(CheckSyntax("module core.network.http;"));
	EXPECT_TRUE(CheckSyntax("import std.io;"));
	EXPECT_TRUE(CheckSyntax("import math as m;"));
	EXPECT_TRUE(CheckSyntax("export var version = 1;"));
	EXPECT_TRUE(CheckSyntax("export struct Point { x: int; };"));
	EXPECT_TRUE(CheckSyntax("export effect Logger;"));
}

TEST(ParserFullCoverage, ComplexTypes)
{
	EXPECT_TRUE(CheckSyntax("var a: [int];"));
	EXPECT_TRUE(CheckSyntax("var p: ptr<int>;"));
	EXPECT_TRUE(CheckSyntax("var r: ref<string>;"));
	EXPECT_TRUE(CheckSyntax("var f: int -> void;"));
	EXPECT_TRUE(CheckSyntax("var g: ptr<[int]>;"));
	EXPECT_TRUE(CheckSyntax("type Callback<T> = T -> bool;"));
}

TEST(ParserFullCoverage, AdvancedFunctions)
{
	std::string func = R"(
        fn process<T>(data: T) : void
        with { ctx: Context }
        raises { NetworkError | Timeout }
        requires (data != null)
        {
            return;
        }
    )";
	EXPECT_TRUE(CheckSyntax(func));
}

TEST(ParserFullCoverage, DataStructures)
{
	EXPECT_TRUE(CheckSyntax("enum Option<T> { Some(T) | None };"));

	std::string iface = R"(
        interface Reader {
            fn read(buf: [int]) : int raises { IO };
            fn close() : void;
        };
    )";
	EXPECT_TRUE(CheckSyntax(iface));
}

TEST(ParserFullCoverage, ExpressionsExtended)
{
	EXPECT_TRUE(CheckSyntax("var x = not true or false and 1 > 2;"));
	EXPECT_TRUE(CheckSyntax("var y = a.b[0].call(1, 2);"));
	EXPECT_TRUE(CheckSyntax("var z = [1, 2, 3];"));
	EXPECT_TRUE(CheckSyntax("var c = comptime { 1 + 1; };"));
}

TEST(ParserFullCoverage, IteratorsAndAdapters)
{
	std::string iter = "fn test() { "
					   "iter (item of collection with [take(10), filter(fn(x) -> x > 0)]) { "
					   "    print(item); "
					   "}; "
					   "}";
	EXPECT_TRUE(CheckSyntax(iter));
}

TEST(ParserFullCoverage, EffectsSystem)
{
	std::string handleStmt = "fn test() { "
							 "handle readFile() with { effect open(p) -> { return 1; } }; "
							 "}";
	EXPECT_TRUE(CheckSyntax(handleStmt));
}

TEST(ParserFullCoverage, ActorModel)
{
	std::string actor = R"(
        actor Wallet<Currency> {
            state balance: int = 0;
            msg deposit(amount: int) {
                balance = balance + amount;
            }
        }
    )";
	EXPECT_TRUE(CheckSyntax(actor));
}

TEST(ParserFullCoverage, MemoryAndSafety)
{
	EXPECT_TRUE(CheckSyntax("fn test() { transaction (shared db) { db.commit(); }; }"));
	EXPECT_TRUE(CheckSyntax("fn test() { unsafe { ptr_copy(a, b); }; }"));
}

TEST(ParserFullCoverage, NegativeTests)
{
	EXPECT_FALSE(CheckSyntax("var 123name = 1;"));
	EXPECT_FALSE(CheckSyntax("fn f() { return }"));
	EXPECT_FALSE(CheckSyntax("struct S { x int }"));
	EXPECT_FALSE(CheckSyntax("iter (x collection) {}"));
	EXPECT_FALSE(CheckSyntax("handle {} with {};"));
	EXPECT_FALSE(CheckSyntax("type A = ;"));
}

TEST(ParserFullCoverage, ArrowFunctions)
{
	EXPECT_TRUE(CheckSyntax("var f = fn(x: int) -> x + 1;"));
	EXPECT_TRUE(CheckSyntax("var g = fn(x, y) raises {Err} -> { return x + y; };"));
}