#include "JsonModule.h"
#include "JsonModuleInternal.h"

#include <sstream>

namespace VM::Runtime
{

std::shared_ptr<const std::string> JsonInternal::MakeSharedString(const std::string& value)
{
	return std::make_shared<const std::string>(value);
}

std::string JsonInternal::EscapeJsonString(const std::string_view input)
{
	std::string out;
	out.reserve(input.size() + 2);
	out.push_back('"');
	for (const unsigned char ch : input)
	{
		switch (ch)
		{
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\b':
			out += "\\b";
			break;
		case '\f':
			out += "\\f";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			if (ch < 0x20)
			{
				std::ostringstream hex;
				hex << "\\u" << std::hex << std::uppercase;
				hex.width(4);
				hex.fill('0');
				hex << static_cast<int>(ch);
				out += hex.str();
			}
			else
			{
				out.push_back(static_cast<char>(ch));
			}
			break;
		}
	}
	out.push_back('"');
	return out;
}

} // namespace VM::Runtime
