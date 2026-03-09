#include "Chunk.h"

#include <gtest/gtest.h>
#include <stdexcept>

using enum VM::Core::OpCode;
using VM::Core::ValueHelper;
using VM::Execution::Chunk;

TEST(ChunkTest, WriteOpcode_AppendsByte)
{
	Chunk chunk;
	chunk.Write(OP_ADD);

	ASSERT_EQ(chunk.GetCode().size(), 1);
	EXPECT_EQ(chunk.GetCode()[0], static_cast<uint8_t>(OP_ADD));
}

TEST(ChunkTest, WriteConstant_AddsToConstantsAndCode)
{
	Chunk chunk;
	chunk.WriteConstant(42.0);

	ASSERT_EQ(chunk.GetCode().size(), 2);
	EXPECT_EQ(chunk.GetCode()[0], static_cast<uint8_t>(OP_CONSTANT));
	EXPECT_EQ(chunk.GetCode()[1], 0);

	ASSERT_EQ(chunk.GetConstants().size(), 1);
	EXPECT_TRUE(ValueHelper::IsType<double>(chunk.GetConstants()[0]));
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(chunk.GetConstants()[0]), 42.0);
}

TEST(ChunkTest, MultipleConstants_HaveSequentialIndices)
{
	Chunk chunk;
	chunk.WriteConstant(1.0);
	chunk.WriteConstant(2.0);
	chunk.WriteConstant(3.0);

	ASSERT_EQ(chunk.GetConstants().size(), 3);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(chunk.GetConstants()[0]), 1.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(chunk.GetConstants()[1]), 2.0);
	EXPECT_DOUBLE_EQ(ValueHelper::As<double>(chunk.GetConstants()[2]), 3.0);
}

TEST(ChunkTest, AddConstant_Overflow_Throws)
{
	Chunk chunk;

	for (int i = 0; i < std::numeric_limits<uint8_t>::max(); ++i)
	{
		chunk.AddConstant(i);
	}

	EXPECT_THROW(chunk.AddConstant(999.0), std::overflow_error);
}

TEST(ChunkTest, Clear_ResetsChunk)
{
	Chunk chunk;
	chunk.WriteConstant(1.0);
	chunk.Write(OP_ADD);
	chunk.debugName = "test";

	chunk.Clear();

	EXPECT_TRUE(chunk.GetCode().empty());
	EXPECT_TRUE(chunk.GetConstants().empty());
	EXPECT_TRUE(chunk.debugName.empty());
}

TEST(ChunkTest, GetCodeSize_ReturnsCorrectSize)
{
	Chunk chunk;
	EXPECT_EQ(chunk.GetCodeSize(), 0);

	chunk.Write(OP_ADD);
	EXPECT_EQ(chunk.GetCodeSize(), 1);

	chunk.WriteConstant(42.0);
	EXPECT_EQ(chunk.GetCodeSize(), 3);
}

TEST(ChunkTest, WriteConstant_DifferentTypes)
{
	Chunk chunk;

	chunk.WriteConstant(true);
	chunk.WriteConstant(int64_t{ -100 });
	chunk.WriteConstant(3.14);

	ASSERT_EQ(chunk.GetConstants().size(), 3);
	EXPECT_TRUE(ValueHelper::IsType<bool>(chunk.GetConstants()[0]));
	EXPECT_TRUE(ValueHelper::IsType<int64_t>(chunk.GetConstants()[1]));
	EXPECT_TRUE(ValueHelper::IsType<double>(chunk.GetConstants()[2]));
}

TEST(ChunkTest, ConstAccessors_ReturnConstRefs)
{
	const Chunk chunk;

	const auto& code = chunk.GetCode();
	const auto& constants = chunk.GetConstants();

	EXPECT_TRUE(code.empty());
	EXPECT_TRUE(constants.empty());
}
