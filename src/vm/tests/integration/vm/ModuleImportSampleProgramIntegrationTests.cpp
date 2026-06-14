#include "../../support/CompilerVmIntegrationSupport.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

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

TEST(ModuleImportIntegrationTest, SampleBubbleSortExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "bubblesort.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("Original list:"));
	EXPECT_THAT(output, ::testing::HasSubstr("[1, 2, 5, 5, 6, 9]"));
}

TEST(ModuleImportIntegrationTest, SampleClosuresExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "closures.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("Closure add10(7):"));
	EXPECT_THAT(output, ::testing::HasSubstr("17"));
	EXPECT_THAT(output, ::testing::HasSubstr("[11, 12, 9, 12]"));
}

TEST(ModuleImportIntegrationTest, SampleFactorialExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "factorial.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("factorial(5) ="));
	EXPECT_THAT(output, ::testing::HasSubstr("120"));
}

TEST(ModuleImportIntegrationTest, SampleModuleNamespaceExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "module_namespace.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("Imported module samples.math_utils"));
	EXPECT_THAT(output, ::testing::HasSubstr("27"));
	EXPECT_THAT(output, ::testing::HasSubstr("13"));
}

TEST(ModuleImportIntegrationTest, SampleResumableEffectsExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "effects_resumable.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("effect result: 42"));
}

TEST(ModuleImportIntegrationTest, SampleActorWalletExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "actor_wallet.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("wallet balance: 42"));
}

TEST(ModuleImportIntegrationTest, SampleTransactionSharedCounterExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "transaction_shared_counter.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("shared counter: 42"));
}

TEST(ModuleImportIntegrationTest, SampleContractsContextAndComptimeExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "contracts_context_and_comptime.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("folded = 7"));
	EXPECT_THAT(output, ::testing::HasSubstr("box = 7"));
	EXPECT_THAT(output, ::testing::HasSubstr("ctx dividing 7 by 1"));
	EXPECT_THAT(output, ::testing::HasSubstr("division = 7"));
}

TEST(ModuleImportIntegrationTest, SampleTypeAliasesAndGenericsExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "type_aliases_and_generics.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("picked = 22"));
	EXPECT_THAT(output, ::testing::HasSubstr("boxed = 22"));
	EXPECT_THAT(output, ::testing::HasSubstr("double = 42"));
}

TEST(ModuleImportIntegrationTest, SampleEnumsAndIteratorsExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "enums_and_iterators.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("tag = 1"));
	EXPECT_THAT(output, ::testing::HasSubstr("payload = 42"));
	EXPECT_THAT(output, ::testing::HasSubstr("iter = 14"));
	EXPECT_THAT(output, ::testing::HasSubstr("iter = 13"));
}

TEST(ModuleImportIntegrationTest, SampleThreadLocalStorageExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "thread_local_storage.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("cache = 42"));
}

TEST(ModuleImportIntegrationTest, SampleMatrixMultiplicationThreadsExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "matrix_multiplication_threads.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("left = [[1, 2, 3], [4, 5, 6]]"));
	EXPECT_THAT(output, ::testing::HasSubstr("right = [[7, 8], [9, 10], [11, 12]]"));
	EXPECT_THAT(output, ::testing::HasSubstr("result = [[58, 64], [139, 154]]"));
	EXPECT_THAT(output, ::testing::HasSubstr("rows = 2"));
}

TEST(ModuleImportIntegrationTest, SampleInterfacesExportsExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "interfaces_exports" / "main.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("reader = 41"));
	EXPECT_THAT(output, ::testing::HasSubstr("factory = 42"));
	EXPECT_THAT(output, ::testing::HasSubstr("ready = 42"));
}

TEST(ModuleImportIntegrationTest, SampleUuidBasicExecutes)
{
	const auto output = RunProgram(RepoRoot() / "samples" / "uuid_basic.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("request_id"));
	EXPECT_THAT(output, ::testing::HasSubstr("-"));
}

TEST(ModuleImportIntegrationTest, MicroserviceEntrypointsCompile)
{
	EXPECT_NO_THROW(CompileProgram(RepoRoot() / "microservicetemplate" / "cmd" / "main.aura"));
	EXPECT_NO_THROW(CompileProgram(RepoRoot() / "microservicetemplate" / "cmd" / "migrate.aura"));
}

TEST(ModuleImportIntegrationTest, MicroserviceLanguageShowcaseExecutes)
{
	const auto output = RunProgram(RepoRoot() / "microservicetemplate" / "cmd" / "showcase.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("AURA_SHOWCASE_OK 42"));
}

TEST(ModuleImportIntegrationTest, MicroserviceAuthSmokeExecutes)
{
	const auto output = RunProgram(RepoRoot() / "microservicetemplate" / "cmd" / "auth_smoke.aura");
	EXPECT_THAT(output, ::testing::HasSubstr("1"));
}
