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

#if !defined(__cplusplus) || (!defined(_MSVC_LANG) && __cplusplus < 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG < 201703L)
#error "fastgltf requires C++17"
#endif

#include <array>
#include <cmath>
#include <functional>

#include <simdutf.h>

#include <fastgltf/base64.hpp>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5030) // attribute 'x' is not recognized
#pragma warning(disable : 4710) // function not inlined
#endif

namespace fg = fastgltf;

void fg::base64::decode_inplace(const std::string_view encoded, std::uint8_t* output, [[maybe_unused]] std::size_t padding) {
	assert(encoded.size() % 4 == 0);
	(void)simdutf::base64_to_binary(encoded.data(), encoded.size(), reinterpret_cast<char*>(output));
}

fg::static_vector<std::uint8_t> fg::base64::decode(const std::string_view encoded) {
	const auto padding = getPadding(encoded);
	static_vector<std::uint8_t> ret(for_overwrite, getDecodedSize(encoded.size(), padding));
	decode_inplace(encoded, ret.data(), padding);
	return ret;
}

void fg::base64::encode_into(const std::uint8_t* data, const std::size_t size, char* output) {
	(void)simdutf::binary_to_base64(reinterpret_cast<const char*>(data), size, output);
}

std::string fg::base64::encode(const std::uint8_t* data, const std::size_t size) {
	std::string out(getEncodedSize(size), '\0');
	encode_into(data, size, out.data());
	return out;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
