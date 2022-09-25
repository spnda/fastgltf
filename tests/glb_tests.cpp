#include <catch2/catch_test_macros.hpp>

#include "fastgltf_parser.hpp"
#include "fastgltf_types.hpp"

TEST_CASE("Load basic GLB file", "[gltf-loader]") {
    auto path = std::filesystem::path { __FILE__ }.parent_path() / "gltf";

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
        REQUIRE(asset->buffers.front().location == fastgltf::DataLocation::VectorWithMime);
        REQUIRE(!asset->buffers.front().data.bytes.empty());
    }
}
