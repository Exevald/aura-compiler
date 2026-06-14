#include "CryptoModule.h"

#include "../../NativeModuleSupport.h"
#include "../../SharedRuntime.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace VM::Runtime
{

using Core::Value;
using Execution::ExecutionContext;

namespace
{

struct Sha256State
{
	std::array<uint32_t, 8> hash = {
		0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
		0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
	};
	std::array<uint8_t, 64> buffer{};
	uint64_t bitLength = 0;
	size_t bufferSize = 0;
};

constexpr std::array Sha256Constants = {
	0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
	0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
	0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
	0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
	0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
	0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
	0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
	0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

uint32_t RotateRight(const uint32_t value, const uint32_t bits)
{
	return (value >> bits) | (value << (32 - bits));
}

void TransformSha256(Sha256State& state, const uint8_t* chunk)
{
	std::array<uint32_t, 64> schedule{};
	for (size_t i = 0; i < 16; ++i)
	{
		schedule[i] = (static_cast<uint32_t>(chunk[i * 4]) << 24)
			| (static_cast<uint32_t>(chunk[i * 4 + 1]) << 16)
			| (static_cast<uint32_t>(chunk[i * 4 + 2]) << 8)
			| static_cast<uint32_t>(chunk[i * 4 + 3]);
	}
	for (size_t i = 16; i < schedule.size(); ++i)
	{
		const uint32_t s0 = RotateRight(schedule[i - 15], 7) ^ RotateRight(schedule[i - 15], 18) ^ (schedule[i - 15] >> 3);
		const uint32_t s1 = RotateRight(schedule[i - 2], 17) ^ RotateRight(schedule[i - 2], 19) ^ (schedule[i - 2] >> 10);
		schedule[i] = schedule[i - 16] + s0 + schedule[i - 7] + s1;
	}

	uint32_t a = state.hash[0];
	uint32_t b = state.hash[1];
	uint32_t c = state.hash[2];
	uint32_t d = state.hash[3];
	uint32_t e = state.hash[4];
	uint32_t f = state.hash[5];
	uint32_t g = state.hash[6];
	uint32_t h = state.hash[7];

	for (size_t i = 0; i < schedule.size(); ++i)
	{
		const uint32_t s1 = RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
		const uint32_t choice = (e & f) ^ ((~e) & g);
		const uint32_t temp1 = h + s1 + choice + Sha256Constants[i] + schedule[i];
		const uint32_t s0 = RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
		const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
		const uint32_t temp2 = s0 + majority;

		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;
	}

	state.hash[0] += a;
	state.hash[1] += b;
	state.hash[2] += c;
	state.hash[3] += d;
	state.hash[4] += e;
	state.hash[5] += f;
	state.hash[6] += g;
	state.hash[7] += h;
}

void UpdateSha256(Sha256State& state, const std::string_view data)
{
	for (const unsigned char byte : data)
	{
		state.buffer[state.bufferSize++] = byte;
		if (state.bufferSize == state.buffer.size())
		{
			TransformSha256(state, state.buffer.data());
			state.bitLength += 512;
			state.bufferSize = 0;
		}
	}
}

std::array<uint8_t, 32> FinalizeSha256(Sha256State state)
{
	state.bitLength += static_cast<uint64_t>(state.bufferSize) * 8;
	state.buffer[state.bufferSize++] = 0x80;

	if (state.bufferSize > 56)
	{
		while (state.bufferSize < 64)
		{
			state.buffer[state.bufferSize++] = 0;
		}
		TransformSha256(state, state.buffer.data());
		state.bufferSize = 0;
	}

	while (state.bufferSize < 56)
	{
		state.buffer[state.bufferSize++] = 0;
	}

	for (int i = 7; i >= 0; --i)
	{
		state.buffer[state.bufferSize++] = static_cast<uint8_t>((state.bitLength >> (i * 8)) & 0xFF);
	}
	TransformSha256(state, state.buffer.data());

	std::array<uint8_t, 32> digest{};
	for (size_t i = 0; i < state.hash.size(); ++i)
	{
		digest[i * 4] = static_cast<uint8_t>((state.hash[i] >> 24) & 0xFF);
		digest[i * 4 + 1] = static_cast<uint8_t>((state.hash[i] >> 16) & 0xFF);
		digest[i * 4 + 2] = static_cast<uint8_t>((state.hash[i] >> 8) & 0xFF);
		digest[i * 4 + 3] = static_cast<uint8_t>(state.hash[i] & 0xFF);
	}
	return digest;
}

std::array<uint8_t, 32> Sha256(const std::string_view data)
{
	Sha256State state;
	UpdateSha256(state, data);
	return FinalizeSha256(state);
}

std::string Base64UrlEncode(const uint8_t* data, const size_t size)
{
	static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

	std::string output;
	output.reserve(((size + 2) / 3) * 4);

	for (size_t i = 0; i < size; i += 3)
	{
		const uint32_t block = (static_cast<uint32_t>(data[i]) << 16)
			| (static_cast<uint32_t>(i + 1 < size ? data[i + 1] : 0) << 8)
			| static_cast<uint32_t>(i + 2 < size ? data[i + 2] : 0);

		output.push_back(alphabet[(block >> 18) & 0x3F]);
		output.push_back(alphabet[(block >> 12) & 0x3F]);
		if (i + 1 < size)
		{
			output.push_back(alphabet[(block >> 6) & 0x3F]);
		}
		if (i + 2 < size)
		{
			output.push_back(alphabet[block & 0x3F]);
		}
	}

	return output;
}

std::array<uint8_t, 32> HmacSha256(const std::string_view key, const std::string_view data)
{
	std::array<uint8_t, 64> blockKey{};
	if (key.size() > blockKey.size())
	{
		const auto hashed = Sha256(key);
		std::memcpy(blockKey.data(), hashed.data(), hashed.size());
	}
	else
	{
		std::memcpy(blockKey.data(), key.data(), key.size());
	}

	std::array<uint8_t, 64> innerPad{};
	std::array<uint8_t, 64> outerPad{};
	for (size_t i = 0; i < blockKey.size(); ++i)
	{
		innerPad[i] = static_cast<uint8_t>(blockKey[i] ^ 0x36u);
		outerPad[i] = static_cast<uint8_t>(blockKey[i] ^ 0x5cu);
	}

	Sha256State innerState;
	UpdateSha256(innerState, std::string_view(reinterpret_cast<const char*>(innerPad.data()), innerPad.size()));
	UpdateSha256(innerState, data);
	const auto innerDigest = FinalizeSha256(innerState);

	Sha256State outerState;
	UpdateSha256(outerState, std::string_view(reinterpret_cast<const char*>(outerPad.data()), outerPad.size()));
	UpdateSha256(outerState, std::string_view(reinterpret_cast<const char*>(innerDigest.data()), innerDigest.size()));
	return FinalizeSha256(outerState);
}

bool ConstantTimeEqual(const std::string_view left, const std::string_view right)
{
	const size_t maxSize = std::max(left.size(), right.size());
	unsigned char diff = static_cast<unsigned char>(left.size() ^ right.size());
	for (size_t i = 0; i < maxSize; ++i)
	{
		const unsigned char l = i < left.size() ? static_cast<unsigned char>(left[i]) : 0;
		const unsigned char r = i < right.size() ? static_cast<unsigned char>(right[i]) : 0;
		diff |= static_cast<unsigned char>(l ^ r);
	}
	return diff == 0;
}

} // namespace

void CryptoModule::Install(SharedRuntime& runtime)
{
	runtime.DefineGlobal(std::string(ModuleName()), MakeModule(std::string(ModuleName())));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".hmac_sha256_base64url",
		MakeNative(
			"hmac_sha256_base64url",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto key = RequireString(ctx, args[0], "std.crypto.hmac_sha256_base64url expects a string key");
				const auto data = RequireString(ctx, args[1], "std.crypto.hmac_sha256_base64url expects a string payload");
				if (!key || !data)
				{
					return std::monostate{};
				}

				const auto digest = HmacSha256(*key, *data);
				return std::make_shared<const std::string>(Base64UrlEncode(digest.data(), digest.size()));
			}));

	runtime.DefineGlobal(
		std::string(ModuleName()) + ".constant_time_equal",
		MakeNative(
			"constant_time_equal",
			2,
			[](ExecutionContext& ctx, const std::vector<Value>& args) -> Value {
				const auto left = RequireString(ctx, args[0], "std.crypto.constant_time_equal expects a string");
				const auto right = RequireString(ctx, args[1], "std.crypto.constant_time_equal expects a string");
				if (!left || !right)
				{
					return std::monostate{};
				}
				return ConstantTimeEqual(*left, *right);
			}));
}

} // namespace VM::Runtime
