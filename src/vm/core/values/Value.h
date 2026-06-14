#pragma once

#pragma once

#include <concepts>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
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
struct TaskHandle;
struct ChannelHandle;
struct ContextHandle;
struct ListenerHandle;
struct ConnectionHandle;
struct HttpRequestHandle;
struct StringMapHandle;
struct MapHandle;
struct MySqlConnectionHandle;
struct MySqlPoolHandle;
struct MySqlStatementHandle;
struct MySqlResultHandle;
struct MySqlRowHandle;
struct RabbitMqConnectionHandle;
struct RabbitMqMessageHandle;
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
using TaskPtr = std::shared_ptr<TaskHandle>;
using ChannelPtr = std::shared_ptr<ChannelHandle>;
using ContextPtr = std::shared_ptr<ContextHandle>;
using ListenerPtr = std::shared_ptr<ListenerHandle>;
using ConnectionPtr = std::shared_ptr<ConnectionHandle>;
using HttpRequestPtr = std::shared_ptr<HttpRequestHandle>;
using StringMapPtr = std::shared_ptr<StringMapHandle>;
using MapPtr = std::shared_ptr<MapHandle>;
using MySqlConnectionPtr = std::shared_ptr<MySqlConnectionHandle>;
using MySqlPoolPtr = std::shared_ptr<MySqlPoolHandle>;
using MySqlStatementPtr = std::shared_ptr<MySqlStatementHandle>;
using MySqlResultPtr = std::shared_ptr<MySqlResultHandle>;
using MySqlRowPtr = std::shared_ptr<MySqlRowHandle>;
using RabbitMqConnectionPtr = std::shared_ptr<RabbitMqConnectionHandle>;
using RabbitMqMessagePtr = std::shared_ptr<RabbitMqMessageHandle>;
using ActorPtr = std::shared_ptr<Actor>;
using ActorMethodMapPtr = std::shared_ptr<ActorMethodMap>;
using HandlerMapPtr = std::shared_ptr<EffectHandlerMap>;

} // namespace VM::Core

namespace VM::Jit
{
struct CompiledFunction;
}

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
	TaskPtr,
	ChannelPtr,
	ContextPtr,
	ListenerPtr,
	ConnectionPtr,
	HttpRequestPtr,
	StringMapPtr,
	MapPtr,
	MySqlConnectionPtr,
	MySqlPoolPtr,
	MySqlStatementPtr,
	MySqlResultPtr,
	MySqlRowPtr,
	RabbitMqConnectionPtr,
	RabbitMqMessagePtr,
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
	int minArity = 0;
	bool variadic = false;
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
	using Handler0 = std::function<Value(Execution::ExecutionContext&)>;
	using Handler1 = std::function<Value(Execution::ExecutionContext&, const Value&)>;
	using Handler2 = std::function<Value(Execution::ExecutionContext&, const Value&, const Value&)>;
	using Handler3 = std::function<Value(Execution::ExecutionContext&, const Value&, const Value&, const Value&)>;
	using Handler4 = std::function<Value(Execution::ExecutionContext&, const Value&, const Value&, const Value&, const Value&)>;

	std::string name;
	int arity = 0;
	bool variadic = false;
	Handler invoke;
	Handler0 invoke0;
	Handler1 invoke1;
	Handler2 invoke2;
	Handler3 invoke3;
	Handler4 invoke4;
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
	bool cacheableMembers = false;
	mutable std::unordered_map<std::string, Value> memberCache;
};

struct ThreadHandle
{
	size_t id = 0;
};

struct MutexHandle
{
	size_t id = 0;
};

struct TaskHandle
{
	size_t id = 0;
};

struct ChannelHandle
{
	size_t id = 0;
};

struct ContextHandle
{
	size_t id = 0;
};

struct ListenerHandle
{
	int fd = -1;
	uint16_t port = 0;
	std::mutex mutex;

	~ListenerHandle()
	{
		if (fd >= 0)
		{
			::close(fd);
		}
	}
};

struct ConnectionHandle
{
	int fd = -1;
	uint16_t port = 0;
	std::mutex mutex;

	~ConnectionHandle()
	{
		if (fd >= 0)
		{
			::close(fd);
		}
	}
};

struct HttpRequestHandle
{
	std::string method;
	std::string path;
	std::string body;
	std::unordered_map<std::string, std::string> headers;
	ArrayPtr cachedSegments;
};

struct StringMapHandle
{
	std::mutex mutex;
	std::unordered_map<std::string, StringPtr> values;
};

struct MapHandle
{
	struct Entry
	{
		Value key;
		Value value;
	};

	std::mutex mutex;
	std::unordered_map<std::string, Entry> entries;
};

struct MySqlPoolHandle
{
	std::vector<void*> availableConnections;
	std::vector<void*> allConnections;
	std::mutex mutex;
	std::string lastError;

	~MySqlPoolHandle();
};

struct MySqlConnectionHandle
{
	void* connection = nullptr;
	MySqlPoolPtr owningPool;
	bool pooledLease = false;
	std::mutex mutex;
	std::string lastError;

	~MySqlConnectionHandle();
};

struct MySqlStatementHandle
{
	void* statement = nullptr;
	MySqlConnectionPtr connection;
	bool ownsConnectionLease = false;
	std::vector<Value> boundValues;
	std::string lastError;

	~MySqlStatementHandle();
};

struct MySqlRowHandle
{
	std::shared_ptr<std::vector<std::string>> columnNames;
	std::shared_ptr<std::unordered_map<std::string, size_t>> nameToIndex;
	std::vector<Value> values;
};

struct MySqlResultHandle
{
	std::vector<std::string> columnNames;
	std::vector<MySqlRowPtr> rows;
	size_t cursor = 0;
	int64_t affectedRows = 0;
	int64_t lastInsertId = 0;
	std::string lastError;
};

struct RabbitMqConnectionHandle
{
	void* connection = nullptr;
	int channel = 1;
	std::mutex mutex;
	std::string url;
	std::string lastError;
	bool open = false;

	~RabbitMqConnectionHandle();
};

struct RabbitMqMessageHandle
{
	RabbitMqConnectionPtr connection;
	int channel = 1;
	int64_t deliveryTag = 0;
	std::string queue;
	std::string routingKey;
	std::string body;
	bool acknowledged = false;
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
	std::jthread worker;
	std::thread::id workerThreadId;
	bool stopping = false;
	uint64_t nextRequestId = 1;

	~Actor()
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			stopping = true;
		}
		worker.request_stop();
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
