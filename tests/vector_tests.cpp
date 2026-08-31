#include <catch2/catch_test_macros.hpp>

#include <fastgltf/types.hpp>

TEST_CASE("Verify clz", "[vector-tests]") {
	REQUIRE(fastgltf::clz<std::uint8_t>(0b00000000) == 8);
	REQUIRE(fastgltf::clz<std::uint8_t>(0b00000001) == 7);
	REQUIRE(fastgltf::clz<std::uint8_t>(0b00000010) == 6);
	REQUIRE(fastgltf::clz<std::uint8_t>(0b00000100) == 5);
	REQUIRE(fastgltf::clz<std::uint8_t>(0b00001000) == 4);
	REQUIRE(fastgltf::clz<std::uint8_t>(0b00010000) == 3);
	REQUIRE(fastgltf::clz<std::uint8_t>(0b00100000) == 2);
	REQUIRE(fastgltf::clz<std::uint8_t>(0b01000000) == 1);
	REQUIRE(fastgltf::clz<std::uint8_t>(0b10000000) == 0);
}

TEST_CASE("Test resize/reserve", "[vector-tests]") {
    fastgltf::SmallVector<uint32_t, 4> vec = {1, 2, 3};
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 3);

    vec.resize(5);
    REQUIRE(vec.size() == 5);
    REQUIRE(vec[3] == 0);
    REQUIRE(vec[4] == 0);

    vec.resize(2);
    REQUIRE(vec.size() == 2);
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);

    vec.resize(6, 4);
    REQUIRE(vec.size() == 6);
    for (std::size_t i = 2; i < vec.size(); ++i) {
        REQUIRE(vec[i] == 4);
    }

    vec.reserve(8);
    REQUIRE(vec.size() == 6);
    REQUIRE(vec.capacity() == 8);

	vec.shrink_to_fit();
	REQUIRE(vec.capacity() == 6);
}

TEST_CASE("Test constructors", "[vector-tests]") {
    fastgltf::SmallVector<uint32_t, 4> vec = {0, 1, 2, 3};
    for (std::size_t i = 0; i < vec.size(); ++i) {
        REQUIRE(vec[i] == i);
    }

    fastgltf::SmallVector<uint32_t, 4> vec2(vec);
    for (std::size_t i = 0; i < vec2.size(); ++i) {
        REQUIRE(vec2[i] == i);
    }

    fastgltf::SmallVector<uint32_t, 4> vec3 = std::move(vec2);
    REQUIRE(vec2.empty());
    vec3.resize(6);
    for (std::size_t i = 0; i < 4; ++i) {
        REQUIRE(vec3[i] == i);
    }
    REQUIRE(vec3[4] == 0);
    REQUIRE(vec3[5] == 0);
}

TEST_CASE("Nested SmallVector", "[vector-tests]") {
    fastgltf::SmallVector<fastgltf::SmallVector<uint32_t, 2>, 4> vectors(6, {4}); // This should heap allocate straight away.
    REQUIRE(vectors.size() == 6);
    for (auto& vector : vectors) {
        REQUIRE(vector.size() == 1);
        REQUIRE(vector.front() == 4);
        vector.reserve(6);
    }
}

struct RefCountedObject {
	static inline std::size_t aliveObjects = 0;

	RefCountedObject() {
		++aliveObjects;
	}

	RefCountedObject(const RefCountedObject& other) {
		++aliveObjects;
	}

	RefCountedObject(RefCountedObject&& other) = delete;

	~RefCountedObject() {
		--aliveObjects;
	}
};

TEST_CASE("Test shrinking vectors", "[vector-tests]") {
	fastgltf::SmallVector<RefCountedObject, 4> objects;
	for (std::size_t i = 0; i < 4; ++i) {
		objects.emplace_back();
	}
	REQUIRE(RefCountedObject::aliveObjects == 4);
	objects.emplace_back();
	REQUIRE(RefCountedObject::aliveObjects == 5);
	objects.resize(4);
	REQUIRE(RefCountedObject::aliveObjects == 4);
}

TEST_CASE("Test vectors with polymorphic allocators", "[vector-tests]") {
	fastgltf::pmr::SmallVector<std::uint32_t, 4> ints;
	ints.assign(10, 5);
	REQUIRE(ints.size() == 10);
	REQUIRE(ints.data() != nullptr);
	for (auto& i : ints) {
		REQUIRE(i == 5);
	}
}

TEST_CASE("Test initial value for StaticVector", "[vector-tests]") {
	fastgltf::StaticVector<std::uint32_t> vector(10, 25);
	std::size_t count = 0;
	for (auto& i : vector) {
		REQUIRE(i == 25);
		++count;
	}
	REQUIRE(count == 10);
}

struct MoveOnlyObject {
	std::unique_ptr<int> ptr;

	MoveOnlyObject() : ptr(std::make_unique<int>(0)) {}
	explicit MoveOnlyObject(int v) : ptr(std::make_unique<int>(v)) {}
	MoveOnlyObject(const MoveOnlyObject&) = delete;
	MoveOnlyObject& operator=(const MoveOnlyObject&) = delete;
	MoveOnlyObject(MoveOnlyObject&&) noexcept = default;
	MoveOnlyObject& operator=(MoveOnlyObject&&) noexcept = default;
	~MoveOnlyObject() = default;
};

TEST_CASE("Test move-only types with SmallVector", "[vector-tests]") {
	SECTION("Stack storage move constructor") {
		fastgltf::SmallVector<MoveOnlyObject, 4> vec;
		vec.emplace_back(10);
		vec.emplace_back(20);
		vec.emplace_back(30);

		fastgltf::SmallVector<MoveOnlyObject, 4> vec2 = std::move(vec);
		REQUIRE(vec.empty());
		REQUIRE(vec2.size() == 3);
		REQUIRE(*vec2[0].ptr == 10);
		REQUIRE(*vec2[1].ptr == 20);
		REQUIRE(*vec2[2].ptr == 30);
	}

	SECTION("Stack storage move assignment") {
		fastgltf::SmallVector<MoveOnlyObject, 4> vec;
		vec.emplace_back(10);
		vec.emplace_back(20);

		fastgltf::SmallVector<MoveOnlyObject, 4> vec2;
		vec2.emplace_back(100);
		vec2 = std::move(vec);

		REQUIRE(vec.empty());
		REQUIRE(vec2.size() == 2);
		REQUIRE(*vec2[0].ptr == 10);
		REQUIRE(*vec2[1].ptr == 20);
	}

	SECTION("Heap storage move constructor") {
		fastgltf::SmallVector<MoveOnlyObject, 2> vec;
		vec.emplace_back(1);
		vec.emplace_back(2);
		vec.emplace_back(3);

		REQUIRE(!vec.isUsingStack());
		fastgltf::SmallVector<MoveOnlyObject, 2> vec2 = std::move(vec);
		REQUIRE(vec.empty());
		REQUIRE(vec2.size() == 3);
		REQUIRE(*vec2[0].ptr == 1);
		REQUIRE(*vec2[1].ptr == 2);
		REQUIRE(*vec2[2].ptr == 3);
	}

	SECTION("Heap storage move assignment") {
		fastgltf::SmallVector<MoveOnlyObject, 2> vec;
		vec.emplace_back(1);
		vec.emplace_back(2);
		vec.emplace_back(3);

		fastgltf::SmallVector<MoveOnlyObject, 2> vec2;
		vec2.emplace_back(99);
		vec2 = std::move(vec);

		REQUIRE(vec.empty());
		REQUIRE(vec2.size() == 3);
		REQUIRE(*vec2[0].ptr == 1);
		REQUIRE(*vec2[1].ptr == 2);
		REQUIRE(*vec2[2].ptr == 3);
	}
}

