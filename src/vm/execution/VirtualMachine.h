#pragma once

#include "../core/Instruction.h"
#include "../core/OpCode.h"
#include "../runtime/ExecutionContext.h"
#include "../runtime/SharedRuntime.h"
#include "../runtime/StringPool.h"
#include "chunk/Chunk.h"

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

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
	explicit VirtualMachine(
		std::shared_ptr<Runtime::SharedRuntime> runtime,
		bool installStdlib);
	~VirtualMachine() = default;

	VirtualMachine(const VirtualMachine&) = delete;
	VirtualMachine& operator=(const VirtualMachine&) = delete;
	VirtualMachine(VirtualMachine&&) noexcept = default;
	VirtualMachine& operator=(VirtualMachine&&) noexcept = default;

	bool Interpret(const Chunk* chunk);
	std::optional<std::string> RunCallable(
		const Core::Value& callee,
		const std::vector<Core::Value>& args,
		Core::Value& result);

	ExecutionContext& GetContext() { return m_context; }
	[[nodiscard]] const ExecutionContext& GetContext() const { return m_context; }

	using ExtensionHandler = std::function<int(Core::OpCode, ExecutionContext&)>;
	void RegisterExtension(Core::OpCode opcode, ExtensionHandler handler);

	void SetDebugMode(const bool enabled) { m_debugMode = enabled; }

private:
	void InstallStdlib() const;
	struct DataExecutor
	{
		VirtualMachine& vm;

		int BuildArray(uint8_t count) const;
		int BuildMap(uint8_t pairCount) const;
		int BuildStruct(uint8_t fieldCount) const;
		int BuildEnum(uint8_t tag, uint8_t argCount) const;
		int BuildActor(uint16_t blueprintOperand, uint8_t fieldCount) const;
		int HandleArrayIndexGet() const;
		int HandleArrayIndexSet() const;
		int HandleMemberGet(uint8_t fieldIdx) const;
		int HandleMemberSet(uint8_t fieldIdx) const;
		int HandleModuleMemberGet(uint16_t operand, const std::vector<Core::Value>& constants) const;
		int HandleEnumTagGet() const;
		int HandleEnumArgGet(uint8_t argIndex) const;
		int HandleAddressOfLocal(uint8_t slot) const;
		int HandleAddressOfGlobal(uint16_t operand, const std::vector<Core::Value>& constants) const;
		int HandleAddressOfMember() const;
		int HandleAddressOfUpvalue(uint8_t slot) const;
		int HandleDerefGet() const;
		int HandleDerefSet() const;
	};

	ExecutionResult Run();
	static Core::Instruction DecodeInstruction();
	int Dispatch(const Core::Instruction& instr);
	int HandleCall(uint16_t argCount);
	int HandleResolvedCall(
		const Core::Value& callee,
		uint16_t argCount,
		size_t calleeIdx);
	int HandleReturn();
	int HandleGlobal(
		Core::OpCode opcode,
		uint16_t operand,
		const std::vector<Core::Value>& constants);
	int HandleClosure(uint16_t operand, const std::vector<Core::Value>& constants);
	[[nodiscard]] std::optional<std::string> InvokeCallable(
		const Core::Value& callee,
		const std::vector<Core::Value>& args,
		Core::Value& result);
	void StartActorWorker(const Core::ActorPtr& actor) const;
	void RunActorWorker(const Core::ActorPtr& actor) const;
	int EnqueueActorSend(
		const Core::ActorPtr& actor,
		std::string methodName,
		std::vector<Core::Value> args);
	int EnqueueActorQuery(
		const Core::ActorPtr& actor,
		std::string methodName,
		std::vector<Core::Value> args);
	void DebugPrintInstruction(const Core::Instruction& instr) const;

	std::shared_ptr<Runtime::SharedRuntime> m_runtime;
	ExecutionContext m_context;
	Core::ActorPtr m_activeActor;
	Chunk* m_currentChunk{ nullptr };
	size_t m_ip{ 0 };
	size_t m_stepsExecuted{ 0 };
	bool m_debugMode{ false };
	std::unordered_map<Core::OpCode, ExtensionHandler> m_extensions;
	Core::StringPool m_stringPool;
};

} // namespace VM::Execution
