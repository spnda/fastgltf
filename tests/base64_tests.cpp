#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "base64_decode.hpp"
#include "fastgltf_types.hpp"
#include "fastgltf_parser.hpp"

TEST_CASE("Check base64 decoding", "[base64]") {
    // This is "Hello World".
    auto bytes = fastgltf::base64::decode("SGVsbG8gV29ybGQuIEhlbGxvIFdvcmxkLg==");
    std::string strings(bytes.begin(), bytes.end());
    REQUIRE(strings == "Hello World. Hello World.");
};

TEST_CASE("Check both base64 decoders", "[base64]") {
    // Checks that both base64 decoders return the same.
    constexpr std::string_view test = "SGVsbG8gV29ybGQuIEhlbGxvIFdvcmxkLg==";
    REQUIRE(fastgltf::base64::fallback_decode(test) == fastgltf::base64::avx2_decode(test));
};

TEST_CASE("Benchmark AVX vs fallback base64 decoding", "[base64]") {
    auto path = std::filesystem::path { __FILE__ }.parent_path() / "gltf";

    fastgltf::Parser parser;
    fastgltf::Image texture;
    std::string bufferData;

    auto cylinderEngine = path / "sample-models" / "2.0" / "2CylinderEngine" / "glTF-Embedded" / "2CylinderEngine.gltf";

    SECTION("Validate large buffer load from glTF") {
        parser.loadGLTF(cylinderEngine);
        REQUIRE(parser.getError() == fastgltf::Error::None);

        auto asset = parser.getParsedAsset();
        REQUIRE(asset->buffers.size() == 1);

        // Load the buffer from the parsed glTF file.
        auto& buffer = asset->buffers.front();
        REQUIRE(buffer.byteLength == 1794612);
        REQUIRE(buffer.location == fastgltf::DataLocation::VectorWithMime);
        REQUIRE(buffer.data.mimeType == fastgltf::MimeType::OctetStream);
        REQUIRE(!buffer.data.bytes.empty());
    }
};
