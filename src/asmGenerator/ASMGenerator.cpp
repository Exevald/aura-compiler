#include "ASMGenerator.h"

#include <iomanip>
#include <sstream>

namespace
{

std::string EscapeString(const std::string& value)
{
	std::string escaped;
	escaped.reserve(value.size());
	for (const char ch : value)
	{
		switch (ch)
		{
		case '\\':
			escaped += "\\\\";
			break;
		case '"':
			escaped += "\\\"";
			break;
		case '\n':
			escaped += "\\n";
			break;
		case '\t':
			escaped += "\\t";
			break;
		default:
			escaped.push_back(ch);
			break;
		}
	}
	return escaped;
}

std::string FormatFloat(const double value)
{
	std::ostringstream output;
	output << std::setprecision(15) << std::defaultfloat << value;
	return output.str();
}

std::string DeclarationInstruction(const VarDeclNode& node)
{
	std::string opcode = "DECLARE_";
	if (node.isConst)
	{
		opcode += "CONST";
	}
	else
	{
		switch (node.storageClass)
		{
		case VarDeclNode::StorageClass::Shared:
			opcode += "SHARED_VAR";
			break;
		case VarDeclNode::StorageClass::ThreadLocal:
			opcode += "THREAD_LOCAL_VAR";
			break;
		case VarDeclNode::StorageClass::Default:
			opcode += "VAR";
			break;
		}
	}

	opcode += " ";
	opcode += node.name;
	if (!node.explicitType.empty() && node.explicitType != "auto")
	{
		opcode += ":";
		opcode += node.explicitType;
	}
	return opcode;
}

std::string UnaryInstruction(const std::string& op)
{
	if (op == "-")
	{
		return "UNARY_NEG";
	}
	if (op == "+")
	{
		return "UNARY_POS";
	}
	if (op == "not" || op == "!")
	{
		return "UNARY_NOT";
	}
	if (op == "*")
	{
		return "DEREF";
	}
	if (op == "&")
	{
		return "ADDR_OF";
	}
	return "UNARY " + op;
}

std::string BinaryInstruction(const std::string& op)
{
	if (op == "+")
	{
		return "ADD";
	}
	if (op == "-")
	{
		return "SUB";
	}
	if (op == "*")
	{
		return "MUL";
	}
	if (op == "/")
	{
		return "DIV";
	}
	if (op == "mod")
	{
		return "MOD";
	}
	if (op == "div")
	{
		return "INT_DIV";
	}
	if (op == "==")
	{
		return "CMP_EQ";
	}
	if (op == "!=")
	{
		return "CMP_NE";
	}
	if (op == "<")
	{
		return "CMP_LT";
	}
	if (op == "<=")
	{
		return "CMP_LE";
	}
	if (op == ">")
	{
		return "CMP_GT";
	}
	if (op == ">=")
	{
		return "CMP_GE";
	}
	if (op == "and" || op == "&&")
	{
		return "LOGICAL_AND";
	}
	if (op == "or" || op == "||")
	{
		return "LOGICAL_OR";
	}
	return "BINARY " + op;
}

ASTNode* UnwrapOptionalNode(ASTNode* node)
{
	if (!node)
	{
		return nullptr;
	}

	if (auto* raw = dynamic_cast<RawNode*>(node))
	{
		for (const auto& child : raw->children)
		{
			if (ASTNode* candidate = UnwrapOptionalNode(child.get()))
			{
				return candidate;
			}
		}
		return nullptr;
	}

	if (dynamic_cast<LeafNode*>(node))
	{
		return nullptr;
	}

	return node;
}

} // namespace

ASMGenerator::ASMGenerator(std::ofstream outFile)
	: m_outFile(std::move(outFile))
{
}

void ASMGenerator::Compile(ASTNode* root)
{
	m_labelCounter = 0;
	if (root)
	{
		root->Accept(*this);
	}
}

void ASMGenerator::Visit(IntegerLiteralNode& node)
{
	EmitInstruction("PUSH_INT " + std::to_string(node.value));
}

void ASMGenerator::Visit(FloatLiteralNode& node)
{
	EmitInstruction("PUSH_FLOAT " + FormatFloat(node.value));
}

void ASMGenerator::Visit(IdentifierNode& node)
{
	EmitInstruction("LOAD " + node.name);
}

void ASMGenerator::Visit(AssignmentNode& node)
{
	if (node.value)
	{
		node.value->Accept(*this);
	}

	if (!node.name.empty())
	{
		EmitInstruction("STORE " + node.name);
		return;
	}

	EmitInstruction("assignment");
}

void ASMGenerator::Visit(VarDeclNode& node)
{
	EmitInstruction(DeclarationInstruction(node));

	if (node.initializer)
	{
		node.initializer->Accept(*this);
		EmitInstruction("STORE " + node.name);
	}
}

void ASMGenerator::Visit(IfStatementNode& node)
{
	ASTNode* elseBlock = UnwrapOptionalNode(node.elseBlock.get());

	if (node.condition)
	{
		node.condition->Accept(*this);
	}

	const std::string elseLabel = NextLabel();
	EmitInstruction("JUMP_IF_FALSE " + elseLabel);

	if (node.thenBlock)
	{
		node.thenBlock->Accept(*this);
	}

	if (!elseBlock)
	{
		EmitInstruction("LABEL " + elseLabel);
		return;
	}

	const std::string endLabel = NextLabel();
	EmitInstruction("JUMP " + endLabel);
	EmitInstruction("LABEL " + elseLabel);
	elseBlock->Accept(*this);
	EmitInstruction("LABEL " + endLabel);
}

void ASMGenerator::Visit(StringLiteralNode& node)
{
	EmitInstruction("PUSH_STRING \"" + EscapeString(node.value) + "\"");
}

void ASMGenerator::Visit(UnaryExprNode& node)
{
	if (node.operand)
	{
		node.operand->Accept(*this);
	}
	EmitInstruction(UnaryInstruction(node.op));
}

void ASMGenerator::Visit(BinaryExprNode& node)
{
	if (node.left)
	{
		node.left->Accept(*this);
	}
	if (node.right)
	{
		node.right->Accept(*this);
	}
	EmitInstruction(BinaryInstruction(node.op));
}

void ASMGenerator::Visit(TypeAliasNode& node)
{
}

void ASMGenerator::Visit(StructDeclNode& node)
{
}

void ASMGenerator::Visit(EnumDeclNode& node)
{
}

void ASMGenerator::Visit(InterfaceDeclNode& node)
{
}

void ASMGenerator::Visit(EffectDeclNode& node)
{
}

void ASMGenerator::Visit(ActorDeclNode& node)
{
}

void ASMGenerator::Visit(BlockNode& node)
{
	for (const auto& stmt : node.statements)
	{
		if (!stmt)
		{
			continue;
		}
		stmt->Accept(*this);
	}
}

void ASMGenerator::Visit(ExportDeclNode& node)
{
}

void ASMGenerator::Visit(WhileStatementNode& node)
{
}

void ASMGenerator::Visit(FunctionDeclNode& node)
{
}

void ASMGenerator::Visit(FunctionExprNode& node)
{
}

void ASMGenerator::Visit(CallNode& node)
{
	EmitInstruction("call");
}

void ASMGenerator::Visit(GoExprNode& node)
{
	if (node.call)
	{
		node.call->Accept(*this);
	}
	EmitInstruction("GO");
}

void ASMGenerator::Visit(AwaitExprNode& node)
{
	if (node.operand)
	{
		node.operand->Accept(*this);
	}
	EmitInstruction("AWAIT");
}

void ASMGenerator::Visit(MemberAccessNode& node)
{
}

void ASMGenerator::Visit(ModuleDeclNode& node)
{
}

void ASMGenerator::Visit(ImportDeclNode& node)
{
}

void ASMGenerator::Visit(ReturnNode& node)
{
}

void ASMGenerator::Visit(PrintNode& node)
{
	if (node.value)
	{
		node.value->Accept(*this);
	}
	EmitInstruction("PRINT");
}

void ASMGenerator::Visit(UnsafeNode& node)
{
}

void ASMGenerator::Visit(ArrayLiteralNode& node)
{
}

void ASMGenerator::Visit(MapLiteralNode& node)
{
	(void)node;
}

void ASMGenerator::Visit(IndexNode& node)
{
}

void ASMGenerator::Visit(IterNode& node)
{
}

void ASMGenerator::Visit(TransactionNode& node)
{
}

void ASMGenerator::Visit(HandleNode& node)
{
}

void ASMGenerator::Visit(LeafNode& node)
{
	if (node.type == "true")
	{
		EmitInstruction("PUSH_BOOL true");
	}
	else if (node.type == "false")
	{
		EmitInstruction("PUSH_BOOL false");
	}
	else if (node.type == "null")
	{
		EmitInstruction("PUSH_NULL");
	}
}

void ASMGenerator::Visit(RawNode& node)
{
	EmitInstruction("raw");
}

void ASMGenerator::Visit(ComptimeNode& node)
{
}

void ASMGenerator::Visit(ContractNode& node)
{
}

void ASMGenerator::EmitInstruction(const std::string& instruction)
{
	m_outFile << instruction << '\n';
}

std::string ASMGenerator::NextLabel()
{
	return "L" + std::to_string(m_labelCounter++);
}
