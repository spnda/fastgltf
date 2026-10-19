#include <fastgltf/core.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Detect invalid or unsupported versions", "[gltf-loader][gltf-2.1]") {
	std::string_view json;
	fastgltf::Error expectedError;
	std::optional<fastgltf::Version> expectedVersion;

	SECTION("Empty version") {
		json = R"({"asset":{"version":""}})";
		expectedError = fastgltf::Error::InvalidVersion;
	}
	SECTION("Long semantic version") {
		json = R"({"asset":{"version":"2.0.0"}})";
		expectedError = fastgltf::Error::InvalidVersion;
	}
	SECTION("Random characters") {
		json = R"({"asset":{"version":"abc"}})";
		expectedError = fastgltf::Error::InvalidVersion;
	}
	SECTION("Hex version") {
		json = R"({"asset":{"version":"A.B"}})";
		expectedError = fastgltf::Error::InvalidVersion;
	}

	SECTION("Multi-digit version") {
		json = R"({"asset":{"version":"20.0"}})";
		expectedError = fastgltf::Error::UnsupportedVersion;
	}
	SECTION("Multi-digit minor version") {
		json = R"({"asset":{"version":"2.100"}})";
		expectedError = fastgltf::Error::None;
		expectedVersion = fastgltf::Version{2, 100};
	}
	SECTION("Preceeding zeroes in version") {
		json = R"({"asset":{"version":"02.000"}})";
		expectedError = fastgltf::Error::None;
		expectedVersion = fastgltf::Version{2, 0};
	}

	SECTION("Version 1.0") {
		json = R"({"asset":{"version":"1.0"}})";
		expectedError = fastgltf::Error::UnsupportedVersion;
	}
	SECTION("Minimum version 1.0") {
		json = R"({"asset":{"version":"2.0", "minVersion":"1.0"}})";
		expectedError = fastgltf::Error::None;
		expectedVersion = fastgltf::Version{2, 0};
	}
	SECTION("Version 2.0") {
		json = R"({"asset":{"version":"2.0"}})";
		expectedError = fastgltf::Error::None;
		expectedVersion = fastgltf::Version{2, 0};
	}
	SECTION("Version 2.2") {
		json = R"({"asset":{"version":"2.2"}})";
		expectedError = fastgltf::Error::None;
		expectedVersion = fastgltf::Version{2, 2};
	}
	SECTION("Version 2.2 with compatible minVersion") {
		json = R"({"asset":{"version":"2.2", "minVersion":"2.0"}})";
		expectedError = fastgltf::Error::None;
		expectedVersion = fastgltf::Version{2, 2};
	}
	SECTION("Minimum version 2.2") {
		json = R"({"asset":{"version":"2.2", "minVersion":"2.2"}})";
		expectedError = fastgltf::Error::UnsupportedVersion;
	}

	CAPTURE(json);
	CAPTURE(expectedError);

	auto data = fastgltf::GltfDataBuffer::FromBytes(reinterpret_cast<const std::byte*>(json.data()), json.size());
	REQUIRE(!data.has_error());

	auto asset = fastgltf::Parser().loadGltfJson(data.get(), {});
	if (expectedError != fastgltf::Error::None) {
		REQUIRE(asset.has_error());
		CHECK(asset.error() == expectedError);
	} else {
		REQUIRE(asset.has_value());
		CHECK(expectedVersion == asset->assetInfo.version);
	}
}

TEST_CASE("Detect invalid version combinations in validation", "[gltf-loader]") {
	constexpr std::string_view json = R"({"asset":{"version":"2.0", "minVersion":"2.1"}})";
	CAPTURE(json);

	auto data = fastgltf::GltfDataBuffer::FromBytes(reinterpret_cast<const std::byte*>(json.data()), json.size());
	REQUIRE(!data.has_error());

	auto asset = fastgltf::Parser().loadGltfJson(data.get(), {});
	REQUIRE(asset.has_value());

	CHECK(asset->assetInfo.version == fastgltf::Version { 2, 0 });
	CHECK(asset->assetInfo.minVersion == fastgltf::Version { 2, 1 });

	CHECK(fastgltf::validate(asset.get()) == fastgltf::Error::InvalidVersion);
}
