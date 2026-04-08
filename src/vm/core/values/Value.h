#pragma once

#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <string>
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

} // namespace VM::Core

namespace VM::Execution
{
struct Chunk;
class ExecutionContext;
}

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
	MutexPtr>;

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

template <typename T>
concept PrimitiveValue = std::is_same_v<T, bool> || std::is_same_v<T, int64_t> || std::is_same_v<T, double>;

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
