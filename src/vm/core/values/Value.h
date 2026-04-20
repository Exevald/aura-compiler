#pragma once

#pragma once

#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

namespace VM::Core
{

struct Array;
struct Instance;
struct EnumVariant;
struct Pointer;
struct Function;
struct Closure;
struct Module;
struct Iterator;
struct ThreadHandle;
struct MutexHandle;
struct Actor;
struct ActorMethodMap;
struct EffectHandlerMap;

using FunctionPtr = std::shared_ptr<Function>;
using ClosurePtr = std::shared_ptr<Closure>;
using ArrayPtr = std::shared_ptr<Array>;
using InstancePtr = std::shared_ptr<Instance>;
using EnumPtr = std::shared_ptr<EnumVariant>;
using PointerPtr = std::shared_ptr<Pointer>;
using StringPtr = std::shared_ptr<const std::string>;
using ModulePtr = std::shared_ptr<Module>;
using IteratorPtr = std::shared_ptr<Iterator>;
using ThreadPtr = std::shared_ptr<ThreadHandle>;
using MutexPtr = std::shared_ptr<MutexHandle>;
using ActorPtr = std::shared_ptr<Actor>;
using ActorMethodMapPtr = std::shared_ptr<ActorMethodMap>;
using HandlerMapPtr = std::shared_ptr<EffectHandlerMap>;

} // namespace VM::Core

namespace VM::Execution
{
struct Chunk;
class ExecutionContext;
} // namespace VM::Execution

namespace VM::Core
{

struct NativeFunction;

using Value = std::variant<
	std::monostate,
	bool,
	int64_t,
	double,
	StringPtr,
	FunctionPtr,
	ClosurePtr,
	std::shared_ptr<NativeFunction>,
	ArrayPtr,
	InstancePtr,
	EnumPtr,
	PointerPtr,
	ModulePtr,
	IteratorPtr,
	ThreadPtr,
	MutexPtr,
	ActorPtr,
	ActorMethodMapPtr,
	HandlerMapPtr>;

using NativeFunctionPtr = std::shared_ptr<NativeFunction>;

struct Iterator
{
	std::function<std::pair<bool, Value>()> next;
};

struct Function
{
	std::string name;
	std::unique_ptr<Execution::Chunk> chunk;
	int arity = 0;
	std::vector<std::string> captureNames;

	Function();
	~Function();
};

struct Closure
{
	FunctionPtr function;
	std::vector<Value> captures;
};

struct NativeFunction
{
	using Handler = std::function<Value(Execution::ExecutionContext&, const std::vector<Value>&)>;

	std::string name;
	int arity = 0;
	bool variadic = false;
	Handler invoke;
};

struct Array
{
	std::vector<Value> elements;
};

struct Instance
{
	std::vector<Value> fields;
};

struct EnumVariant
{
	int tag;
	std::vector<Value> args;
};

struct Pointer
{
	std::function<Value()> get;
	std::function<void(Value)> set;
	std::function<bool()> deallocate;
	std::string targetName;
};

struct Module
{
	std::string name;
};

struct ThreadHandle
{
	size_t id = 0;
};

struct MutexHandle
{
	size_t id = 0;
};

struct Actor
{
	struct MailboxItem
	{
		enum class Kind
		{
			Send,
			Query
		};

		Kind kind = Kind::Send;
		std::string methodName;
		std::vector<Value> args;
		uint64_t requestId = 0;
	};

	struct QueryResult
	{
		std::mutex mutex;
		std::condition_variable cv;
		bool ready = false;
		Value value;
		std::string error;
	};

	std::string typeName;
	InstancePtr state;
	ActorMethodMapPtr methods;
	std::deque<MailboxItem> mailbox;
	std::unordered_map<uint64_t, std::shared_ptr<QueryResult>> pendingQueries;
	std::vector<std::string> failures;
	size_t runtimeId = 0;
	std::mutex mutex;
	std::condition_variable cv;
	std::thread worker;
	std::thread::id workerThreadId;
	bool stopping = false;
	uint64_t nextRequestId = 1;

	~Actor()
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			stopping = true;
		}
		cv.notify_all();
		if (worker.joinable())
		{
			if (worker.get_id() == std::this_thread::get_id())
			{
				worker.detach();
			}
			else
			{
				worker.join();
			}
		}
	}
};

struct ActorMethodMap
{
	std::unordered_map<std::string, Value> methods;
};

struct EffectHandlerMap
{
	std::unordered_map<std::string, Value> handlers;
};

template <typename T>
concept PrimitiveValue = std::is_same_v<T, bool>
	|| std::is_same_v<T, int64_t>
	|| std::is_same_v<T, double>;

template <typename Op, typename T>
concept BinaryOp = requires(Op op, T a, T b) {
	{
		op(a, b)
	} -> std::convertible_to<T>;
};

template <typename Op, typename T>
concept UnaryOp = requires(Op op, T a) {
	{
		op(a)
	} -> std::convertible_to<T>;
};

} // namespace VM::Core
