#include <catch2/catch_test_macros.hpp>

#include <fastgltf/types.hpp>

TEST_CASE("Test basic Optional interface", "[optional-tests]") {
	// We have no specialization for std::uint32_t, and therefore this is just
	// a std::optional with a bool field padded by 3 bytes.
	fastgltf::Optional<std::uint32_t> optional;
	REQUIRE(sizeof(optional) > sizeof(std::uint32_t));
}

TEST_CASE("Test Optional float specialization", "[optional-tests]") {
	fastgltf::Optional<float> foptional;
	REQUIRE(!foptional.has_value());
	if constexpr (std::numeric_limits<float>::is_iec559) {
		REQUIRE(sizeof(foptional) == sizeof(float));
	} else {
		REQUIRE(sizeof(foptional) > sizeof(float));
	}

	fastgltf::Optional<double> doptional;
	REQUIRE(!doptional.has_value());
	if constexpr (std::numeric_limits<double>::is_iec559) {
		REQUIRE(sizeof(doptional) == sizeof(double));
	} else {
		REQUIRE(sizeof(doptional) > sizeof(double));
	}
}
