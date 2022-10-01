#include <array>
#include <cmath>

#include <immintrin.h>
#if defined(__clang__) || defined(__GNUC__)
#include <avxintrin.h>
#include <avx2intrin.h>
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5030)
#endif

#include "simdjson.h"

#include "base64_decode.hpp"

namespace fg = fastgltf;

// The AVX and SSE decoding functions are based on http://0x80.pl/notesen/2016-01-17-sse-base64-decoding.html.
// It covers various methods of en-/decoding base64 using SSE and AVX and also shows their
// performance metrics.
// TODO: Mark these functions with msvc::forceinline which is available from C++20
[[gnu::target("avx2"), gnu::always_inline]] auto lookup_pshufb_bitmask(const __m256i input) {
    const auto higher_nibble = _mm256_and_si256(_mm256_srli_epi32(input, 4), _mm256_set1_epi8(0x0f));

    const auto shiftLUT = _mm256_setr_epi8(
        0,   0,  19,   4, -65, -65, -71, -71,
        0,   0,   0,   0,   0,   0,   0,   0,

        0,   0,  19,   4, -65, -65, -71, -71,
        0,   0,   0,   0,   0,   0,   0,   0);

    const auto sh     = _mm256_shuffle_epi8(shiftLUT,  higher_nibble);
    const auto eq_2f  = _mm256_cmpeq_epi8(input, _mm256_set1_epi8(0x2f));
    const auto shift  = _mm256_blendv_epi8(sh, _mm256_set1_epi8(16), eq_2f);

    return _mm256_add_epi8(input, shift);
}

[[gnu::target("avx2"), gnu::always_inline]] auto pack_ints(__m256i input) {
    const auto merge = _mm256_maddubs_epi16(input, _mm256_set1_epi32(0x01400140));
    return _mm256_madd_epi16(merge, _mm256_set1_epi32(0x00011000));
}

[[gnu::target("avx2")]] std::vector<uint8_t> fg::base64::avx2_decode(std::string_view encoded) {
    constexpr auto dataSetSize = 32;

    // We align the memory to be a multiple of 32, as we can only process 32 bytes at one time,
    // and nothing less.
    auto encodedSize = encoded.size();
    std::vector<uint8_t> input((encodedSize + dataSetSize - 1) & -dataSetSize);
    std::memcpy(input.data(), encoded.data(), encodedSize);

    // We search for the amount of padding the string has.
    size_t padding = 0;
    for (auto i = encodedSize - 1; i >= (encodedSize - 3); --i) {
        if (input[i] == '=')
            ++padding;
        else
            break;
    }

    auto length = input.size();
    std::vector<uint8_t> ret(length);
    auto* out = ret.data();

    for (size_t pos = 0; pos < length; pos += dataSetSize) {
        auto in = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&input[pos]));
        auto values = lookup_pshufb_bitmask(in);
        const auto merged = pack_ints(values);

        const auto shuf = _mm256_setr_epi8(
            2,  1,  0,
            6,  5,  4,
            10,  9,  8,
            14, 13, 12,
            char(0xff), char(0xff), char(0xff), char(0xff),
            2,  1,  0,
            6,  5,  4,
            10,  9,  8,
            14, 13, 12,
            char(0xff), char(0xff), char(0xff), char(0xff));

        const auto shuffled = _mm256_shuffle_epi8(merged, shuf);

        _mm_storeu_si128(reinterpret_cast<__m128i*>(out), _mm256_extracti128_si256(shuffled, 0));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 12), _mm256_extracti128_si256(shuffled, 1));

        out += 24;
    }

    // We now resize the vector to only show the actual values, not the 0's padded in to meet the
    // 32x requirement.
    auto diff = static_cast<float>(encodedSize - padding);
    ret.resize(static_cast<size_t>(std::floor(diff * 0.75f)));

    return ret;
}

[[gnu::target("sse4.1"), gnu::always_inline]] auto sse4_lookup_pshufb_bitmask(const __m128i input) {
    const auto higher_nibble = _mm_and_si128(_mm_srli_epi32(input, 4), _mm_set1_epi8(0x0f));

    const auto shiftLUT = _mm_setr_epi8(
        0,   0,  19,   4, -65, -65, -71, -71,
        0,   0,   0,   0,   0,   0,   0,   0);

    const auto sh     = _mm_shuffle_epi8(shiftLUT,  higher_nibble);
    const auto eq_2f  = _mm_cmpeq_epi8(input, _mm_set1_epi8(0x2f));
    const auto shift  = _mm_blendv_epi8(sh, _mm_set1_epi8(16), eq_2f);

    return _mm_add_epi8(input, shift);
}

[[gnu::target("sse4.1"), gnu::always_inline]] auto sse4_pack_ints(__m128i input) {
    const auto merge = _mm_maddubs_epi16(input, _mm_set1_epi32(0x01400140));
    return _mm_madd_epi16(merge, _mm_set1_epi32(0x00011000));
}

[[gnu::target("sse4.1")]] std::vector<uint8_t> fg::base64::sse4_decode(std::string_view encoded) {
    constexpr auto dataSetSize = 16;

    auto encodedSize = encoded.size();
    std::vector<uint8_t> input((encodedSize + dataSetSize - 1) & -dataSetSize);
    std::memcpy(input.data(), encoded.data(), encodedSize);

    // We search for the amount of padding the string has.
    size_t padding = 0;
    for (auto i = encodedSize - 1; i >= (encodedSize - 3); --i) {
        if (input[i] == '=')
            ++padding;
        else
            break;
    }

    auto length = input.size();
    std::vector<uint8_t> ret(length);
    auto* out = ret.data();

    for (size_t pos = 0; pos < length; pos += dataSetSize) {
        auto in = _mm_load_si128(reinterpret_cast<const __m128i*>(&input[pos]));
        auto values = sse4_lookup_pshufb_bitmask(in);
        const auto merged = sse4_pack_ints(values);

        const auto shuf = _mm_setr_epi8(
            2,  1,  0,
            6,  5,  4,
            10,  9,  8,
            14, 13, 12,
            char(0xff), char(0xff), char(0xff), char(0xff));

        const auto shuffled = _mm_shuffle_epi8(merged, shuf);

        _mm_storeu_si128(reinterpret_cast<__m128i*>(out), shuffled);

        out += 12;
    }

    // We now resize the vector to only show the actual values, not the 0's padded in to meet the
    // 16x requirement.
    auto diff = static_cast<float>(encodedSize - padding);
    ret.resize(static_cast<size_t>(std::floor(diff * 0.75f)));

    return ret;
}

// clang-format off
// ASCII value -> base64 value LUT
constexpr std::array<uint8_t, 128> base64lut = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
    0,0,0,0,0,0,0,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
    0,0,0,0,0,0,
    26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
    0,0,0,0,0,
};
// clang-format on

std::vector<uint8_t> fg::base64::fallback_decode(std::string_view encoded) {
    auto encodedSize = encoded.size();
    std::array<uint8_t, 4> sixBitChars = {};
    std::array<uint8_t, 3> eightBitChars = {};
    std::vector<uint8_t> ret;
    ret.reserve(static_cast<size_t>(static_cast<float>(encodedSize) * 0.75f));

    size_t padding = 0;
    for (auto i = encodedSize - 1; i >= (encodedSize - 3); --i) {
        if (encoded[i] == '=')
            ++padding;
        else
            break;
    }

    // We use i here to track how many we've parsed and to batch 4 chars together.
    size_t i = 0U;
    for (auto pos = 0U; pos < encodedSize;) {
        sixBitChars[i++] = encoded[pos]; ++pos;
        if (i != 4)
            continue;

        for (i = 0; i < 4; i++) {
            sixBitChars[i] = base64lut[sixBitChars[i]];
        }

        eightBitChars[0] = (sixBitChars[0] << 2) + ((sixBitChars[1] & 0x30) >> 4);
        eightBitChars[1] = ((sixBitChars[1] & 0xf) << 4) + ((sixBitChars[2] & 0x3c) >> 2);
        eightBitChars[2] = ((sixBitChars[2] & 0x3) << 6) + sixBitChars[3];

        // This adds 3 elements to the output vector. It also checks to not write zeroes that are
        // generate from the padding.
        for (size_t j = 0; j < 3 && ((pos - i + 1) + j) < (encodedSize - padding); ++j) {
            ret.emplace_back(eightBitChars[j]);
        }

        i = 0;
    }

    return ret;
}

std::vector<uint8_t> fg::base64::decode(std::string_view encoded) {
    // Use simdjson to determine if AVX2/SSE4.2 is supported by the current CPU.
    auto* avx2 = simdjson::get_available_implementations()["haswell"];
    auto* sse4 = simdjson::get_available_implementations()["westmere"];
    if (avx2 != nullptr && avx2->supported_by_runtime_system()) {
        return avx2_decode(encoded);
    } else if (sse4 != nullptr && sse4->supported_by_runtime_system()) {
        return sse4_decode(encoded);
    } else {
        return fallback_decode(encoded);
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
