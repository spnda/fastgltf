#include <fastgltf/core.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Detect valid/invalid version strings", "[gltf-loader][gltf-2.1]") {
	using namespace fastgltf;

	// These are the example version strings from the glTF 2.1 specification that are deemed valid
	CHECK(detail::parseVersionString("0.0").has_value());
	CHECK(detail::parseVersionString("0.12").has_value());
	CHECK(detail::parseVersionString("2.0").has_value());
	CHECK(detail::parseVersionString("12.34").has_value());
	CHECK(detail::parseVersionString("999999999.999999999").has_value());

	// These are the example version strings from the glTF 2.1 specification that are deemed invalid
	CHECK(detail::parseVersionString("+1.0").has_error());
	CHECK(detail::parseVersionString("2.0.1").has_error());
	CHECK(detail::parseVersionString("2.00").has_error());
	CHECK(detail::parseVersionString("01.1").has_error());
}

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
	SECTION("Preceeding zeroes in version") {
		json = R"({"asset":{"version":"02.000"}})";
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

	auto data = fastgltf::GltfDataString(json);
	auto asset = fastgltf::Parser().loadGltfJson(data, {});
	if (expectedError != fastgltf::Error::None) {
		REQUIRE(asset.has_error());
		CHECK(asset.error() == expectedError);
	} else {
		REQUIRE(asset.has_value());
		CHECK(expectedVersion == asset->assetInfo.version);
	}
}

// TODO: Re-add the following test when actual glTF 2.1 support is added. Currently the parser
//       rejects where minVersion is anything but "2.0".
#if 0
TEST_CASE("Detect invalid version combinations in validation", "[gltf-loader]") {
	constexpr std::string_view json = R"({"asset":{"version":"2.0", "minVersion":"2.1"}})";
	CAPTURE(json);

	auto data = fastgltf::GltfDataString(json);
	auto asset = fastgltf::Parser().loadGltfJson(data, {});
	REQUIRE(asset.has_value());

	CHECK(asset->assetInfo.version == fastgltf::Version { 2, 0 });
	CHECK(asset->assetInfo.minVersion == fastgltf::Version { 2, 1 });

	CHECK(fastgltf::validate(asset.get()) == fastgltf::Error::InvalidVersion);
}
#endif

TEST_CASE("Version comparisons", "[gltf-loader]") {
	using namespace fastgltf;
	CHECK(Version(2, 0) < Version(3, 0));
	CHECK(Version(2, 0) < Version(2, 1));
	CHECK(Version(2, 1) < Version(2, 10));
	CHECK(Version(2, 10) < Version(3, 0));

	REQUIRE(detail::parseVersionString("2.0").has_value());
	REQUIRE(detail::parseVersionString("2.1").has_value());
	REQUIRE(detail::parseVersionString("2.10").has_value());
	REQUIRE(detail::parseVersionString("3.0").has_value());

	CHECK(*detail::parseVersionString("2.0") < *detail::parseVersionString("3.0"));
	CHECK(*detail::parseVersionString("2.0") < *detail::parseVersionString("2.1"));
	CHECK(*detail::parseVersionString("2.1") < *detail::parseVersionString("2.10"));
	CHECK(*detail::parseVersionString("2.10") < *detail::parseVersionString("3.0"));
}
