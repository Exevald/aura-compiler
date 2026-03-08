#pragma once

#include "../core/Instruction.h"
#include "../core/OpCode.h"
#include "ExecutionContext.h"

#include <functional>
#include <memory>
#include <unordered_map>

namespace VM::Execution
{

enum class ExecutionResult
{
	Success,
	RuntimeError,
	Timeout,
	StackOverflow,
	UnknownOpcode
};

struct Chunk
{
	std::vector<uint8_t> code;
	std::vector<Core::Value> constants;
	std::string debugName;

	void Write(Core::OpCode opcode);
	void WriteConstant(const Core::Value& value);
	uint8_t AddConstant(const Core::Value& value);
	[[nodiscard]] size_t GetCodeSize() const { return code.size(); }
	[[nodiscard]] const std::vector<uint8_t>& GetCode() const { return code; }
	[[nodiscard]] const std::vector<Core::Value>& GetConstants() const { return constants; }
	void Clear();
};

class VirtualMachine
{
public:
	VirtualMachine();
	~VirtualMachine() = default;

	VirtualMachine(const VirtualMachine&) = delete;
	VirtualMachine& operator=(const VirtualMachine&) = delete;
	VirtualMachine(VirtualMachine&&) noexcept = default;
	VirtualMachine& operator=(VirtualMachine&&) noexcept = default;

	bool Interpret(Chunk* chunk);

	ExecutionContext& GetContext() { return m_context; }
	[[nodiscard]] const ExecutionContext& GetContext() const { return m_context; }

	using ExtensionHandler = std::function<int(Core::OpCode, ExecutionContext&)>;
	void RegisterExtension(Core::OpCode opcode, ExtensionHandler handler);

private:
	ExecutionResult Run();
	Core::Instruction DecodeInstruction();
	int Dispatch(const Core::Instruction& instr);

	ExecutionContext m_context;
	Chunk* m_currentChunk{ nullptr };
	size_t m_ip{ 0 };
	size_t m_maxSteps{ 1000000 };
	size_t m_stepsExecuted{ 0 };
	std::unordered_map<Core::OpCode, ExtensionHandler> m_extensions;
};

} // namespace VM::Execution