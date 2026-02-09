#pragma once

#include <iostream>
#include <sstream>
#include <string>

class Reader
{
public:
	explicit Reader(const std::string& str);
	explicit Reader(const std::istream& strm);

	char Get();
	[[nodiscard]] char Peek();
	void Unget();

	[[nodiscard]] size_t Count() const;
	[[nodiscard]] size_t LineCount() const;

	[[nodiscard]] bool Empty();
	[[nodiscard]] bool EndOfFile();

	void Seek(size_t pos);
	void Record();
	[[nodiscard]] std::string StopRecord();

private:
	std::stringstream m_input;
	size_t m_count = 0;
	size_t m_lineCount = 0;
	size_t m_prevCount = 0;
	std::string m_record;
};