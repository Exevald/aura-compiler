#include "Compiler.h"
#include "VirtualMachine.h"

#include <filesystem>
#include <fstream>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <sstream>
#include <unistd.h>

namespace
{

std::filesystem::path MakeTempRoot()
{
	const auto root = std::filesystem::temp_directory_path()
		/ ("aura_vm_module_tests_" + std::to_string(::getpid()));
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	return root;
}

void WriteFile(const std::filesystem::path& path, const std::string& contents)
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream output(path);
	output << contents;
}

std::filesystem::path GrammarPath()
{
	return std::filesystem::path(__FILE__)
			   .parent_path()
			   .parent_path()
			   .parent_path()
			   .parent_path()
			   .parent_path()
		/ "grammar.md";
}

std::string RunProgram(const std::filesystem::path& entryFile)
{
	std::string emptySource;
	Lexer lexer(emptySource);
	BytecodeGenerator generator;
	Compiler compiler(GrammarPath().string(), generator, lexer);

	auto chunk = compiler.CompileFileToChunk(entryFile);
	VM::Execution::VirtualMachine vm;

	std::ostringstream oss;
	std::streambuf* old = std::cout.rdbuf(oss.rdbuf());

	if (const bool ok = vm.Interpret(&chunk);
		!ok && vm.GetContext().HasError())
	{
		std::cout << "Error: " << vm.GetContext().GetError();
	}

	std::cout.rdbuf(old);
	return oss.str();
}

} // namespace

TEST(ModuleImportIntegrationTest, ImportedModuleExecutesThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "math_utils.aura",
		"module samples.math_utils;"
		"fn sum(a: int, b: int) : int { return a + b; }");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import samples.math_utils as math;"
		"print math.sum(19, 23);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, ImportedClosureFactoryExecutesAcrossFiles)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "factory.aura",
		"module samples.factory;"
		"fn makeAdder(base: int) {"
		"  var add = fn(value: int) -> value + base;"
		"  return add;"
		"}");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import samples.factory as factory;"
		"var add7 = factory.makeAdder(7);"
		"print add7(5);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("12"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, ImportedStructWholeFieldAssignmentExecutesAcrossFiles)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "models.aura",
		"module samples.models;"
		"struct Inner { x: int; }"
		"struct Outer { inner: Inner; }");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import samples.models as models;"
		"var outer = models.Outer(models.Inner(10));"
		"outer.inner = models.Inner(42);"
		"print outer.inner.x;");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, ImportedEnumConstructorAndTagExecutesAcrossFiles)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "models.aura",
		"module samples.models;"
		"enum Option { None | Some(int) }");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import samples.models as models;"
		"var value = models.Some(42);"
		"print value.tag;");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("1"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, ImportedEnumArgumentExecutesAcrossFiles)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "models.aura",
		"module samples.models;"
		"enum Option { None | Some(int) }");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import samples.models as models;"
		"var value = models.Some(42);"
		"print value[0];");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, AddressOfLocalAndStructFieldExecute)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"struct Point { x: int; }"
		"fn run() : int {"
		"  unsafe {"
		"    var x: int = 10;"
		"    var px: ptr<int> = &x;"
		"    *(px) = 21;"
		"    var point = Point(5);"
		"    var field_ptr: ptr<int> = &point.x;"
		"    *(field_ptr) = 34;"
		"    print point.x;"
		"    return *(px);"
		"  }"
		"}"
		"print run();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("34"));
	EXPECT_THAT(output, ::testing::HasSubstr("21"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, AddressOfCapturedVariableExecute)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"fn makeCounter() {"
		"  var x: int = 41;"
		"  var next = fn() -> {"
		"    unsafe {"
		"      var px: ptr<int> = &x;"
		"      *px = *px + 1;"
		"      return x;"
		"    }"
		"  };"
		"  return next;"
		"}"
		"var counter = makeCounter();"
		"print counter();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, GenericFunctionAndConstraintExecute)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"type Box<T> = [T];"
		"fn identity<T>(value: T) : T { return value; }"
		"fn doubleValue<T: int>(value: T) : int { return value + value; }"
		"var values: Box<int> = [identity(21)];"
		"print values[0];"
		"print doubleValue(identity(21));");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("21"));
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, ExportedFunctionAndVarExecuteAcrossFiles)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "api.aura",
		"module samples.api;"
		"export var version: int = 7;"
		"fn hidden() : int { return 35; }"
		"export fn answer() : int { return hidden() + version; };");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import samples.api as api;"
		"print api.version;"
		"print api.answer();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("7"));
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinRuntimeModuleExecutesThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.runtime as rt;"
		"var mem = rt.alloc(16);"
		"print rt.active_allocations();"
		"print rt.active_bytes();"
		"print rt.is_send([1, 2, 3]);"
		"rt.free(mem);"
		"rt.assert_no_leaks();"
		"print rt.active_allocations();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("1"));
	EXPECT_THAT(output, ::testing::HasSubstr("16"));
	EXPECT_THAT(output, ::testing::HasSubstr("true"));
	EXPECT_THAT(output, ::testing::HasSubstr("0"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinRuntimeUnsafePointerUseExecutesThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.runtime as rt;"
		"unsafe {"
		"  var mem = rt.alloc(8);"
		"  *mem = 42;"
		"  print *mem;"
		"  print rt.is_send(mem);"
		"  rt.free(mem);"
		"}"
		"rt.assert_no_leaks();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	EXPECT_THAT(output, ::testing::HasSubstr("false"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinSyncModuleRejectsDeadlockAtCompileTime)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.sync as sync;"
		"var t1 = sync.spawn();"
		"var t2 = sync.spawn();"
		"var m1 = sync.mutex();"
		"var m2 = sync.mutex();"
		"print sync.lock(t1, m1);"
		"print sync.lock(t2, m2);"
		"print sync.lock(t1, m2);"
		"print sync.would_deadlock(t2, m1);"
		"print sync.lock(t2, m1);");

	EXPECT_THROW((void)RunProgram(root / "samples" / "main.aura"), std::runtime_error);
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinSyncModuleRejectsUnlockByNonOwnerAtCompileTime)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.sync as sync;"
		"var t1 = sync.spawn();"
		"var t2 = sync.spawn();"
		"var m = sync.mutex();"
		"print sync.lock(t1, m);"
		"print sync.unlock(t2, m);");

	EXPECT_THROW((void)RunProgram(root / "samples" / "main.aura"), std::runtime_error);
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinSyncModuleCompiletimeDeadlockRejectedAcrossImportedHelpers)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "helpers.aura",
		"module samples.helpers;"
		"import std.sync as sync;"
		"export fn lock_pair(thread, left, right) : void {"
		"  sync.lock(thread, left);"
		"  sync.lock(thread, right);"
		"};");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import samples.helpers as helpers;"
		"import std.sync as sync;"
		"var t1 = sync.spawn();"
		"var t2 = sync.spawn();"
		"var m1 = sync.mutex();"
		"var m2 = sync.mutex();"
		"helpers.lock_pair(t1, m1, m2);"
		"helpers.lock_pair(t2, m2, m1);");

	EXPECT_THROW((void)RunProgram(root / "samples" / "main.aura"), std::runtime_error);
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BuiltinSyncModuleCompiletimeDeadlockRejectedAcrossModuleChain)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "locks.aura",
		"module samples.locks;"
		"import std.sync as sync;"
		"export fn lock_pair(thread, left, right) : void {"
		"  sync.lock(thread, left);"
		"  sync.lock(thread, right);"
		"};");
	WriteFile(
		root / "samples" / "workflow.aura",
		"module samples.workflow;"
		"import samples.locks as locks;"
		"export fn run(thread, first, second) : void {"
		"  locks.lock_pair(thread, first, second);"
		"};");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import samples.workflow as workflow;"
		"import std.sync as sync;"
		"var t1 = sync.spawn();"
		"var t2 = sync.spawn();"
		"var m1 = sync.mutex();"
		"var m2 = sync.mutex();"
		"workflow.run(t1, m1, m2);"
		"workflow.run(t2, m2, m1);");

	EXPECT_THROW((void)RunProgram(root / "samples" / "main.aura"), std::runtime_error);
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, ModuleBackedInterfaceExecutes)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "reader.aura",
		"module samples.reader;"
		"fn read(buf: [int]) : int { return 42; }"
		"fn close() : void { return; }");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import samples.reader as reader_mod;"
		"interface Reader {"
		"  fn read(buf: [int]) : int;"
		"  fn close() : void;"
		"}"
		"var reader: Reader = reader_mod;"
		"print reader.read([1, 2]);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, FunctionBackedSingleMethodInterfaceExecutes)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"interface Action { fn run(v: int) : int; }"
		"var action: Action = fn(v: int) -> v + 1;"
		"print action.run(41);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, StructMethodExecutes)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"struct Counter {"
		"  value: int;"
		"  fn increment() : int {"
		"    value = value + 1;"
		"    return value;"
		"  }"
		"}"
		"var counter = Counter(41);"
		"print counter.increment();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, StructBackedInterfaceExecutes)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"interface Counter { fn increment() : int; }"
		"struct Box implements Counter {"
		"  value: int;"
		"  fn increment() : int {"
		"    value = value + 1;"
		"    return value;"
		"  }"
		"}"
		"var counter: Counter = Box(41);"
		"print counter.increment();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	std::filesystem::remove_all(root);
}
