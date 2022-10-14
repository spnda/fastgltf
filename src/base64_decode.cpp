#include <array>
#include <cmath>

#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_IX86)
#include <immintrin.h>
#if defined(__clang__) || defined(__GNUC__)
#include <avxintrin.h>
#include <avx2intrin.h>
#endif
#elif defined(__aarch64__)
#include <arm_neon.h>
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5030)
#endif

#include "simdjson.h"

#include "base64_decode.hpp"

namespace fg = fastgltf;

namespace fastgltf::base64 {
    [[gnu::always_inline]] inline size_t getPadding(std::string_view string) {
        size_t padding = 0;
        auto size = string.size();
        for (auto i = size - 1; i >= (size - 3); --i) {
            if (string[i] != '=')
                break;
            ++padding;
        }
        return padding;
    }

    [[gnu::always_inline]] inline size_t getOutputSize(size_t encodedSize, size_t padding) {
        return static_cast<size_t>(static_cast<float>(encodedSize - padding) * 0.75f);
    }
}

#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_IX86)
// The AVX and SSE decoding functions are based on http://0x80.pl/notesen/2016-01-17-sse-base64-decoding.html.
// It covers various methods of en-/decoding base64 using SSE and AVX and also shows their
// performance metrics.
// TODO: Mark these functions with msvc::forceinline which is available from C++20
[[gnu::target("avx2"), gnu::always_inline]] inline auto lookup_pshufb_bitmask(const __m256i input) {
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

[[gnu::target("avx2"), gnu::always_inline]] inline auto pack_ints(__m256i input) {
    const auto merge = _mm256_maddubs_epi16(input, _mm256_set1_epi32(0x01400140));
    return _mm256_madd_epi16(merge, _mm256_set1_epi32(0x00011000));
}

[[gnu::target("avx2")]] std::vector<uint8_t> fg::base64::avx2_decode(std::string_view encoded) {
    constexpr auto dataSetSize = 32;
    constexpr auto dataOutputSize = 24;

    // We align the size to the highest size divisible by 32. By doing this, we don't need to
    // allocate any new memory to hold the encoded data and let the fallback decoder decode the
    // remaining data.
    const auto encodedSize = encoded.size();
    const auto alignedSize = encodedSize - (encodedSize % dataSetSize);
    const auto padding = getPadding(encoded);

    // We have to allocate more than we actually use because _mm_storeu_si128 will write 16 bytes,
    // regardless if we only use 12 of those.
    const auto outputSize = (alignedSize / dataSetSize) * dataOutputSize + (dataSetSize - dataOutputSize);
    std::vector<uint8_t> ret(outputSize);
    auto* out = ret.data();

    for (size_t pos = 0; pos < alignedSize; pos += dataSetSize) {
        auto in = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&encoded[pos]));
        auto values = lookup_pshufb_bitmask(in);
        const auto merged = pack_ints(values);

        const auto shuffle = _mm256_setr_epi8(
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

        const auto shuffled = _mm256_shuffle_epi8(merged, shuffle);

        _mm_storeu_si128(reinterpret_cast<__m128i*>(out), _mm256_extracti128_si256(shuffled, 0));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 12), _mm256_extracti128_si256(shuffled, 1));

        out += dataOutputSize;
    }

    // Decode the last chunk traditionally
    if (alignedSize < encodedSize) {
        // Decode the last chunk traditionally
        auto remainder = outputSize - (outputSize % dataOutputSize);
        fallback_decode(encoded.substr(alignedSize, encodedSize), &ret[remainder], padding);
    }

    // Remove the zeroes at the end of the vector.
    ret.resize(getOutputSize(encodedSize, padding));

    return ret;
}

[[gnu::target("sse4.1"), gnu::always_inline]] inline auto sse4_lookup_pshufb_bitmask(const __m128i input) {
    const auto higher_nibble = _mm_and_si128(_mm_srli_epi32(input, 4), _mm_set1_epi8(0x0f));

    const auto shiftLUT = _mm_setr_epi8(
        0,   0,  19,   4, -65, -65, -71, -71,
        0,   0,   0,   0,   0,   0,   0,   0);

    const auto sh     = _mm_shuffle_epi8(shiftLUT,  higher_nibble);
    const auto eq_2f  = _mm_cmpeq_epi8(input, _mm_set1_epi8(0x2f));
    const auto shift  = _mm_blendv_epi8(sh, _mm_set1_epi8(16), eq_2f);

    return _mm_add_epi8(input, shift);
}

[[gnu::target("sse4.1"), gnu::always_inline]] inline auto sse4_pack_ints(__m128i input) {
    const auto merge = _mm_maddubs_epi16(input, _mm_set1_epi32(0x01400140));
    return _mm_madd_epi16(merge, _mm_set1_epi32(0x00011000));
}

[[gnu::target("sse4.1")]] std::vector<uint8_t> fg::base64::sse4_decode(std::string_view encoded) {
    constexpr auto dataSetSize = 16;
    constexpr auto dataOutputSize = 12;

    // We align the size to the highest size divisible by 16. By doing this, we don't need to
    // allocate any new memory to hold the encoded data and let the fallback decoder decode the
    // remaining data.
    const auto encodedSize = encoded.size();
    const auto alignedSize = encodedSize - (encodedSize % dataSetSize);
    const auto padding = getPadding(encoded);

    // We have to allocate more than we actually use because _mm_storeu_si128 will write 16 bytes,
    // regardless if we only use 12 of those.
    const auto outputSize = (alignedSize / dataSetSize) * dataOutputSize + (dataSetSize - dataOutputSize);
    std::vector<uint8_t> ret(outputSize);
    auto* out = ret.data();

    for (size_t pos = 0; pos < alignedSize; pos += dataSetSize) {
        auto in = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&encoded[pos]));
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

        out += dataOutputSize;
    }

    // Decode the last chunk traditionally
    if (alignedSize < encodedSize) {
        // Decode the last chunk traditionally
        const auto remainder = outputSize - (outputSize % dataOutputSize);
        fallback_decode(encoded.substr(alignedSize, encodedSize), &ret[remainder], padding);
    }

    // Remove the zeroes at the end of the vector.
    ret.resize(getOutputSize(encodedSize, padding));

    return ret;
}
#elif defined(__aarch64__)
[[gnu::always_inline]] int8x16_t neon_lookup_pshufb_bitmask(const uint8x16_t input) {
    // clang-format off
    constexpr std::array<int8_t, 16> shiftLUTdata = {
        0,   0,  19,   4, -65, -65, -71, -71,
        0,   0,   0,   0,   0,   0,   0,   0
    };
    // clang-fomat on

    const uint64x2_t higher_nibble = vandq_s32(vshlq_u32(vreinterpretq_u32_u8(input), vdupq_n_s32(-4)), vdupq_n_s8(0x0f));

    const int8x16_t shiftLUT = vld1q_s8(shiftLUTdata.data());

    const int8x16_t sh = vqtbl1q_s8(shiftLUT, vandq_u8(higher_nibble, vdupq_n_u8(0x8F)));
    const uint8x16_t eq_2f = vceqq_s8(input, vdupq_n_s8(0x2F));
    const uint8x16_t shift = vbslq_u8(vshrq_n_s8(eq_2f, 7), vdupq_n_s8(16), sh);

    return vaddq_s8(input, shift);
}

[[gnu::always_inline]] int16x8_t neon_pack_ints(const int8x16_t input) {
    const uint32x4_t mask = vdupq_n_u32(0x01400140);

    const int16x8_t tl = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(input))), vmovl_s8(vget_low_s8(mask)));
    const int16x8_t th = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(input))), vmovl_s8(vget_high_s8(mask)));
    const int16x8_t merge = vqaddq_s16(vuzp1q_s16(tl, th), vuzp2q_s16(tl, th));

    // Multiply the 8 signed 16-bit integers from a and b and add the n and n + 1 results together,
    // resulting in 4 32-bit integers.
    const uint32x4_t mergeMask = vdupq_n_u32(0x00011000);
    const int32x4_t pl = vmull_s16(vget_low_s16(merge), vget_low_s16(mergeMask));
    const int32x4_t ph = vmull_high_s16(merge, mergeMask);
    return vpaddq_s32(pl, ph);
}

std::vector<uint8_t> fg::base64::neon_decode(std::string_view encoded) {
    constexpr auto dataSetSize = 16;
    constexpr auto dataOutputSize = 12;

    // We align the size to the highest size divisible by 16. By doing this, we don't need to
    // allocate any new memory to hold the encoded data and let the fallback decoder decode the
    // remaining data.
    const auto encodedSize = encoded.size();
    const auto alignedSize = encodedSize - (encodedSize % dataSetSize);
    const auto padding = getPadding(encoded);

    // We have to allocate more than we actually use because vst1q_u8 will write 16 bytes,
    // regardless if we only use 12 of those.
    const auto outputSize = (alignedSize / dataSetSize) * dataOutputSize + (dataSetSize - dataOutputSize);
    std::vector<uint8_t> ret(outputSize);
    auto* out = ret.data();

    // clang-format off
    [[gnu::aligned(16)]] constexpr std::array<int8_t, 16> shuffleData = {
            2,  1,  0,
            6,  5,  4,
            10,  9,  8,
            14, 13, 12,
            char(0xff), char(0xff), char(0xff), char(0xff)
    };
    // clang-fomat on

    // Decode the first 16 long chunks with Neon intrinsics
    const auto shuffle = vreinterpretq_u8_s8(vld1q_s8(shuffleData.data()));
    for (size_t pos = 0; pos < alignedSize; pos += dataSetSize) {
        // Load 16 8-bit values into a 128-bit register.
        auto in = vld1q_u8(reinterpret_cast<const uint8_t*>(&encoded[pos]));
        auto values = neon_lookup_pshufb_bitmask(in);
        const auto merged = neon_pack_ints(values);

        const auto masked = vandq_u8(shuffle, vdupq_n_u8(0x8F));
        const auto shuffled = vqtbl1q_s8(merged, masked);

        // Store 16 8-bit values into output pointer
        vst1q_u8(out, shuffled);
        out += 12;
    }

    // Decode the last chunk traditionally
    if (alignedSize < encodedSize) {
        // Decode the last chunk traditionally
        auto remainder = outputSize - (outputSize % dataOutputSize);
        fallback_decode(encoded.substr(alignedSize, encodedSize), &ret[remainder], padding);
    }

    // Remove the zeroes at the end of the vector.
    ret.resize(getOutputSize(encodedSize, padding));

    return ret;
}
#endif

// clang-format off
// ASCII value -> base64 value LUT
constexpr std::array<uint8_t, 128> base64lut = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,62,0,0,0,63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
    0,0,0,0,0,0,0,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
    0,0,0,0,0,0,
    26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
    0,0,0,0,0,
};
// clang-format on

[[gnu::always_inline]] void fg::base64::fallback_decode(std::string_view encoded, uint8_t* output, size_t padding) {
    std::array<uint8_t, 4> sixBitChars = {};
    std::array<uint8_t, 3> eightBitChars = {};

    // We use i here to track how many we've parsed and to batch 4 chars together.
    const auto encodedSize = encoded.size();
    size_t i = 0U, cursor = 0U;
    for (auto pos = 0U; pos < encodedSize;) {
        sixBitChars[i++] = encoded[pos]; ++pos;
        if (i != 4)
            continue;

        for (i = 0; i < 4; i++) {
            assert(static_cast<size_t>(sixBitChars[i]) < base64lut.size());
            sixBitChars[i] = base64lut[sixBitChars[i]];
        }

        eightBitChars[0] = (sixBitChars[0] << 2) + ((sixBitChars[1] & 0x30) >> 4);
        eightBitChars[1] = ((sixBitChars[1] & 0xf) << 4) + ((sixBitChars[2] & 0x3c) >> 2);
        eightBitChars[2] = ((sixBitChars[2] & 0x3) << 6) + sixBitChars[3];

        // This adds 3 elements to the output vector. It also checks to not write zeroes that are
        // generate from the padding.
        for (size_t j = 0; j < 3 && ((pos - i + 1) + j) < (encodedSize - padding); ++j) {
            output[cursor++] = eightBitChars[j];
        }

        i = 0;
    }
}

std::vector<uint8_t> fg::base64::fallback_decode(std::string_view encoded) {
    auto encodedSize = encoded.size();
    auto padding = getPadding(encoded);

    std::vector<uint8_t> ret(getOutputSize(encodedSize, padding));

    fallback_decode(encoded, ret.data(), padding);

    return ret;
}

std::vector<uint8_t> fg::base64::decode(std::string_view encoded) {
    assert(encoded.size() % 4 == 0);

    // We use simdjson's helper functions to determine which SIMD intrinsics are available at runtime.
#if defined(__x86_64__) || defined(_M_AMD64)
    auto* avx2 = simdjson::get_available_implementations()["haswell"];
    auto* sse4 = simdjson::get_available_implementations()["westmere"];
    if (avx2 != nullptr && avx2->supported_by_runtime_system()) {
        return avx2_decode(encoded);
    } else if (sse4 != nullptr && sse4->supported_by_runtime_system()) {
        return sse4_decode(encoded);
    }
#endif

#if defined(__aarch64__)
    auto* neon = simdjson::get_available_implementations()["arm64"];
    if (neon != nullptr && neon->supported_by_runtime_system()) {
        return neon_decode(encoded);
    }
#endif

    return fallback_decode(encoded);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
