#include "Whitespace.h"

void whitespaceRule::SkipWhitespacesAndComments(Reader& reader)
{
	while (!reader.EndOfFile())
	{
		char ch = reader.Peek();
		if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
		{
			reader.Get();
			continue;
		}

		if (ch == '/' && !reader.EndOfFile())
		{
			reader.Get();
			if (reader.EndOfFile())
			{
				break;
			}
			if (reader.Peek() == '/')
			{
				reader.Get();
				while (!reader.EndOfFile() && reader.Get() != '\n')
				{
				}
				continue;
			}
			else if (reader.Peek() == '*')
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
			}
		}

		break;
	}
}
