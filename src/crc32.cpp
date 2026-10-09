/*
 * Copyright (C) 2022 - 2026 Sean Apeler
 * This file is part of fastgltf <https://github.com/spnda/fastgltf>.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

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
#ifdef __x86_64__
		std::uint64_t crc64 = ~0;

		while (len >= sizeof(std::uint64_t)) [[likely]] {
			std::uint64_t value;
			std::memcpy(&value, d, sizeof value);
			crc64 = _mm_crc32_u64(crc64, value);
			len -= sizeof value;
			d += sizeof value;
		}

		auto crc = static_cast<std::uint32_t>(crc64);

		if (len & sizeof(std::uint32_t)) {
			std::uint32_t value;
			std::memcpy(&value, d, sizeof value);
			crc = _mm_crc32_u32(crc, value);
			d += sizeof value;
		}
#else
		std::uint32_t crc = ~0;

		while (len >= sizeof(std::uint32_t)) [[likely]] {
			std::uint32_t value;
			std::memcpy(&value, d, sizeof value);
			crc = _mm_crc32_u32(crc, value);
			len -= sizeof value;
			d += sizeof value;
		}
#endif

		if (len & sizeof(std::uint16_t)) {
			std::uint16_t value;
			std::memcpy(&value, d, sizeof value);
			crc = _mm_crc32_u16(crc, value);
			d += sizeof value;
		}

		if (len & sizeof(std::uint8_t)) {
			crc = _mm_crc32_u8(crc, *d);
		}

		return crc ^ 0xffffffff;
	}
#elif defined(FASTGLTF_ENABLE_ARMV8_CRC)
	[[gnu::hot, gnu::pure, gnu::target("+crc")]] std::uint32_t armv8_crc32c(const std::uint8_t* d, const std::size_t len) noexcept {
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
