#pragma once

namespace VM::Runtime
{
class SharedRuntime;

void InstallBuiltinStdlib(SharedRuntime& runtime);

} // namespace VM::Runtime
