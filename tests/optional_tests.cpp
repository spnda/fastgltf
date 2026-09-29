#include <catch2/catch_test_macros.hpp>

#include <fastgltf/containers/flagged_optional.hpp>

TEST_CASE("Test basic Optional interface", "[optional-tests]") {
	// We have no specialization for std::uint32_t, and therefore this is just
	// a std::optional with a bool field padded by 3 bytes.
	fastgltf::optional<std::uint32_t> optional;
	static_assert(sizeof(optional) > sizeof(std::uint32_t));
}

TEST_CASE("Test OptionalWithFlagValue copy and move operations", "[optional-tests]") {
	fastgltf::flagged_optional<std::size_t> value(std::size_t(5));
	fastgltf::flagged_optional<std::size_t> empty;

	auto copy = value;
	auto moved = std::move(copy);
	REQUIRE(moved.has_value());
	REQUIRE(*moved == 5);

	moved = empty;
	REQUIRE(!moved.has_value());
	moved = std::move(value);
	REQUIRE(*moved == 5);
}

TEST_CASE("Test Optional float specialization", "[optional-tests]") {
	fastgltf::optional<float> foptional;
	REQUIRE(!foptional.has_value());
	if constexpr (std::numeric_limits<float>::is_iec559) {
		REQUIRE(sizeof(foptional) == sizeof(float));
	} else {
		REQUIRE(sizeof(foptional) > sizeof(float));
	}

	fastgltf::optional<double> doptional;
	REQUIRE(!doptional.has_value());
	if constexpr (std::numeric_limits<double>::is_iec559) {
		REQUIRE(sizeof(doptional) == sizeof(double));
	} else {
		REQUIRE(sizeof(doptional) > sizeof(double));
	}
}

TEST_CASE("Test OptionalWithFlagValue in constant expressions", "[optional-tests]") {
	static_assert(!fastgltf::flagged_optional<float>().has_value());
	static_assert(!fastgltf::flagged_optional<double>().has_value());
	static_assert(!fastgltf::flagged_optional<std::size_t>().has_value());

	static_assert(fastgltf::flagged_optional<float>(1.0f).has_value());
	static_assert(*fastgltf::flagged_optional<float>(1.0f) == 1.0f);
	static_assert(fastgltf::flagged_optional<double>().value_or(2.0) == 2.0);

	static_assert([] {
		fastgltf::flagged_optional<std::size_t> a;
		a = std::size_t(5);
		fastgltf::flagged_optional<std::size_t> b;
		b.emplace(std::size_t(7));
		fastgltf::flagged_optional<std::size_t> c;
		c = std::move(b);
		return a.has_value() && *a == 5 && c.has_value() && *c == 7;
	}());

	static_assert([] {
		fastgltf::flagged_optional<float> a(1.0f);
		a.reset();
		return !a.has_value();
	}());
}

TEST_CASE("Test OptionalWithFlagValue comparisons with std::nullopt", "[optional-tests]") {
	const fastgltf::flagged_optional<float> empty;
	const fastgltf::flagged_optional<float> value(1.0f);

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
	const fastgltf::flagged_optional<float> empty;
	const fastgltf::flagged_optional<float> one(1.0f);

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

	const fastgltf::flagged_optional<std::size_t> index(std::size_t(3));
	REQUIRE(index == std::size_t(3));
	REQUIRE(index < std::size_t(4));
	REQUIRE(((index <=> std::size_t(3)) == 0));
}

TEST_CASE("Test OptionalWithFlagValue comparisons between optionals", "[optional-tests]") {
	const fastgltf::flagged_optional<float> empty1;
	const fastgltf::flagged_optional<float> empty2;
	const fastgltf::flagged_optional<float> one(1.0f);
	const fastgltf::flagged_optional<float> two(2.0f);

	REQUIRE(empty1 == empty2);
	REQUIRE(!(empty1 != empty2));
	REQUIRE(!(empty1 < empty2));
	REQUIRE(empty1 <= empty2);
	REQUIRE(((empty1 <=> empty2) == 0));

	REQUIRE(one == fastgltf::flagged_optional<float>(1.0f));
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
		fastgltf::flagged_optional<double> target;
		fastgltf::flagged_optional<float> source(1.5f);
		target = std::move(source);
		REQUIRE(target.has_value());
		REQUIRE(*target == 1.5);
	}

	SECTION("Into an engaged optional") {
		fastgltf::flagged_optional<double> target(3.0);
		fastgltf::flagged_optional<float> source(1.5f);
		target = std::move(source);
		REQUIRE(target.has_value());
		REQUIRE(*target == 1.5);
	}

	SECTION("From an empty optional") {
		fastgltf::flagged_optional<double> target(3.0);
		fastgltf::flagged_optional<float> source;
		target = std::move(source);
		REQUIRE(!target.has_value());
	}

	static_assert([] {
		fastgltf::flagged_optional<double> target;
		fastgltf::flagged_optional<float> source(1.5f);
		target = std::move(source);
		return target.has_value() && *target == 1.5;
	}());
}

TEST_CASE("Test OptionalWithFlagValue swap", "[optional-tests]") {
	using Opt = fastgltf::flagged_optional<std::size_t>;

	Opt a(std::size_t(1)), b(std::size_t(2));
	a.swap(b);
	REQUIRE((*a == 2 && *b == 1));

	Opt value(std::size_t(3)), empty;
	value.swap(empty);
	REQUIRE(!value.has_value());
	REQUIRE(*empty == 3);

	value.swap(empty);
	REQUIRE(*value == 3);
	REQUIRE(!empty.has_value());

	Opt empty2;
	empty.swap(empty2);
	REQUIRE((!empty.has_value() && !empty2.has_value()));
}

TEST_CASE("Test OptionalWithFlagValue or_else", "[optional-tests]") {
	using Opt = fastgltf::flagged_optional<std::size_t>;
	const auto fallback = [] { return Opt(std::size_t(42)); };

	const Opt value(std::size_t(1));
	const Opt empty;
	static_assert(std::is_same_v<decltype(value.or_else(fallback)), Opt>);

	REQUIRE(*value.or_else(fallback) == 1);
	REQUIRE(*empty.or_else(fallback) == 42);
	REQUIRE(*Opt(std::size_t(1)).or_else(fallback) == 1);
	REQUIRE(*Opt().or_else(fallback) == 42);
	REQUIRE(!Opt().or_else([] { return Opt(); }).has_value());
}
