#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "fastgltf_parser.hpp"
#include "fastgltf_types.hpp"
#include "gltf_path.hpp"

TEST_CASE("Load basic GLB file", "[gltf-loader]") {
    fastgltf::Parser parser;
    auto folder = sampleModels / "2.0" / "Box" / "glTF-Binary";
    fastgltf::GltfDataBuffer buffer;
    REQUIRE(buffer.loadFromFile(folder / "Box.glb"));

    SECTION("Load basic Box.glb") {
        auto box = parser.loadBinaryGLTF(&buffer, folder, fastgltf::Options::None);
        REQUIRE(parser.getError() == fastgltf::Error::None);

        REQUIRE(box->parse(fastgltf::Category::Buffers) == fastgltf::Error::None);

        auto asset = box->getParsedAsset();
        REQUIRE(asset->buffers.size() == 1);
        REQUIRE(asset->buffers.front().location == fastgltf::DataLocation::FilePathWithByteRange);
        REQUIRE(asset->buffers.front().data.fileByteOffset == 1016);
    }

    SECTION("Load basic Box.glb and load buffers") {
        auto box = parser.loadBinaryGLTF(&buffer, folder, fastgltf::Options::LoadGLBBuffers);
        REQUIRE(parser.getError() == fastgltf::Error::None);

        REQUIRE(box->parse(fastgltf::Category::Buffers) == fastgltf::Error::None);

        auto asset = box->getParsedAsset();
        REQUIRE(asset->buffers.size() == 1);

        auto& buffer1 = asset->buffers.front();
        REQUIRE(buffer1.location == fastgltf::DataLocation::VectorWithMime);
        REQUIRE(!buffer1.data.bytes.empty());
        REQUIRE(static_cast<uint64_t>(buffer1.data.bytes.size() - buffer1.byteLength) < 3);
    }

    SECTION("Load GLB by bytes") {
        std::ifstream file(folder / "Box.glb", std::ios::binary | std::ios::ate);
        auto length = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ifstream::beg);
        std::vector<uint8_t> bytes(length + fastgltf::getGltfBufferPadding());
        file.read(reinterpret_cast<char*>(bytes.data()), length);

        fastgltf::GltfDataBuffer byteBuffer;
        byteBuffer.fromByteView(bytes.data(), length, length + fastgltf::getGltfBufferPadding());

        auto box = parser.loadBinaryGLTF(&byteBuffer, folder, fastgltf::Options::LoadGLBBuffers);
        REQUIRE(parser.getError() == fastgltf::Error::None);

        REQUIRE(box->parse(fastgltf::Category::Buffers) == fastgltf::Error::None);

        auto asset = box->getParsedAsset();
    }
}
