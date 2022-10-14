#pragma once

#include <string_view>
#include <vector>

namespace fastgltf::base64 {
    constexpr size_t FALLBACK_PADDING = 4;

#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_IX86)
    [[nodiscard, gnu::target("sse4.1")]] std::vector<uint8_t> sse4_decode(std::string_view encoded);
    [[nodiscard, gnu::target("avx2")]] std::vector<uint8_t> avx2_decode(std::string_view encoded);
#endif
#if defined(__aarch64__)
    [[nodiscard]] std::vector<uint8_t> neon_decode(std::string_view encoded);
#endif
    // Used from the SSE4, AVX2, and Neon decoders. Should not be used otherwise.
    void fallback_decode(std::string_view encoded, uint8_t* output, size_t padding);
    [[nodiscard]] std::vector<uint8_t> fallback_decode(std::string_view encoded);
    [[nodiscard]] std::vector<uint8_t> decode(std::string_view encoded);
}
