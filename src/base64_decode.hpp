#pragma once

#include <string_view>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5030)
#endif

namespace fastgltf::base64 {
    /**
     * Calculates the amount of base64 padding chars ('=') at the end of the encoded string.
     * @note There's at most 2 padding chars, and this function expects that the input string
     * points to the original string that has a size that is a multiple of 4 and is at least
     * 4 chars long.
     */
    [[gnu::always_inline]] constexpr size_t getPadding(std::string_view string) {
        assert(string.size() >= 4 && string.size() % 4 == 0);
        const auto size = string.size();
        for (auto i = 1; i < 4; ++i)
            if (string[size - i] != '=')
                return i - 1;
        return 0;
    }

    /**
     * Calculates the size of the decoded string based on the size of the base64 encoded string and
     * the amount of padding the encoded data contains.
     */
    [[gnu::always_inline]] constexpr size_t getOutputSize(size_t encodedSize, size_t padding) noexcept {
        assert(encodedSize % 4 == 0);
        return (encodedSize / 4) * 3 - padding;
    }

#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_IX86)
    void sse4_decode(std::string_view encoded, uint8_t* output, size_t padding);
    void avx2_decode(std::string_view encoded, uint8_t* output, size_t padding);

    [[nodiscard]] std::vector<uint8_t> sse4_decode(std::string_view encoded);
    [[nodiscard]] std::vector<uint8_t> avx2_decode(std::string_view encoded);
#endif
#if defined(_M_ARM64) || defined(__ARM_NEON) || defined(__aarch64__)
    void neon_decode(std::string_view encoded, uint8_t* output, size_t padding);
    [[nodiscard]] std::vector<uint8_t> neon_decode(std::string_view encoded);
#endif
    void fallback_decode(std::string_view encoded, uint8_t* output, size_t padding);
    void decode(std::string_view encoded, uint8_t* output, size_t padding);

    [[nodiscard]] std::vector<uint8_t> fallback_decode(std::string_view encoded);
    [[nodiscard]] std::vector<uint8_t> decode(std::string_view encoded);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
