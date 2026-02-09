#include "reader/Reader.h"
#include <gtest/gtest.h>

TEST(Reader, GetPeekUnget)
{
	Reader reader("abc");

	EXPECT_EQ(reader.Peek(), 'a');
	EXPECT_EQ(reader.Get(), 'a');
	EXPECT_EQ(reader.Peek(), 'b');
	EXPECT_EQ(reader.Get(), 'b');

	reader.Unget();
	EXPECT_EQ(reader.Peek(), 'b');
	EXPECT_EQ(reader.Get(), 'b');
	EXPECT_EQ(reader.Get(), 'c');
	EXPECT_TRUE(reader.EndOfFile());
}

TEST(Reader, LineCountTracking)
{
	Reader reader("line1\nline2\nline3\n");
	while (!reader.EndOfFile())
	{
		reader.Get();
	}
	EXPECT_EQ(reader.LineCount(), 3);
	EXPECT_EQ(reader.Count(), 0);
}

TEST(Reader, SeekFunctionality)
{
	Reader reader("0123456789");

	reader.Get();
	reader.Get();
	EXPECT_EQ(reader.Count(), 2);

	reader.Seek(0);
	EXPECT_EQ(reader.Count(), 0);
	EXPECT_EQ(reader.Get(), '0');

	reader.Seek(5);
	EXPECT_EQ(reader.Count(), 5);
	EXPECT_EQ(reader.Get(), '5');
}

TEST(Reader, RecordFunctionality)
{
	Reader reader("hello world");

	reader.Record();
	for (int i = 0; i < 5; ++i)
	{
		reader.Get();
	}
	EXPECT_EQ(reader.StopRecord(), "hello");

	reader.Record();
	while (!reader.EndOfFile())
	{
		reader.Get();
	}
	EXPECT_EQ(reader.StopRecord(), " world");
}

TEST(Reader, EmptyAndEOF)
{
	Reader empty_reader("");
	EXPECT_TRUE(empty_reader.Empty());
	EXPECT_TRUE(empty_reader.EndOfFile());

	Reader non_empty_reader("x");
	EXPECT_FALSE(non_empty_reader.Empty());
	EXPECT_FALSE(non_empty_reader.EndOfFile());
	non_empty_reader.Get();
	EXPECT_TRUE(non_empty_reader.EndOfFile());
}

TEST(Reader, UngetAfterNewline)
{
	Reader reader("a\nb");

	EXPECT_EQ(reader.Get(), 'a');
	EXPECT_EQ(reader.Get(), '\n');
	EXPECT_EQ(reader.LineCount(), 1);
	EXPECT_EQ(reader.Count(), 0);

	reader.Unget();
	EXPECT_EQ(reader.LineCount(), 0);
	EXPECT_EQ(reader.Count(), 1);

	EXPECT_EQ(reader.Get(), '\n');
	EXPECT_EQ(reader.Get(), 'b');
}