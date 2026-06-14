#include "CompilerTest.h"

TEST_F(CompilerTest, BytecodeSimpleAddition)
{
	auto root = ParseCode("var res = 10 + 20;");
	ASSERT_NE(root, nullptr)
		<< "Parsing failed! Remember: expressions in Aura must be inside declarations or functions.";

	BytecodeGenerator compiler;
	auto chunk = compiler.Compile(root.get());

	ASSERT_GE(chunk.code.size(), 8);

	EXPECT_EQ(static_cast<OpCode>(chunk.code[0]), OpCode::OP_CONSTANT);
	EXPECT_EQ(static_cast<OpCode>(chunk.code[3]), OpCode::OP_CONSTANT);

	const auto firstIndex = static_cast<uint16_t>((chunk.code[1] << 8) | chunk.code[2]);
	const auto secondIndex = static_cast<uint16_t>((chunk.code[4] << 8) | chunk.code[5]);
	EXPECT_EQ(std::get<long long>(chunk.constants[firstIndex]), 10);
	EXPECT_EQ(std::get<long long>(chunk.constants[secondIndex]), 20);

	EXPECT_EQ(static_cast<OpCode>(chunk.code[6]), OpCode::OP_ADD);
	EXPECT_EQ(static_cast<OpCode>(chunk.code[7]), OpCode::OP_SET_LOCAL);
}

TEST_F(CompilerTest, BytecodeVariableDeclaration)
{
	const auto root = ParseCode("var myVar = 100;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	auto chunk = compiler.Compile(root.get());

	bool foundSetLocal = false;
	for (size_t i = 0; i < chunk.code.size(); ++i)
	{
		if (static_cast<OpCode>(chunk.code[i]) == OpCode::OP_SET_LOCAL)
		{
			foundSetLocal = true;
			ASSERT_LT(i + 1, chunk.code.size());
			EXPECT_EQ(chunk.code[i + 1], 0);
		}
	}
	EXPECT_TRUE(foundSetLocal);
}

TEST_F(CompilerTest, BytecodeMathComplexity)
{
	const auto root = ParseCode("var x = 2 + 3 * 4;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	auto chunk = compiler.Compile(root.get());

	const std::vector expectedSequence = {
		OpCode::OP_CONSTANT,
		OpCode::OP_CONSTANT,
		OpCode::OP_CONSTANT,
		OpCode::OP_MULTIPLY,
		OpCode::OP_ADD,
		OpCode::OP_SET_LOCAL,
		OpCode::OP_RETURN
	};

	size_t seqIdx = 0;
	for (size_t i = 0; i < chunk.code.size(); ++i)
	{
		if (const auto op = static_cast<OpCode>(chunk.code[i]);
			op == expectedSequence[seqIdx])
		{
			seqIdx++;
			if (OpCodeHasOperand(op))
			{
				switch (GetOperandSize(op))
				{
				case OperandSize::Uint8:
					i += 1;
					break;
				case OperandSize::Uint16:
					i += 2;
					break;
				case OperandSize::None:
					break;
				}
			}
		}
		if (seqIdx == expectedSequence.size())
		{
			break;
		}
	}

	EXPECT_EQ(seqIdx, expectedSequence.size());
}

TEST_F(CompilerTest, BytecodeIfElse)
{
	const auto root = ParseCode(
		"var x = 10;"
		"if (x > 5) {"
		"  x = 1;"
		"} else {"
		"  x = 0;"
		"}");

	ASSERT_NE(root, nullptr)
		<< "Parsing failed! 'if' must be inside a function and followed by ';'.";

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	bool hasJumpIfFalse = false;
	bool hasJump = false;

	for (const auto byte : chunk.code)
	{
		if (byte == static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE))
		{
			hasJumpIfFalse = true;
		}
		if (byte == static_cast<uint8_t>(OpCode::OP_JUMP))
		{
			hasJump = true;
		}
	}

	EXPECT_TRUE(hasJumpIfFalse) << "Missing OP_JUMP_IF_FALSE";
	EXPECT_TRUE(hasJump) << "Missing OP_JUMP (for else branch)";
}

TEST_F(CompilerTest, BytecodeTransactionEmitsScopedOpcodes)
{
	const auto root = ParseCode(
		"shared var counter: int = 0;"
		"fn main() {"
		"  transaction(shared counter) {"
		"    counter = counter + 1;"
		"  }"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BEGIN_TXN));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_END_TXN));
}

TEST_F(CompilerTest, BytecodeMultiRegionTransactionBuildsMutexArray)
{
	const auto root = ParseCode(
		"shared var left: int = 0;"
		"shared var right: int = 0;"
		"fn main() {"
		"  transaction(shared left | shared right) {"
		"    left = left + 1;"
		"    right = right + 1;"
		"  }"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_ARRAY));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BEGIN_TXN));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_END_TXN));
}

TEST_F(CompilerTest, BytecodeActorConstructionAndCalls)
{
	const auto root = ParseCode(
		"actor Wallet {"
		"  state balance: int = 0;"
		"  msg deposit(amount: int) { balance = balance + amount; }"
		"  query getBalance() : int { return balance; }"
		"}"
		"var wallet = Wallet(0);"
		"wallet.deposit(5);"
		"print wallet.getBalance();");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_ACTOR));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ACTOR_SEND));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ACTOR_QUERY));
}

TEST_F(CompilerTest, BytecodeEffectsEmitHandlerAndInvokeOpcodes)
{
	const auto root = ParseCode(
		"effect IO { fn read() : int; }"
		"fn run() : int raises { IO } { return read(); }"
		"fn main() : int {"
		"  handle run() with { effect read() -> { resume(7); } }"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_BUILD_HANDLER));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_PUSH_HANDLER));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_EFFECT_INVOKE));
}

TEST_F(CompilerTest, BytecodeQualifiedEffectOperationsUseQualifiedKeys)
{
	const auto root = ParseCode(
		"effect Input { fn read() : int; }"
		"effect Backup { fn read() : int; }"
		"fn run() : int raises { Input } {"
		"  handle read() with { effect read() -> { resume(7); } }"
		"  return 0;"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	std::function<bool(const VM::Execution::Chunk&, const std::string&)> chunkContainsString =
		[&](const VM::Execution::Chunk& current, const std::string& needle) {
			for (const auto& constant : current.constants)
			{
				if (std::holds_alternative<std::shared_ptr<const std::string>>(constant)
					&& *std::get<std::shared_ptr<const std::string>>(constant) == needle)
				{
					return true;
				}
				if (std::holds_alternative<FunctionPtr>(constant))
				{
					if (const auto& fn = std::get<FunctionPtr>(constant);
						fn && chunkContainsString(*fn->chunk, needle))
					{
						return true;
					}
				}
				if (std::holds_alternative<ClosurePtr>(constant))
				{
					if (const auto& closure = std::get<ClosurePtr>(constant);
						closure && closure->function && chunkContainsString(*closure->function->chunk, needle))
					{
						return true;
					}
				}
			}
			return false;
		};

	bool sawQualifiedInvoke = false;
	bool sawQualifiedHandler = false;
	sawQualifiedInvoke = chunkContainsString(chunk, "Input.read");
	sawQualifiedHandler = chunkContainsString(chunk, "Input.read");

	EXPECT_TRUE(sawQualifiedInvoke);
	EXPECT_TRUE(sawQualifiedHandler);
}

TEST_F(CompilerTest, BytecodeContractsEmitAssertOpcodes)
{
	const auto root = ParseCode(
		"fn divide_safe(a: int, b: int) : int requires (b != 0) ensures (result >= 0) {"
		"  return a / b;"
		"}");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ASSERT));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_DUP));
}

TEST_F(CompilerTest, BytecodeStructInvariantEmitsAssertAndSwap)
{
	const auto root = ParseCode(
		"struct Box { value: int; } invariant (self.value > 0)"
		"var box = Box(1);");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_ASSERT));
	EXPECT_TRUE(ChunkContainsOpcode(chunk, OpCode::OP_SWAP));
}

TEST_F(CompilerTest, BytecodeComptimeBlockFoldsToConstant)
{
	const auto root = ParseCode("var x = comptime { var y = 2; return y + 3; };");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());
	bool foundFive = false;
	for (const auto& constant : chunk.constants)
	{
		if ((std::holds_alternative<long long>(constant) && std::get<long long>(constant) == 5)
			|| (std::holds_alternative<double>(constant) && std::get<double>(constant) == 5.0))
		{
			foundFive = true;
			break;
		}
	}
	EXPECT_TRUE(foundFive);
}

TEST_F(CompilerTest, BytecodeComptimeFunctionIsEvaluatedAtCompileTime)
{
	const auto root = ParseCode(
		"comptime fn twice(x: int) : int { return x * 2; }"
		"var x = comptime { return twice(4); };");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());
	bool foundEight = false;
	for (const auto& constant : chunk.constants)
	{
		if ((std::holds_alternative<long long>(constant) && std::get<long long>(constant) == 8)
			|| (std::holds_alternative<double>(constant) && std::get<double>(constant) == 8.0))
		{
			foundEight = true;
			break;
		}
	}
	EXPECT_TRUE(foundEight);
}

TEST_F(CompilerTest, BytecodeComptimeStructMemberAccessFoldsToConstant)
{
	const auto root = ParseCode(
		"struct Point { x: int; y: int; }"
		"var x = comptime {"
		"  var point = Point(7, 9);"
		"  return point.x;"
		"};");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());
	bool foundSeven = false;
	for (const auto& constant : chunk.constants)
	{
		if ((std::holds_alternative<long long>(constant) && std::get<long long>(constant) == 7)
			|| (std::holds_alternative<double>(constant) && std::get<double>(constant) == 7.0))
		{
			foundSeven = true;
			break;
		}
	}
	EXPECT_TRUE(foundSeven);
}

TEST_F(CompilerTest, BytecodeComptimeArrayAndEnumIndexFoldToConstant)
{
	const auto root = ParseCode(
		"enum Option { Some(int) | None }"
		"var a = comptime {"
		"  var values = [10, 20, 30];"
		"  return values[1];"
		"};"
		"var b = comptime {"
		"  var value = Some(42);"
		"  return value[0];"
		"};");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());
	bool foundTwenty = false;
	bool foundFortyTwo = false;
	for (const auto& constant : chunk.constants)
	{
		if (std::holds_alternative<long long>(constant))
		{
			const auto value = std::get<long long>(constant);
			foundTwenty = foundTwenty || value == 20;
			foundFortyTwo = foundFortyTwo || value == 42;
		}
		if (std::holds_alternative<double>(constant))
		{
			const auto value = std::get<double>(constant);
			foundTwenty = foundTwenty || value == 20.0;
			foundFortyTwo = foundFortyTwo || value == 42.0;
		}
	}
	EXPECT_TRUE(foundTwenty);
	EXPECT_TRUE(foundFortyTwo);
}
