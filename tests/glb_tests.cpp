#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "fastgltf_parser.hpp"
#include "fastgltf_types.hpp"
#include "gltf_path.hpp"

TEST_CASE("Load basic GLB file", "[gltf-loader]") {
    fastgltf::Parser parser;
    auto folder = sampleModels / "2.0" / "Box" / "glTF-Binary";
    fastgltf::GltfDataBuffer jsonData;
    REQUIRE(jsonData.loadFromFile(folder / "Box.glb"));

    SECTION("Load basic Box.glb") {
        auto box = parser.loadBinaryGLTF(&jsonData, folder, fastgltf::Options::None);
        REQUIRE(parser.getError() == fastgltf::Error::None);

        REQUIRE(box->parse(fastgltf::Category::Buffers) == fastgltf::Error::None);

        auto asset = box->getParsedAsset();
        REQUIRE(asset->buffers.size() == 1);

        auto& buffer = asset->buffers.front();
        auto* bufferFile = std::get_if<fastgltf::sources::FilePath>(&buffer.data);
        REQUIRE(bufferFile != nullptr);
        REQUIRE(bufferFile->fileByteOffset == 1016);
    }

    SECTION("Load basic Box.glb and load buffers") {
        auto box = parser.loadBinaryGLTF(&jsonData, folder, fastgltf::Options::LoadGLBBuffers);
        REQUIRE(parser.getError() == fastgltf::Error::None);

        REQUIRE(box->parse(fastgltf::Category::Buffers) == fastgltf::Error::None);

        auto asset = box->getParsedAsset();
        REQUIRE(asset->buffers.size() == 1);

        auto& buffer = asset->buffers.front();
        auto* bufferVector = std::get_if<fastgltf::sources::Vector>(&buffer.data);
        REQUIRE(bufferVector != nullptr);
        REQUIRE(!bufferVector->bytes.empty());
        REQUIRE(static_cast<uint64_t>(bufferVector->bytes.size() - buffer.byteLength) < 3);
    }

    SECTION("Load GLB by bytes") {
        std::ifstream file(folder / "Box.glb", std::ios::binary | std::ios::ate);
        auto length = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ifstream::beg);
        std::vector<uint8_t> bytes(length + fastgltf::getGltfBufferPadding());
        file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length));

        fastgltf::GltfDataBuffer byteBuffer;
        REQUIRE(byteBuffer.fromByteView(bytes.data(), length, length + fastgltf::getGltfBufferPadding()));

        auto box = parser.loadBinaryGLTF(&byteBuffer, folder, fastgltf::Options::LoadGLBBuffers);
        REQUIRE(parser.getError() == fastgltf::Error::None);

        REQUIRE(box->parse(fastgltf::Category::Buffers) == fastgltf::Error::None);

        auto asset = box->getParsedAsset();
    }
}
