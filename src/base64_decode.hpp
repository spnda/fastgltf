#pragma once

#include <string_view>
#include <vector>

namespace fastgltf::base64 {
    [[nodiscard]] std::vector<uint8_t> sse4_decode(std::string_view encoded);
    [[nodiscard]] std::vector<uint8_t> avx2_decode(std::string_view encoded);
    [[nodiscard]] std::vector<uint8_t> fallback_decode(std::string_view encoded);
    [[nodiscard]] std::vector<uint8_t> decode(std::string_view encoded);
}
