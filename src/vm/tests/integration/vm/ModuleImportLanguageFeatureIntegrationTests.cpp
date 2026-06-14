#include "../../support/CompilerVmIntegrationSupport.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

TEST(ModuleImportIntegrationTest, TransactionCoversStdArrayMutationsAndIteratorReads)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.array as arr;"
		"shared var gate: int = 0;"
		"fn run() : void {"
		"  var values = [3, 1];"
		"  transaction(shared gate) {"
		"    arr.push(values, 2);"
		"    var sorted = arr.sort(values);"
		"    print arr.len(sorted);"
		"    print sorted[0];"
		"    print arr.pop(sorted);"
		"  }"
		"  print arr.len(values);"
		"  print values[0];"
		"  print values[1];"
		"}"
		"run();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("3"));
	EXPECT_THAT(output, ::testing::HasSubstr("1"));
	EXPECT_THAT(output, ::testing::HasSubstr("3"));
	EXPECT_THAT(output, ::testing::HasSubstr("2"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, BlockScopedVariableDoesNotLeakOutsideBlock)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.io as io;"
		"{"
		"  var blockMessage: string = \"hidden\";"
		"  io.println(blockMessage);"
		"}"
		"io.println(blockMessage);");

	EXPECT_THROW(CompileProgram(root / "samples" / "main.aura"), std::runtime_error);
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, MultidimensionalArraysExecuteThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import std.io as io;"
		"var grid: [[int]] = [[1, 2], [3, 4]];"
		"io.println(grid);"
		"io.println(grid[0]);"
		"io.println(grid[1][0]);");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("[[1, 2], [3, 4]]"));
	EXPECT_THAT(output, ::testing::HasSubstr("[1, 2]"));
	EXPECT_THAT(output, ::testing::HasSubstr("3"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, GoAwaitExecutesThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"fn add(a: int, b: int) : int {"
		"  return a + b;"
		"}"
		"fn run() : void {"
		"  var task = go add(19, 23);"
		"  print await task;"
		"}"
		"run();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, MultiRegionTransactionExecutesAcrossSharedRegions)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"shared var left: int = 1;"
		"shared var right: int = 2;"
		"fn run() : void {"
		"  transaction(shared left | shared right) {"
		"    left = left + 9;"
		"    right = right + 18;"
		"  }"
		"  print left;"
		"  print right;"
		"}"
		"run();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("10"));
	EXPECT_THAT(output, ::testing::HasSubstr("20"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, SampleUnsafeCapabilitiesExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "unsafe_capabilities.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("unsafe value: 42"));
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

TEST(ModuleImportIntegrationTest, ImportedGenericStructNestedFieldExecutesAcrossFiles)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "models.aura",
		"module samples.models;"
		"struct Point { x: int; }"
		"struct Box<T> { value: T; }");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import samples.models as models;"
		"var box: models.Box<models.Point> = models.Box(models.Point(7));"
		"print box.value.x;");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("7"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, ImportedGenericInterfaceCallExecutesAcrossFiles)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "api.aura",
		"module samples.api;"
		"interface Reader<T> { fn read() : T; }");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import samples.api as api;"
		"struct Box implements api.Reader<int> { value: int; fn read() : int { return self.value; } }"
		"var reader: api.Reader<int> = Box(5);"
		"print reader.read();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("5"));
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

TEST(ModuleImportIntegrationTest, SourceLevelIterExecutesAdapterChain)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"var values = [1, 2, 3, 4, 5];"
		"iter (value of values with [drop(1), take(3), reverse]) {"
		"  print value;"
		"}");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("4"));
	EXPECT_THAT(output, ::testing::HasSubstr("3"));
	EXPECT_THAT(output, ::testing::HasSubstr("2"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, RefParameterMutatesCallerStorage)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"struct Box { value: int; }"
		"fn inc(target: ref<int>) : void {"
		"  *target = *target + 1;"
		"}"
		"var value: int = 1;"
		"var values = [10, 20];"
		"var box = Box(30);"
		"inc(value);"
		"inc(values[0]);"
		"inc(box.value);"
		"print value;"
		"print values[0];"
		"print box.value;");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("2"));
	EXPECT_THAT(output, ::testing::HasSubstr("11"));
	EXPECT_THAT(output, ::testing::HasSubstr("31"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, ResumableEffectHandlerExecutesThroughCompilerAndVM)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"effect IO { fn read() : int; }"
		"fn run() raises { IO } {"
		"  print read() + 35;"
		"}"
		"handle run() with {"
		"  effect read() -> { resume(7); }"
		"}");

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
		"export fn setVersion(value: int) { version = value; };"
		"fn hidden() : int { return 34; }"
		"export fn answer() : int { return hidden() + version; };");
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"import samples.api as api;"
		"print api.version;"
		"api.setVersion(8);"
		"print api.answer();");

	const auto output = RunProgram(root / "samples" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("7"));
	EXPECT_THAT(output, ::testing::HasSubstr("42"));
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, ModuleBackedInterfaceRejected)
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

	EXPECT_THROW((void)RunProgram(root / "samples" / "main.aura"), std::runtime_error);
	std::filesystem::remove_all(root);
}

TEST(ModuleImportIntegrationTest, FunctionBackedSingleMethodInterfaceRejected)
{
	const auto root = MakeTempRoot();
	WriteFile(
		root / "samples" / "main.aura",
		"module samples.main;"
		"interface Action { fn run(v: int) : int; }"
		"var action: Action = fn(v: int) -> v + 1;"
		"print action.run(41);");

	EXPECT_THROW((void)RunProgram(root / "samples" / "main.aura"), std::runtime_error);
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
