#include <array>
#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include "base64_decode.hpp"
#include "fastgltf_types.hpp"
#include "fastgltf_parser.hpp"

TEST_CASE("Check base64 decoding") {
    // This is "Hello World".
    auto bytes = fastgltf::base64::decode("SGVsbG8gV29ybGQuIEhlbGxvIFdvcmxkLg==");
    std::string strings(bytes.begin(), bytes.end());
    REQUIRE(strings == "Hello World. Hello World.");
}

TEST_CASE("Check both base64 decoders") {
    // Checks that both base64 decoders return the same.
    constexpr std::string_view test = "SGVsbG8gV29ybGQuIEhlbGxvIFdvcmxkLg==";
    REQUIRE(fastgltf::base64::fallback_decode(test) == fastgltf::base64::avx2_decode(test));
}

TEST_CASE("Benchmark AVX vs fallback base64 decoding") {
    auto path = std::filesystem::path { __FILE__ }.parent_path() / "gltf";

    constexpr auto sampleCount = 10000U;
    fastgltf::Parser parser;
    fastgltf::Image texture;
    std::string imageData;

    SECTION("Load image data from glTF") {
        parser.loadGlTF(path / "sample-models" / "2.0" / "BoxTextured" / "glTF-Embedded" / "BoxTextured.gltf");
        REQUIRE(parser.getError() == fastgltf::Error::None);

        auto asset = parser.getParsedAsset();
        REQUIRE(asset->images.size() == 1);

        // Load the image from the parsed glTF file.
        auto& image = asset->images.front();
        REQUIRE(image.location == fastgltf::DataLocation::VectorWithMime);
        REQUIRE(image.data.mimeType == "image/png");
        imageData = std::string { image.data.bytes.begin(), image.data.bytes.end() };
        REQUIRE(!imageData.empty());

        std::array<std::chrono::duration<float, std::milli>, sampleCount> durationsFallback = {};
        for (auto i = 0U; i < sampleCount; ++i) {
            auto start = std::chrono::high_resolution_clock::now();

            fastgltf::base64::fallback_decode(imageData);

            auto end = std::chrono::high_resolution_clock::now();
            durationsFallback[i] = end - start;
        }

        std::array<std::chrono::duration<float, std::milli>, sampleCount> durationsAvx = {};
        for (auto i = 0U; i < sampleCount; ++i) {
            auto start = std::chrono::high_resolution_clock::now();

            fastgltf::base64::avx2_decode(imageData);

            auto end = std::chrono::high_resolution_clock::now();
            durationsAvx[i] = end - start;
        }

        float fallbackAverage = durationsFallback[0].count();
        for (auto i = 1U; i < sampleCount; ++i)
            fallbackAverage += durationsFallback[i].count();

        float avxAverage = durationsAvx[0].count();
        for (auto i = 1U; i < sampleCount; ++i)
            avxAverage += durationsAvx[i].count();

        std::cout << "fallback base64 avg decoding: " << fallbackAverage / sampleCount << "ms." << std::endl;
        std::cout << "avx      base64 avg decoding: " << avxAverage / sampleCount << "ms." << std::endl;
    }
}
