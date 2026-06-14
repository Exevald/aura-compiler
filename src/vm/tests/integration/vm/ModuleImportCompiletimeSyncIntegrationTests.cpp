#include "../../support/CompilerVmIntegrationSupport.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

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
		"print sync.lock(t2, m1);");

	EXPECT_THROW(CompileProgram(root / "samples" / "main.aura"), std::runtime_error);
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

	EXPECT_THROW(CompileProgram(root / "samples" / "main.aura"), std::runtime_error);
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
