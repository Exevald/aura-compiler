#include "Utils.h"

void utils::SkipWhitespacesAndComments(Reader& reader, size_t& line)
{
	while (!reader.EndOfFile())
	{
		char ch = reader.Peek();
		if (ch == ' ' || ch == '\t' || ch == '\r')
		{
			reader.Get();
			continue;
		}
		if (ch == '\n')
		{
			reader.Get();
			++line;
			continue;
		}

		if (ch == '/' && reader.Peek() == '/')
		{
			reader.Get();
			reader.Get();
			while (!reader.EndOfFile() && reader.Get() != '\n')
			{
			}
			++line;
			continue;
		}

		if (ch == '/' && reader.Peek() == '*')
		{
			reader.Get();
			reader.Get();
			while (!reader.EndOfFile())
			{
				if (reader.Get() == '*' && !reader.EndOfFile() && reader.Peek() == '/')
				{
					reader.Get();
					break;
				}
				if (reader.Peek() == '\n')
				{
					++line;
				}
			}
			continue;
		}

		break;
	}
}