#include <catch2/catch_test_macros.hpp>

#include "fastgltf_parser.hpp"
#include "fastgltf_types.hpp"

extern std::filesystem::path path;

TEST_CASE("Load basic GLB file", "[gltf-loader]") {
    fastgltf::Parser parser;
    SECTION("Load basic Box.glb") {
        auto box = parser.loadBinaryGLTF(path / "sample-models" / "2.0" / "Box" / "glTF-Binary" / "Box.glb", fastgltf::Options::None);
        REQUIRE(parser.getError() == fastgltf::Error::None);

        REQUIRE(box->parseBuffers() == fastgltf::Error::None);

        auto asset = box->getParsedAsset();
        REQUIRE(asset->buffers.size() == 1);
        REQUIRE(asset->buffers.front().location == fastgltf::DataLocation::FilePathWithByteRange);
        REQUIRE(asset->buffers.front().data.fileByteOffset == 1014);
    }

    SECTION("Load basic Box.glb and load buffers") {
        auto box = parser.loadBinaryGLTF(path / "sample-models" / "2.0" / "Box" / "glTF-Binary" / "Box.glb", fastgltf::Options::LoadGLBBuffers);
        REQUIRE(parser.getError() == fastgltf::Error::None);

        REQUIRE(box->parseBuffers() == fastgltf::Error::None);

        auto asset = box->getParsedAsset();
        REQUIRE(asset->buffers.size() == 1);

        auto& buffer = asset->buffers.front();
        REQUIRE(buffer.location == fastgltf::DataLocation::VectorWithMime);
        REQUIRE(!buffer.data.bytes.empty());
        REQUIRE(static_cast<uint64_t>(buffer.data.bytes.size() - buffer.byteLength) < 3);
    }
}
