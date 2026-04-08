#include "IOModule.h"

#include "../../core/values/ValueHelper.h"
#include "../ExecutionContext.h"
#include "../NativeModuleSupport.h"

#include <iostream>

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

void IOModule::Install(ExecutionContext& context)
{
	context.DefineGlobal(ModuleName(), MakeModule(ModuleName()));

	context.DefineGlobal(
		std::string(ModuleName()) + ".print",
		MakeNative(
			"print",
			0,
			[](ExecutionContext&, const std::vector<Value>& args) -> Value {
				std::cout << JoinArgs(args);
				return std::monostate{};
			},
			true));

	context.DefineGlobal(
		std::string(ModuleName()) + ".println",
		MakeNative(
			"println",
			0,
			[](ExecutionContext&, const std::vector<Value>& args) -> Value {
				std::cout << JoinArgs(args) << "\n";
				return std::monostate{};
			},
			true));

	context.DefineGlobal(
		std::string(ModuleName()) + ".printf",
		MakeNative(
			"printf",
			1,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto formatted = FormatPrintf(ctx, args);
				if (ctx.HasError())
				{
					return std::monostate{};
				}
				std::cout << formatted;
				return std::monostate{};
			},
			true));
}

} // namespace VM::Runtime
