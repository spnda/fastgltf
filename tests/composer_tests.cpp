#include <catch2/catch_test_macros.hpp>

#include <fastgltf/core.hpp>
#include "gltf_path.hpp"

TEST_CASE("Test simple glTF composition", "[composer-tests]") {
	fastgltf::BufferView bufferView = {};
	bufferView.bufferIndex = 0;
	bufferView.byteStride = 4;
	bufferView.byteLength = 16;

	fastgltf::Asset asset;
	asset.bufferViews.emplace_back(std::move(bufferView));

	fastgltf::Composer composer;
	auto string = composer.writeGLTF(&asset);
	REQUIRE(composer.getError() == fastgltf::Error::None);
	REQUIRE(!string.empty());
}

#include <fstream>

TEST_CASE("Read glTF, write it, and then read it again and validate", "[composer-tests]") {
	auto cubePath = sampleModels / "2.0" / "Cube" / "glTF";
	auto cubeJsonData = std::make_unique<fastgltf::GltfDataBuffer>();
	REQUIRE(cubeJsonData->loadFromFile(cubePath / "Cube.gltf"));

	fastgltf::Parser parser;
	auto cube = parser.loadGLTF(cubeJsonData.get(), cubePath);
	REQUIRE(cube.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(cube.get()) == fastgltf::Error::None);

	fastgltf::Composer composer;
	auto json = composer.writeGLTF(&(cube.get()));

	fastgltf::GltfDataBuffer cube2JsonData;
	cube2JsonData.copyBytes(reinterpret_cast<const uint8_t*>(json.data()), json.size());
	auto cube2 = parser.loadGLTF(&cube2JsonData, cubePath);
	REQUIRE(cube2.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(cube2.get()) == fastgltf::Error::None);
}
