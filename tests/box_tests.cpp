#include <catch2/catch_test_macros.hpp>

#include <fastgltf/containers/box.hpp>

TEST_CASE("Box constructors", "[box-tests]") {
	SECTION("Default constructor") {
		fastgltf::box<std::uint64_t> box;
		REQUIRE(*box == std::uint64_t{});
	}
	SECTION("U&& constructor") {
		constexpr std::string_view str = "Hello World!";
		std::string copy(str);
		fastgltf::box box(std::move(copy));
		REQUIRE(*box == str);
		REQUIRE(copy.empty());
	}
	SECTION("Inplace constructors") {
		SECTION("Multiple arguments") {
			fastgltf::box<std::string> box(std::in_place, 3, 'a');
			REQUIRE(box == "aaa");
		}

		SECTION("std::initializer_list") {
			fastgltf::box<std::vector<int>> list(std::in_place, { 3, 4 });
			fastgltf::box<std::vector<int>> args(std::in_place, 3, 4);
			REQUIRE(*list == std::vector { 3, 4 });
			REQUIRE(*args == std::vector { 4, 4, 4 });
		}
	}

	SECTION("Move constructors") {
		std::string str = "Hello World!";
		fastgltf::box box1(str);
		REQUIRE(*box1 == str);
		fastgltf::box box2(std::move(box1));
		REQUIRE(box1.valueless_after_move());
		REQUIRE(*box2 == str);
	}
}

TEST_CASE("Allocator behavior", "[box-tests]") {
	SECTION("Passes allocator to inner object") {
		std::pmr::monotonic_buffer_resource buffer;
		fastgltf::pmr::box<std::pmr::string> box(std::allocator_arg, &buffer, std::in_place, 64, 'x');
		REQUIRE(box.get_allocator().resource() == &buffer);
		REQUIRE(box->get_allocator().resource() == &buffer);
	}

	SECTION("Move can steal objects") {
		fastgltf::box source(5);
		const auto* addr = &*source;
		fastgltf::box moved(std::move(source));
		REQUIRE(source.valueless_after_move());
		REQUIRE(&*moved == addr);
	}

	SECTION("Valueless source") {
		fastgltf::box source(5);
		fastgltf::box sink(std::move(source));
		fastgltf::box copy(source);
		fastgltf::box move(std::move(source));
		REQUIRE((copy.valueless_after_move() && move.valueless_after_move()));
	}
}

TEST_CASE("Box equality", "[box-tests]") {
	SECTION("Two boxes") {
		fastgltf::box<std::uint64_t> box1(5);
		fastgltf::box<std::uint64_t> box2(5);
		REQUIRE(box1 == box2);

		box1 = 10;
		REQUIRE(box1 != box2);
		REQUIRE(box1 > box2);
	}

	SECTION("Box with value") {
		fastgltf::box<std::uint64_t> box1(5);
		REQUIRE(box1 == 5);

		box1 = 10;
		REQUIRE(box1 != 5);
		REQUIRE(box1 > 5);
	}
}

TEST_CASE("Box hashing", "[box-tests]") {
	SECTION("std::uint64_t") {
		fastgltf::box<std::uint64_t> boxed(100);
		REQUIRE(std::hash<decltype(boxed)>{}(boxed) == std::hash<std::uint64_t>{}(*boxed));
	}

	SECTION("std::string") {
		fastgltf::box<std::string> boxed("Hello World!");
		REQUIRE(std::hash<decltype(boxed)>{}(boxed) == std::hash<std::string>{}(*boxed));
	}
}
