#include <catch2/catch_test_macros.hpp>

#include <fastgltf/core.hpp>
#include "gltf_path.hpp"

TEST_CASE("Test simple glTF composition", "[write-tests]") {
	fastgltf::BufferView bufferView = {};
	bufferView.bufferIndex = 0;
	bufferView.byteStride = 4;
	bufferView.byteLength = 16;

	fastgltf::Asset asset;
	asset.bufferViews.emplace_back(std::move(bufferView));

	fastgltf::Exporter exporter;
	auto result = exporter.writeGLTF(&asset);
	REQUIRE(result.error() == fastgltf::Error::None);
	REQUIRE(!result.get().output.empty());
}

TEST_CASE("Read glTF, write it, and then read it again and validate", "[write-tests]") {
	auto cubePath = sampleModels / "2.0" / "Cube" / "glTF";
	auto cubeJsonData = std::make_unique<fastgltf::GltfDataBuffer>();
	REQUIRE(cubeJsonData->loadFromFile(cubePath / "Cube.gltf"));

	fastgltf::Parser parser;
	auto cube = parser.loadGLTF(cubeJsonData.get(), cubePath);
	REQUIRE(cube.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(cube.get()) == fastgltf::Error::None);

	fastgltf::Exporter exporter;
	auto expected = exporter.writeGLTF(&(cube.get()));
    REQUIRE(expected.error() == fastgltf::Error::None);

	fastgltf::GltfDataBuffer cube2JsonData;
	cube2JsonData.copyBytes(reinterpret_cast<const uint8_t*>(expected.get().output.data()),
                            expected.get().output.size());
	auto cube2 = parser.loadGLTF(&cube2JsonData, cubePath);
	REQUIRE(cube2.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(cube2.get()) == fastgltf::Error::None);
}

TEST_CASE("Try writing a glTF with all buffers and images", "[write-tests]") {
    auto cubePath = sampleModels / "2.0" / "Cube" / "glTF";

    fastgltf::GltfDataBuffer gltfDataBuffer;
    gltfDataBuffer.loadFromFile(cubePath / "Cube.gltf");

    fastgltf::Parser parser;
    auto options = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;
    auto cube = parser.loadGLTF(&gltfDataBuffer, cubePath, options);
    REQUIRE(cube.error() == fastgltf::Error::None);

    fastgltf::FileExporter exporter;
    auto error = exporter.writeGLTF(&cube.get(), path / "export" / "cube.gltf");
    REQUIRE(error == fastgltf::Error::None);
}
