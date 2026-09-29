#include <fastgltf/crc32.hpp>

#include <simdjson.h>

#if defined(FASTGLTF_IS_X86)
#include <nmmintrin.h> // SSE4.2 for the CRC-32C instructions
#elif defined(FASTGLTF_ENABLE_ARMV8_CRC)
// MSVC does not provide the arm crc32 intrinsics.
#include <arm_acle.h>
#endif

namespace fastgltf {
#if defined(FASTGLTF_IS_X86)
	[[gnu::hot, gnu::pure, gnu::target("sse4.2")]] std::uint32_t sse_crc32c(const std::uint8_t* d, std::size_t len) noexcept {
		std::uint32_t crc = ~0;

		// Decode as much as possible using 4 byte steps.
		// We specifically don't use the 8 byte instruction here because it uses a 64-bit output integer.
		auto length = static_cast<std::int64_t>(len);
		while ((length -= sizeof(std::uint32_t)) >= 0) [[likely]] {
			std::uint32_t v;
			std::memcpy(&v, d, sizeof v);
			crc = _mm_crc32_u32(crc, v);
			d += sizeof v;
		}

		if (length & sizeof(std::uint16_t)) {
			std::uint16_t v;
			std::memcpy(&v, d, sizeof v);
			crc = _mm_crc32_u16(crc, v);
			d += sizeof v;
		}

		if (length & sizeof(std::uint8_t)) {
			crc = _mm_crc32_u8(crc, *d);
		}

		return crc ^ 0xffffffff;
	}
#elif defined(FASTGLTF_ENABLE_ARMV8_CRC)
	[[gnu::hot, gnu::pure, gnu::target("+crc")]] std::uint32_t armv8_crc32c(const std::uint8_t* d, std::size_t len) noexcept {
		std::uint32_t crc = ~0;

		// Decrementing the length variable and incrementing the pointer directly has better codegen with Clang
		// than using a std::size_t i = 0.
		auto length = static_cast<std::int64_t>(len);
		while ((length -= sizeof(std::uint64_t)) >= 0) [[likely]] {
			std::uint64_t value;
			std::memcpy(&value, d, sizeof value);
			crc = __crc32cd(crc, value);
			d += sizeof value;
		}

		if (length & sizeof(std::uint32_t)) {
			std::uint32_t value;
			std::memcpy(&value, d, sizeof value);
			crc = __crc32cw(crc, value);
			d += sizeof value;
		}

		if (length & sizeof(std::uint16_t)) {
			std::uint16_t value;
			std::memcpy(&value, d, sizeof value);
			crc = __crc32ch(crc, value);
			d += sizeof value;
		}

		if (length & sizeof(std::uint8_t)) {
			crc = __crc32cb(crc, *d);
		}

		return crc ^ 0xffffffff;
	}
#endif
}
