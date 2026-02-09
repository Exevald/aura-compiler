#include "Reader.h"
#include <stdexcept>

Reader::Reader(const std::string& str)
	: m_input(str)
{
}

Reader::Reader(const std::istream& strm)
{
	m_input << strm.rdbuf();
}

char Reader::Get()
{
	if (EndOfFile())
	{
		throw std::runtime_error("EOF Error: tried to get char from an empty reader");
	}
	const auto ch = static_cast<char>(m_input.get());

	if (ch == '\n')
	{
		m_prevCount = m_count;
		++m_lineCount;
		m_count = 0;
	}
	else
	{
		++m_count;
	}

	m_record += ch;
	return ch;
}

char Reader::Peek()
{
	return static_cast<char>(m_input.peek());
}

void Reader::Unget()
{
	if (m_record.empty())
	{
		throw std::runtime_error("Cannot unget: no characters consumed");
	}

	const char lastChar = m_record.back();

	if (lastChar == '\n')
	{
		if (m_lineCount == 0)
		{
			throw std::runtime_error("Cannot unget newline: no newlines to undo");
		}
		--m_lineCount;
		m_count = m_prevCount;
	}
	else
	{
		if (m_count == 0)
		{
			throw std::runtime_error("Cannot unget: count underflow");
		}
		--m_count;
	}

	m_record.pop_back();
	m_input.unget();
}

size_t Reader::Count() const
{
	return m_count;
}

size_t Reader::LineCount() const
{
	return m_lineCount;
}

bool Reader::Empty()
{
	return EndOfFile();
}

bool Reader::EndOfFile()
{
	return m_input.peek() == std::char_traits<char>::eof();
}

void Reader::Seek(size_t pos)
{
	m_input.clear();
	m_input.seekg(0, std::ios_base::beg);

	m_count = 0;
	m_lineCount = 0;
	m_prevCount = 0;
	m_record.clear();

	for (size_t i = 0; i < pos; ++i)
	{
		if (m_input.peek() == std::char_traits<char>::eof())
		{
			throw std::runtime_error("Seek beyond EOF");
		}

		const char ch = static_cast<char>(m_input.get());
		if (ch == '\n')
		{
			m_prevCount = m_count;
			++m_lineCount;
			m_count = 0;
		}
		else
		{
			++m_count;
		}
	}
}

void Reader::Record()
{
	m_record.clear();
}

std::string Reader::StopRecord()
{
	return m_record;
}