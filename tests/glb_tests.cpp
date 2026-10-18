#include <fstream>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include "gltf_path.hpp"

TEST_CASE("Load basic GLB file", "[gltf-loader]") {
	auto folder = sampleAssets / "Models" / "Box" / "glTF-Binary";
	auto jsonData = fastgltf::GltfDataBuffer::FromPath(folder / "Box.glb");
	REQUIRE(jsonData.error() == fastgltf::Error::None);

	fastgltf::Parser parser;
	SECTION("Load basic Box.glb") {
		auto asset = parser.loadGltfBinary(jsonData.get(), folder, fastgltf::Options::None, fastgltf::Category::Buffers);
		REQUIRE(asset.error() == fastgltf::Error::None);
		REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

		REQUIRE(asset->buffers.size() == 1);

		auto& buffer = asset->buffers.front();
		auto* array = std::get_if<fastgltf::sources::Array>(&buffer.data);
		REQUIRE(array != nullptr);
		REQUIRE(array->bytes.size() == 1664 - 1016);
	}

	SECTION("Load basic Box.glb and load buffers") {
		auto asset = parser.loadGltfBinary(jsonData.get(), folder, fastgltf::Options::None, fastgltf::Category::Buffers);
		REQUIRE(asset.error() == fastgltf::Error::None);
		REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

		REQUIRE(asset->buffers.size() == 1);

		auto& buffer = asset->buffers.front();
		auto* bufferVector = std::get_if<fastgltf::sources::Array>(&buffer.data);
		REQUIRE(bufferVector != nullptr);
		REQUIRE(!bufferVector->bytes.empty());
		REQUIRE(static_cast<uint64_t>(bufferVector->bytes.size() - buffer.byteLength) < 3);
	}

	SECTION("Load GLB by bytes") {
		std::ifstream file(folder / "Box.glb", std::ios::binary | std::ios::ate);
		auto length = static_cast<size_t>(file.tellg());
		file.seekg(0, std::ifstream::beg);
		std::vector<uint8_t> bytes(length);
		file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length));

		auto byteBuffer = fastgltf::GltfDataBuffer::FromBytes(
				reinterpret_cast<const std::byte*>(bytes.data()), length);
		REQUIRE(byteBuffer.error() == fastgltf::Error::None);

		auto asset = parser.loadGltfBinary(byteBuffer.get(), folder, fastgltf::Options::None, fastgltf::Category::Buffers);
		REQUIRE(asset.error() == fastgltf::Error::None);
	}
}

TEST_CASE("glTF binary format version 3", "[gltf-loader][write-tests][gltf-2.1]") {
	std::string_view json;
	fastgltf::Version version;
	std::uint32_t expectedBinaryVersion;

	SECTION("glTF advertising 2.0") {
		json = R"({"asset":{"version":"2.0"}})";
		version = fastgltf::Version { 2, 0 };
		expectedBinaryVersion = 2;
	}
	SECTION("glTF advertising 2.1") {
		json = R"({"asset":{"version":"2.1"}})";
		version = fastgltf::Version { 2, 1 };
		expectedBinaryVersion = 2;
	}
	SECTION("glTF advertising 2.1 with 2.1 minimum") {
		json = R"({"asset":{"version":"2.1","minVersion":"2.1"}})";
		version = fastgltf::Version { 2, 1 };
		expectedBinaryVersion = 3;
	}
	SECTION("glTF advertising 2.1 with 2.0 compatibility") {
		json = R"({"asset":{"version":"2.1","minVersion":"2.0"}})";
		version = fastgltf::Version { 2, 1 };
		expectedBinaryVersion = 2;
	}

	CAPTURE(json);
	CAPTURE(version);
	CAPTURE(expectedBinaryVersion);

	auto data = fastgltf::GltfDataBuffer::FromBytes(reinterpret_cast<const std::byte*>(json.data()), json.size());
	REQUIRE(!data.has_error());

	auto asset = fastgltf::Parser().loadGltfJson(data.get(), {});
	REQUIRE(!asset.has_error());

	CHECK(asset->assetInfo.version == version);

	auto glb = fastgltf::Exporter().writeGltfBinary(asset.get());
	REQUIRE(!glb.has_error());

	// The fifth byte is the first byte of the second 32-bit int, which is the version number
	CHECK(static_cast<std::uint8_t>(std::span(glb->output)[4]) == expectedBinaryVersion);

	auto glbData = fastgltf::GltfDataBuffer::FromBytes(glb->output.data(), glb->output.size());
	REQUIRE(!glbData.has_error());

	auto roundtripAsset = fastgltf::Parser().loadGltfBinary(glbData.get(), {});
	REQUIRE(!roundtripAsset.has_error());

	CHECK(asset->assetInfo.version == version);
}
