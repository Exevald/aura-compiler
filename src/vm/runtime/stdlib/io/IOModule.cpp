#include "IOModule.h"

#include "../../../core/values/ValueHelper.h"
#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace VM::Runtime
{

using Core::Value;
using Core::ValueHelper;
using Execution::ExecutionContext;

namespace
{

std::string JoinArgs(const std::vector<Value>& args, const size_t startIndex = 0)
{
	std::string result;
	for (size_t i = startIndex; i < args.size(); ++i)
	{
		if (!result.empty())
		{
			result += " ";
		}
		result += ValueHelper::ToString(args[i]);
	}
	return result;
}

std::vector<Value> ExtractValuesArray(ExecutionContext& ctx, const Value& value, const std::string& domain)
{
	const auto array = RequireArray(ctx, value, domain + " expects an array of values");
	if (!array)
	{
		return {};
	}
	return array->elements;
}

std::string FormatPrintf(ExecutionContext& ctx, const std::vector<Value>& args)
{
	const auto format = RequireString(
		ctx,
		args[0],
		"std.io.printf expects a string format");
	if (!format)
	{
		return {};
	}

	std::string out;
	size_t argIndex = 1;
	for (size_t i = 0; i < format->size(); ++i)
	{
		if ((*format)[i] != '%')
		{
			out.push_back((*format)[i]);
			continue;
		}

		if (i + 1 >= format->size())
		{
			ctx.RaiseError("std.io.printf has dangling %");
			return {};
		}

		const char spec = (*format)[++i];
		if (spec == '%')
		{
			out.push_back('%');
			continue;
		}

		if (argIndex >= args.size())
		{
			ctx.RaiseError("std.io.printf missing arguments for format");
			return {};
		}

		const auto& value = args[argIndex++];
		switch (spec)
		{
		case 'v':
		case 's':
		case 'd':
		case 'f':
		case 't':
			out += ValueHelper::ToString(value);
			break;
		default:
			ctx.RaiseError(std::string("std.io.printf unknown format specifier: ") + spec);
			return {};
		}
	}

	if (argIndex < args.size())
	{
		if (!out.empty())
		{
			out += " ";
		}
		out += JoinArgs(args, argIndex);
	}

	return out;
}

} // namespace

void IOModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(ModuleName(), MakeModule(ModuleName()));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".print",
		MakeNative(
			"print",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto values = ExtractValuesArray(ctx, args[0], "std.io_native.print");
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				std::cout << JoinArgs(values);
				return std::monostate{};
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".println",
		MakeNative(
			"println",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto values = ExtractValuesArray(ctx, args[0], "std.io_native.println");
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				std::cout << JoinArgs(values) << "\n";
				return std::monostate{};
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".printf",
		MakeNative(
			"printf",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto values = ExtractValuesArray(ctx, args[1], "std.io_native.printf");
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				std::vector<Value> formattedArgs;
				formattedArgs.reserve(values.size() + 1);
				formattedArgs.push_back(args[0]);
				for (const auto& value : values)
				{
					formattedArgs.push_back(value);
				}
				const auto formatted = FormatPrintf(ctx, formattedArgs);
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				std::cout << formatted;
				return std::monostate{};
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".read",
		MakeNative(
			"read",
			0,
			[](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
				std::string value;
				if (!(std::cin >> value))
				{
					ctx.RaiseError("std.io.read failed to read input");
					return std::monostate{};
				}
				return std::make_shared<const std::string>(std::move(value));
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".readln",
		MakeNative(
			"readln",
			0,
			[](ExecutionContext& ctx, const std::vector<Value>&) -> Value {
				std::string value;
				if (!std::getline(std::cin, value))
				{
					ctx.RaiseError("std.io.readln failed to read input");
					return std::monostate{};
				}
				return std::make_shared<const std::string>(std::move(value));
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".read_file",
		MakeNative(
			"read_file",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto path = RequireString(
					ctx,
					args[0],
					"std.io.read_file expects a string path");
				if (!path)
				{
					return std::monostate{};
				}

				std::ifstream input(*path, std::ios::binary);
				if (!input)
				{
					ctx.RaiseError("std.io.read_file failed to open: " + *path);
					return std::monostate{};
				}

				std::ostringstream buffer;
				buffer << input.rdbuf();
				if (!input.good() && !input.eof())
				{
					ctx.RaiseError("std.io.read_file failed to read: " + *path);
					return std::monostate{};
				}

				return std::make_shared<const std::string>(buffer.str());
			}));
}

} // namespace VM::Runtime
