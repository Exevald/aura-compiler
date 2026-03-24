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

TEST(ParserTests, ModulesAndImports)
{
	EXPECT_TRUE(CheckSyntax("module core.network.http;"));
	EXPECT_TRUE(CheckSyntax("import std.io;"));
	EXPECT_TRUE(CheckSyntax("import math as m;"));
	EXPECT_TRUE(CheckSyntax("export var version = 1;"));
	EXPECT_TRUE(CheckSyntax("export struct Point { x: int; };"));
	EXPECT_TRUE(CheckSyntax("export effect Logger;"));
}

TEST(ParserTests, ComplexTypes)
{
	EXPECT_TRUE(CheckSyntax("var a: [int];"));
	EXPECT_TRUE(CheckSyntax("var p: ptr<int>;"));
	EXPECT_TRUE(CheckSyntax("var r: ref<string>;"));
	EXPECT_TRUE(CheckSyntax("var f: int -> void;"));
	EXPECT_TRUE(CheckSyntax("var g: ptr<[int]>;"));
	EXPECT_TRUE(CheckSyntax("type Callback<T> = T -> bool;"));
}

TEST(ParserTests, AdvancedFunctions)
{
	const std::string func = R"(
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

TEST(ParserTests, DataStructures)
{
	EXPECT_TRUE(CheckSyntax("enum Option<T> { Some(T) | None }"));

	const std::string iface = R"(
        interface Reader {
            fn read(buf: [int]) : int raises { IO };
            fn close() : void;
        }
    )";
	EXPECT_TRUE(CheckSyntax(iface));
}

TEST(ParserTests, ExpressionsExtended)
{
	EXPECT_TRUE(CheckSyntax("var x = not true or false and 1 > 2;"));
	EXPECT_TRUE(CheckSyntax("var y = a.b[0].call(1, 2);"));
	EXPECT_TRUE(CheckSyntax("var z = [1, 2, 3];"));
	EXPECT_TRUE(CheckSyntax("var c = comptime { 1 + 1; };"));
}

TEST(ParserTests, IteratorsAndAdapters)
{
	const std::string iter = "fn test() { "
							 "iter (item of collection with [take(10), filter(fn(x) -> x > 0)]) { "
							 "    print(item); "
							 "} "
							 "}";
	EXPECT_TRUE(CheckSyntax(iter));
}

TEST(ParserTests, EffectsSystem)
{
	const std::string handleStmt = "fn test() { "
								   "handle readFile() with { effect open(p) -> { return 1; } } "
								   "}";
	EXPECT_TRUE(CheckSyntax(handleStmt));
}

TEST(ParserTests, ActorModel)
{
	const std::string actor = R"(
        actor Wallet<Currency> {
            state balance: int = 0;
            msg deposit(amount: int) {
                balance = balance + amount;
            }
        }
    )";
	EXPECT_TRUE(CheckSyntax(actor));
}

TEST(ParserTests, MemoryAndSafety)
{
	EXPECT_TRUE(CheckSyntax("fn test() { transaction (shared db) { db.commit(); } }"));
	EXPECT_TRUE(CheckSyntax("fn test() { unsafe { ptr_copy(a, b); } }"));
}

TEST(ParserTests, NegativeTests)
{
	EXPECT_FALSE(CheckSyntax("var 123name = 1;"));
	EXPECT_FALSE(CheckSyntax("fn f() { return }"));
	EXPECT_FALSE(CheckSyntax("struct S { x int }"));
	EXPECT_FALSE(CheckSyntax("iter (x collection) {}"));
	EXPECT_FALSE(CheckSyntax("handle {} with {};"));
	EXPECT_FALSE(CheckSyntax("type A = ;"));
}

TEST(ParserTests, ArrowFunctions)
{
	EXPECT_TRUE(CheckSyntax("var f = fn(x: int) -> x + 1;"));
	EXPECT_TRUE(CheckSyntax("var g = fn(x, y) raises {Err} -> { return x + y; };"));
}

TEST(ParserTests, DeeplyNestedTypes)
{
	EXPECT_TRUE(CheckSyntax("var x: ptr<ref<[int -> void]>>;"));
	EXPECT_TRUE(CheckSyntax("type ComplexFn = int -> float -> string -> bool;"));
	EXPECT_TRUE(CheckSyntax("fn generic<T: Comparable + Serializable + Cloneable>(arg: T) {}"));
}

TEST(ParserTests, AllIteratorAdapters)
{
	const std::string fullIter = R"(
        fn test() {
            iter (x of collection with [
                drop(5),
                take(10),
                reverse,
                filter(fn(v) -> v != null),
                transform(fn(v) -> v.id)
            ]) {
                print x;
            }
        }
    )";
	EXPECT_TRUE(CheckSyntax(fullIter));
}

TEST(ParserTests, FormalContracts)
{
	EXPECT_TRUE(CheckSyntax(R"(
        fn divide(a: int, b: int) : int
        requires (b != 0)
        requires (a > b)
        ensures (return < a)
        { return a / b; }
    )"));

	EXPECT_TRUE(CheckSyntax(R"(
        struct Rectangle {
            w: int;
            h: int;
        }
        invariant (w > 0)
        invariant (h > 0)
    )"));
}

TEST(ParserTests, ActorFullFeatures)
{
	const std::string actor = R"(
        actor BankAccount {
            state balance: int = 0;
            state owner: string;

            msg deposit(amount: int)
            requires (amount > 0)
            {
                balance = balance + amount;
            }

            query getBalance(): int {
                return balance;
            }

            query getOwner(): string {
                return owner;
            }
        }
    )";
	EXPECT_TRUE(CheckSyntax(actor));
}

TEST(ParserTests, EffectDetails)
{
	EXPECT_TRUE(CheckSyntax(R"(
        effect FileSystem {
            fn open(path: string) : int;
            fn close(fd: int) : void;
        }
    )"));

	EXPECT_TRUE(CheckSyntax(R"(
        fn main() {
            handle app.run() with {
                effect Log(m) -> { print m; }
                effect Error(e) -> { return; }
                effect Panic() -> { return; }
            };
        }
    )"));
}

TEST(ParserTests, ExpressionPrecedenceAndTrailers)
{
	EXPECT_TRUE(CheckSyntax("var res = a + b * c == d / e - f;"));
	EXPECT_TRUE(CheckSyntax("var val = object.method(1)(2).field[index].subField;"));
	EXPECT_TRUE(CheckSyntax("var logic = not !true == +-5;"));
}

TEST(ParserTests, SpecialKeywords)
{
	EXPECT_TRUE(CheckSyntax("const x = comptime { var a = 1; return a + 1; };"));
	EXPECT_TRUE(CheckSyntax("fn sync() { transaction(shared global_lock) { do_work(); } }"));
	EXPECT_TRUE(CheckSyntax("fn hack() { unsafe { *(ptr_val) = 0; } }"));
}

TEST(ParserTests, OptionalAndEmpty)
{
	EXPECT_TRUE(CheckSyntax("struct Empty {}"));
	EXPECT_TRUE(CheckSyntax("interface Blank {}"));
	EXPECT_TRUE(CheckSyntax("enum Single { Only }"));
	EXPECT_TRUE(CheckSyntax("module test; ; ; ;"));
}

TEST(ParserTests, StrictNegativeTests)
{
	EXPECT_FALSE(CheckSyntax("var x: int"));
	EXPECT_FALSE(CheckSyntax("fn f(a: int,) {}"));
	EXPECT_FALSE(CheckSyntax("struct S { x: int }"));
	EXPECT_FALSE(CheckSyntax("const x;"));
	EXPECT_FALSE(CheckSyntax("export 123;"));
	EXPECT_FALSE(CheckSyntax("import ;"));
}

TEST(ParserTests, ControlFlow)
{
	EXPECT_TRUE(CheckSyntax(R"(
        fn test(x: int) {
            if (x > 0) {
                print x;
            }
        }
    )"));

	EXPECT_TRUE(CheckSyntax(R"(
        fn test(x: int) {
            if (x mod 2 == 0) {
                print "even";
            } else {
                print "odd";
            }
        }
    )"));

	EXPECT_TRUE(CheckSyntax(R"(
        fn test(x: int) {
            if (x > 100) {
                print "large";
            } else if (x > 50) {
                print "medium";
            } else if (x > 0) {
                print "small";
            } else {
                print "none";
            }
        }
    )"));

	EXPECT_TRUE(CheckSyntax(R"(
        fn loop() {
            var i = 0;
            while (i < 10) {
                print i;
                i = i + 1;
            }
        }
    )"));

	EXPECT_TRUE(CheckSyntax(R"(
        fn nested() {
            while (true) {
                if (should_stop()) {
                    return;
                }

                if (data_available) {
                    process();
                }
            }
        }
    )"));
}

TEST(ParserTests, EmptyBlocks)
{
	EXPECT_TRUE(CheckSyntax("fn empty() { if (true) {} while (false) {} }"));
}

TEST(ParserTests, ModifiersAndExports)
{
	EXPECT_TRUE(CheckSyntax("shared var globalCounter: int = 0;"));
	EXPECT_TRUE(CheckSyntax("thread_local var localCache: [int] = [];"));
	EXPECT_TRUE(CheckSyntax("export MyType;"));
	EXPECT_TRUE(CheckSyntax("export myFunction;"));
}

TEST(ParserTests, TypeConstraints)
{
	EXPECT_TRUE(CheckSyntax("fn sort<T: Comparable + Cloneable + Serializable>(list: [T]) {}"));
	EXPECT_TRUE(CheckSyntax("type Registry<T: ptr<Context>> = [T];"));
}

TEST(ParserTests, MultipleEffectsAndContexts)
{
	EXPECT_TRUE(CheckSyntax(R"(
        fn compute() with { Logger: ILog | Config: IConfig | Database: IDB } {
            return;
        }
    )"));

	EXPECT_TRUE(CheckSyntax(R"(
        fn dangerous() raises { ReadError | WriteError | Panic } {
            if (true) { return; }
        }
    )"));
}

TEST(ParserTests, DetailedDataStructures)
{
	EXPECT_TRUE(CheckSyntax(R"(
        enum Result<T, E> {
            Ok(T) |
            Err(E) |
            Pending
        }
    )"));

	EXPECT_TRUE(CheckSyntax(R"(
        interface Storage {
            fn save(data: string) : bool;
            fn load(id: int) : string raises { NotFound };
            fn clear() : void;
        }
    )"));
}

TEST(ParserTests, ArithmeticExtended)
{
	EXPECT_TRUE(CheckSyntax("var a = 10 mod 3 + 10 mod 3 - 10 div 2;"));
	EXPECT_TRUE(CheckSyntax("var b = -*p + !!true;"));
}