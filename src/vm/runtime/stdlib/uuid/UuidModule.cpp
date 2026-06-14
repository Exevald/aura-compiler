#include "UuidModule.h"

#include "../../NativeModuleSupport.h"

#include <array>
#include <iomanip>
#include <random>
#include <sstream>

namespace VM::Runtime
{

namespace
{

std::string FormatUuidV4()
{
	std::array<unsigned char, 16> bytes{};
	std::random_device random;
	for (auto& byte : bytes)
	{
		byte = static_cast<unsigned char>(random());
	}

	bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
	bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);

	std::ostringstream stream;
	stream << std::hex << std::nouppercase << std::setfill('0');
	for (size_t index = 0; index < bytes.size(); ++index)
	{
		stream << std::setw(2) << static_cast<int>(bytes[index]);
		if (index == 3 || index == 5 || index == 7 || index == 9)
		{
			stream << "-";
		}
	}
	return stream.str();
}

} // namespace

void UuidModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(
		std::string(ModuleName()),
		MakeModule(std::string(ModuleName())));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".new_v4",
		MakeNative(
			"new_v4",
			0,
			[](Execution::ExecutionContext&, const std::vector<Core::Value>&) -> Core::Value {
				return std::make_shared<const std::string>(FormatUuidV4());
			}));
}

} // namespace VM::Runtime
