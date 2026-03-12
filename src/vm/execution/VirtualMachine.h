#pragma once

#include "../core/Instruction.h"
#include "../core/OpCode.h"
#include "../core/StringPool.h"
#include "Chunk.h"
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

	void SetDebugMode(const bool enabled) { m_debugMode = enabled; }
	void SetMaxSteps(const size_t max) { m_maxSteps = max; }

private:
	ExecutionResult Run();
	Core::Instruction DecodeInstruction();
	int Dispatch(const Core::Instruction& instr);
	void DebugPrintInstruction(const Core::Instruction& instr) const;

	ExecutionContext m_context;
	Chunk* m_currentChunk{ nullptr };
	size_t m_ip{ 0 };
	size_t m_maxSteps{ 1000000 };
	size_t m_stepsExecuted{ 0 };
	bool m_debugMode{ false };
	std::unordered_map<Core::OpCode, ExtensionHandler> m_extensions;
	Core::StringPool m_stringPool;
};

} // namespace VM::Execution