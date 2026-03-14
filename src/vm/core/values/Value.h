#pragma once

#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <variant>

namespace VM::Core
{

struct Array;
struct Instance;
struct EnumVariant;
struct Pointer;
struct Function;
struct Iterator;

using FunctionPtr = std::shared_ptr<Function>;
using ArrayPtr = std::shared_ptr<Array>;
using InstancePtr = std::shared_ptr<Instance>;
using EnumPtr = std::shared_ptr<EnumVariant>;
using PointerPtr = std::shared_ptr<Pointer>;
using StringPtr = std::shared_ptr<const std::string>;
using IteratorPtr = std::shared_ptr<Iterator>;

} // namespace VM::Core

namespace VM::Execution
{
struct Chunk;
}

namespace VM::Core
{

using Value = std::variant<
	std::monostate,
	bool,
	int64_t,
	double,
	StringPtr,
	FunctionPtr,
	ArrayPtr,
	InstancePtr,
	EnumPtr,
	PointerPtr,
	IteratorPtr>;

struct Iterator
{
	std::function<std::pair<bool, Value>()> next;
};

struct Function
{
	std::string name;
	std::unique_ptr<Execution::Chunk> chunk;
	int arity = 0;

	Function();
	~Function();
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
	std::string targetName;
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