#include <catch2/catch_test_macros.hpp>

#include <fastgltf/types.hpp>

TEST_CASE("Test basic Optional interface", "[optional-tests]") {
	// We have no specialization for std::uint32_t, and therefore this is just
	// a std::optional with a bool field padded by 3 bytes.
	fastgltf::Optional<std::uint32_t> optional;
	static_assert(sizeof(optional) > sizeof(std::uint32_t));
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

TEST_CASE("Test OptionalWithFlagValue in constant expressions", "[optional-tests]") {
	static_assert(!fastgltf::OptionalWithFlagValue<float>().has_value());
	static_assert(!fastgltf::OptionalWithFlagValue<double>().has_value());
	static_assert(!fastgltf::OptionalWithFlagValue<std::size_t>().has_value());

	static_assert(fastgltf::OptionalWithFlagValue<float>(1.0f).has_value());
	static_assert(*fastgltf::OptionalWithFlagValue<float>(1.0f) == 1.0f);
	static_assert(fastgltf::OptionalWithFlagValue<double>().value_or(2.0) == 2.0);

	static_assert([] {
		fastgltf::OptionalWithFlagValue<std::size_t> a;
		a = std::size_t(5);
		fastgltf::OptionalWithFlagValue<std::size_t> b;
		b.emplace(std::size_t(7));
		fastgltf::OptionalWithFlagValue<std::size_t> c;
		c = std::move(b);
		return a.has_value() && *a == 5 && c.has_value() && *c == 7;
	}());

	static_assert([] {
		fastgltf::OptionalWithFlagValue<float> a(1.0f);
		a.reset();
		return !a.has_value();
	}());
}

TEST_CASE("Test OptionalWithFlagValue comparisons with std::nullopt", "[optional-tests]") {
	const fastgltf::OptionalWithFlagValue<float> empty;
	const fastgltf::OptionalWithFlagValue<float> value(1.0f);

	REQUIRE(empty == std::nullopt);
	REQUIRE(std::nullopt == empty);
	REQUIRE(!(empty != std::nullopt));
	REQUIRE(value != std::nullopt);
	REQUIRE(std::nullopt != value);
	REQUIRE(!(value == std::nullopt));

	REQUIRE(((empty <=> std::nullopt) == 0));
	REQUIRE(((value <=> std::nullopt) > 0));
	REQUIRE(std::nullopt < value);
	REQUIRE(!(value < std::nullopt));
	REQUIRE(empty <= std::nullopt);
	REQUIRE(empty >= std::nullopt);
}

TEST_CASE("Test OptionalWithFlagValue comparisons with values", "[optional-tests]") {
	const fastgltf::OptionalWithFlagValue<float> empty;
	const fastgltf::OptionalWithFlagValue<float> one(1.0f);

	REQUIRE(one == 1.0f);
	REQUIRE(1.0f == one);
	REQUIRE(one != 2.0f);
	REQUIRE(!(empty == 1.0f));
	REQUIRE(empty != 1.0f);

	REQUIRE(empty < 1.0f);
	REQUIRE(empty <= 1.0f);
	REQUIRE(!(empty > 1.0f));
	REQUIRE(!(empty >= 1.0f));
	REQUIRE(((empty <=> 1.0f) < 0));

	REQUIRE(one < 2.0f);
	REQUIRE(one <= 1.0f);
	REQUIRE(one > 0.0f);
	REQUIRE(one >= 1.0f);
	REQUIRE(((one <=> 1.0f) == 0));
	REQUIRE(((one <=> 2.0f) < 0));
	REQUIRE(((one <=> 0.0f) > 0));

	const fastgltf::OptionalWithFlagValue<std::size_t> index(std::size_t(3));
	REQUIRE(index == std::size_t(3));
	REQUIRE(index < std::size_t(4));
	REQUIRE(((index <=> std::size_t(3)) == 0));
}

TEST_CASE("Test OptionalWithFlagValue comparisons between optionals", "[optional-tests]") {
	const fastgltf::OptionalWithFlagValue<float> empty1;
	const fastgltf::OptionalWithFlagValue<float> empty2;
	const fastgltf::OptionalWithFlagValue<float> one(1.0f);
	const fastgltf::OptionalWithFlagValue<float> two(2.0f);

	REQUIRE(empty1 == empty2);
	REQUIRE(!(empty1 != empty2));
	REQUIRE(!(empty1 < empty2));
	REQUIRE(empty1 <= empty2);
	REQUIRE(((empty1 <=> empty2) == 0));

	REQUIRE(one == fastgltf::OptionalWithFlagValue<float>(1.0f));
	REQUIRE(one != two);
	REQUIRE(one != empty1);

	REQUIRE(empty1 < one);
	REQUIRE(!(one < empty1));
	REQUIRE(one > empty1);
	REQUIRE(one < two);
	REQUIRE(two >= one);
	REQUIRE(((empty1 <=> one) < 0));
	REQUIRE(((one <=> two) < 0));
	REQUIRE(((two <=> one) > 0));
}

TEST_CASE("Test OptionalWithFlagValue converting move assignment", "[optional-tests]") {
	SECTION("Into an empty optional") {
		fastgltf::OptionalWithFlagValue<double> target;
		fastgltf::OptionalWithFlagValue<float> source(1.5f);
		target = std::move(source);
		REQUIRE(target.has_value());
		REQUIRE(*target == 1.5);
	}

	SECTION("Into an engaged optional") {
		fastgltf::OptionalWithFlagValue<double> target(3.0);
		fastgltf::OptionalWithFlagValue<float> source(1.5f);
		target = std::move(source);
		REQUIRE(target.has_value());
		REQUIRE(*target == 1.5);
	}

	SECTION("From an empty optional") {
		fastgltf::OptionalWithFlagValue<double> target(3.0);
		fastgltf::OptionalWithFlagValue<float> source;
		target = std::move(source);
		REQUIRE(!target.has_value());
	}

	static_assert([] {
		fastgltf::OptionalWithFlagValue<double> target;
		fastgltf::OptionalWithFlagValue<float> source(1.5f);
		target = std::move(source);
		return target.has_value() && *target == 1.5;
	}());
}
