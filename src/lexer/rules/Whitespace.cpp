#include "Whitespace.h"

void whitespaceRule::SkipWhitespacesAndComments(Reader& reader)
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
			continue;
		}

		if (ch == '/')
		{
			reader.Get();

			if (reader.EndOfFile())
			{
				reader.Unget();
				break;
			}

			char next = reader.Peek();
			if (next == '/')
			{
				reader.Get();
				while (!reader.EndOfFile())
				{
					char c = reader.Get();
					if (c == '\n')
					{
						break;
					}
				}
				continue;
			}
			else if (next == '*')
			{
				reader.Get();
				while (!reader.EndOfFile())
				{
					char c = reader.Get();
					if (c == '*' && !reader.EndOfFile() && reader.Peek() == '/')
					{
						reader.Get();
						break;
					}
				}
				continue;
			}
			else
			{
				reader.Unget();
				break;
			}
		}

		break;
	}
}