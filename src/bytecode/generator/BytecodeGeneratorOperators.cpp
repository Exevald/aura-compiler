#include "../BytecodeGenerator.h"

#include <memory>

using namespace VM::Core;

void BytecodeGenerator::EmitBinaryOp(const std::string& op) const
{
	if (op == "+")
	{
		CurrentChunk().Write(OpCode::OP_ADD);
	}
	else if (op == "-")
	{
		CurrentChunk().Write(OpCode::OP_SUBTRACT);
	}
	else if (op == "*")
	{
		CurrentChunk().Write(OpCode::OP_MULTIPLY);
	}
	else if (op == "/")
	{
		CurrentChunk().Write(OpCode::OP_DIVIDE);
	}
	else if (op == "div")
	{
		CurrentChunk().Write(OpCode::OP_DIV);
	}
	else if (op == "mod")
	{
		CurrentChunk().Write(OpCode::OP_MOD);
	}
	else if (op == "and" || op == "&&")
	{
		CurrentChunk().Write(OpCode::OP_AND);
	}
	else if (op == "or" || op == "||")
	{
		CurrentChunk().Write(OpCode::OP_OR);
	}
	else if (op == "==")
	{
		CurrentChunk().Write(OpCode::OP_EQUAL);
	}
	else if (op == "!=")
	{
		CurrentChunk().Write(OpCode::OP_NOT_EQUAL);
	}
	else if (op == "<")
	{
		CurrentChunk().Write(OpCode::OP_LESS);
	}
	else if (op == ">")
	{
		CurrentChunk().Write(OpCode::OP_GREATER);
	}
	else if (op == "<=")
	{
		CurrentChunk().Write(OpCode::OP_LESS_EQUAL);
	}
	else if (op == ">=")
	{
		CurrentChunk().Write(OpCode::OP_GREATER_EQUAL);
	}
}