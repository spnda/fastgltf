#include <fstream>
#include <sstream>

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <fastgltf/base64.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/core.hpp>
#include "gltf_path.hpp"

constexpr std::string_view testBase64 = "SGVsbG8gV29ybGQuIEhlbGxvIFdvcmxkLiBIZWxsbyBXb3JsZC4=";

TEST_CASE("Check base64 utility functions", "[base64]") {
	REQUIRE(fastgltf::base64::getPadding("Li==") == 2);
	REQUIRE(fastgltf::base64::getPadding("Li4=") == 1);
	REQUIRE(fastgltf::base64::getPadding("Li4u") == 0);

	REQUIRE(fastgltf::base64::getDecodedSize(4, 0) == 3); // Li4u
	REQUIRE(fastgltf::base64::getDecodedSize(4, 1) == 2); // Li4=
	REQUIRE(fastgltf::base64::getDecodedSize(4, 2) == 1); // Li==
}

TEST_CASE("Check base64 decoding", "[base64]") {
	// This is "Hello World. Hello World.". The decode function
	// uses the best possible SIMD version of the algorithm.
	auto bytes = fastgltf::base64::decode(testBase64);
	REQUIRE(bytes.error() == fastgltf::Error::None);
	std::string strings(bytes->begin(), bytes->end());
	REQUIRE(strings == "Hello World. Hello World. Hello World.");
}

TEST_CASE("Test base64 buffer decoding", "[base64]") {
	fastgltf::Parser parser;
	fastgltf::Image texture;
	std::string bufferData;

	auto cylinderEngine = sampleAssets / "Models" / "MetalRoughSpheres" / "glTF-Embedded";
	auto boxTextured = sampleAssets / "Models" / "BoxTextured" / "glTF-Embedded";

	fastgltf::GltfFileStream tceJsonData(cylinderEngine / "MetalRoughSpheres.gltf");
	REQUIRE(tceJsonData.isOpen());
	fastgltf::GltfFileStream btJsonData(boxTextured / "BoxTextured.gltf");
	REQUIRE(btJsonData.isOpen());

	SECTION("Validate large buffer load from glTF") {
		auto asset = parser.loadGltfJson(tceJsonData, cylinderEngine, fastgltf::Options::None, fastgltf::Category::Buffers);
		REQUIRE(asset.error() == fastgltf::Error::None);

		REQUIRE(asset->buffers.size() == 1);

		// Load the buffer from the parsed glTF file.
		auto& buffer = asset->buffers.front();
		REQUIRE(buffer.byteLength == 11199904);
		auto bufferVector = std::get_if<fastgltf::sources::Array>(&buffer.data);
		REQUIRE(bufferVector != nullptr);
		REQUIRE(bufferVector->mimeType == fastgltf::MimeType::OctetStream);
		REQUIRE(!bufferVector->bytes.empty());
	}

	SECTION("Validate base64 buffer and image load from glTF") {
		auto asset = parser.loadGltfJson(btJsonData, boxTextured, fastgltf::Options::None, fastgltf::Category::Images | fastgltf::Category::Buffers);
		REQUIRE(asset.error() == fastgltf::Error::None);

		REQUIRE(asset->buffers.size() == 1);
		REQUIRE(asset->images.size() == 1);

		auto& buffer = asset->buffers.front();
		REQUIRE(buffer.byteLength == 840);
		auto bufferVector = std::get_if<fastgltf::sources::Array>(&buffer.data);
		REQUIRE(bufferVector != nullptr);
		REQUIRE(bufferVector->mimeType == fastgltf::MimeType::OctetStream);
		REQUIRE(!bufferVector->bytes.empty());

		auto& image = asset->images.front();
		auto imageVector = std::get_if<fastgltf::sources::Array>(&image.data);
		REQUIRE(imageVector != nullptr);
		REQUIRE(imageVector->mimeType == fastgltf::MimeType::PNG);
		REQUIRE(!imageVector->bytes.empty());
	}
}
