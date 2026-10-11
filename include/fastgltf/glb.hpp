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

#ifndef FASTGLTF_GLB_HPP
#define FASTGLTF_GLB_HPP

#if !defined(FASTGLTF_MODULE)
#include <cstdint>
#include <cstddef>
#include <type_traits>
#endif

namespace fastgltf::internal {
	constexpr std::uint32_t binaryGltfHeaderMagic = 0x46546C67; // ASCII for "glTF".
	constexpr std::uint32_t binaryGltfJsonChunkMagic = 0x4E4F534A;
	constexpr std::uint32_t binaryGltfDataChunkMagic = 0x004E4942;

	struct BinaryGltfHeader {
		std::uint32_t magic;
		std::uint32_t version;
		std::uint32_t length;
	};
	static_assert(sizeof(BinaryGltfHeader) == 12, "Binary glTF header must be 12 bytes");
	static_assert(std::is_trivially_copyable_v<BinaryGltfHeader>);

	constexpr void readUint32LE(std::uint32_t& x, std::byte* bytes) noexcept {
		x = static_cast<std::uint32_t>(bytes[0])
				| (static_cast<std::uint32_t>(bytes[1]) << 8)
				| (static_cast<std::uint32_t>(bytes[2]) << 16)
				| (static_cast<std::uint32_t>(bytes[3]) << 24);
	}

	constexpr void writeUint32LE(std::uint32_t x, std::byte* buffer) noexcept {
		buffer[0] = static_cast<std::byte>(x);
		buffer[1] = static_cast<std::byte>(x >> 8);
		buffer[2] = static_cast<std::byte>(x >> 16);
		buffer[3] = static_cast<std::byte>(x >> 24);
	}

	struct BinaryGltfChunk {
		std::uint32_t chunkLength;
		std::uint32_t chunkType;
	};
	static_assert(sizeof(BinaryGltfChunk) == 8, "Binary glTF chunk must be 8 bytes");
	static_assert(std::is_trivially_copyable_v<BinaryGltfChunk>);
}
#endif
