#include <catch2/catch_test_macros.hpp>

#include <fastgltf/error.hpp>

TEST_CASE("Expected<T> constructors", "[expected-tests]") {
	SECTION("Init with value") {
		static constexpr std::string_view str = "This is a string.";
		fastgltf::Expected expected = std::string(str);
		REQUIRE(expected.hasError() == false);
		REQUIRE(expected.error() == fastgltf::Error::None);
		REQUIRE(expected.get() == str);
		REQUIRE(*expected.get_if() == str);
	}

	SECTION("Init from error") {
		fastgltf::Expected<std::string> expected = fastgltf::Error::InvalidGltf;
		REQUIRE(expected.hasError() == true);
		REQUIRE(expected.error() == fastgltf::Error::InvalidGltf);
		REQUIRE(expected.get_if() == nullptr);
	}
}

TEST_CASE("Expected<T> with references", "[expected-tests]") {
	SECTION("Errors") {
		fastgltf::Expected<const std::string&> expected = fastgltf::Error::InvalidGltf;
		REQUIRE(expected.hasError() == true);
		REQUIRE(expected.error() == fastgltf::Error::InvalidGltf);
		REQUIRE(expected.get_if() == nullptr);
	}

	SECTION("Reference from lvalue") {
		std::string str = "This is a very very very long string.";

		fastgltf::Expected<std::string&> expected = str;
		REQUIRE(expected.hasError() == false);
		REQUIRE(expected.error() == fastgltf::Error::None);

		REQUIRE(expected.get() == str);
		REQUIRE(*expected.get_if() == str);
		REQUIRE(&expected.get() == &str);
	}

	SECTION("Reference through std::ref") {
		std::string str = "This is a very very very long string.";

		fastgltf::Expected expected = std::ref(str);
		static_assert(std::is_same_v<decltype(expected), fastgltf::Expected<std::string&>>);

		REQUIRE(expected.hasError() == false);
		REQUIRE(expected.get() == str);
	}

	SECTION("* and -> operators") {
		std::string str = "This is a very very very long string.";

		fastgltf::Expected<std::string&> expected = str;
		REQUIRE(expected.hasError() == false);
		REQUIRE(expected.get() == str);

		REQUIRE(expected.operator->() == &str);
		REQUIRE(&*expected == &str);
	}

	SECTION("Const reference") {
		std::string str = "This is a very very very long string.";
		fastgltf::Expected<const std::string&> expected = std::ref(str);
		REQUIRE(expected.hasError() == false);
		REQUIRE(expected.error() == fastgltf::Error::None);
		REQUIRE(expected.get() == str);
		REQUIRE(*expected.get_if() == str);
		REQUIRE(&expected.get() == &str);
	}

	SECTION("Reference destruction") {
		struct Counter { int* n; ~Counter() { ++*n; } };
		int destroyed = 0;
		{
			Counter c { .n = &destroyed };
			{
				fastgltf::Expected<Counter&> expected = c;
			}
			REQUIRE(destroyed == 0);
		}
		REQUIRE(destroyed == 1);
	}
}
