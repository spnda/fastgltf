#include <memory_resource>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include <fastgltf/containers/small_vector.hpp>
#include <fastgltf/containers/static_vector.hpp>

TEST_CASE("Test resizing and allocation behaviour", "[vector-tests]") {
	SECTION("Resizing") {
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

	SECTION("Clearing") {
		fastgltf::SmallVector<uint32_t, 4> vec = {1, 2, 3};
		REQUIRE(vec.size() == 3);
		REQUIRE(vec.capacity() == 4);
		vec.clear();
		REQUIRE(vec.empty());
		REQUIRE(vec.capacity() == 4);

		vec.resize(16);
		REQUIRE(vec.size() == 16);
		REQUIRE(vec.capacity() >= 16);
		vec.clear();
		REQUIRE(vec.empty());
		REQUIRE(vec.capacity() >= 16);

		vec.reserve(64);
		REQUIRE(vec.empty());
		REQUIRE(vec.capacity() >= 64);
	}

	SECTION("Pushing and popping") {
		fastgltf::SmallVector<std::string, 2> vec;
		const std::string value = "value";
		vec.push_back(value);
		vec.push_back(std::string("moved"));
		vec.push_back(vec[0]);
		REQUIRE(vec.size() == 3);
		REQUIRE(vec[0] == "value");
		REQUIRE(vec[1] == "moved");
		REQUIRE(vec[2] == "value");

		vec.pop_back();
		REQUIRE(vec.size() == 2);
		REQUIRE(vec.back() == "moved");
		vec.pop_back();
		vec.pop_back();
		REQUIRE(vec.empty());
	}

	SECTION("Growing from an element of the vector itself") {
		const std::string first = "The first string in the vector, which is too long for SSO";
		const std::string second = "The second string in the vector, which is too long for SSO";

		fastgltf::SmallVector<std::string, 2> vec;
		vec.emplace_back(first);
		vec.emplace_back(second);

		vec.emplace_back(vec[0]);
		REQUIRE(vec.size() == 3);
		REQUIRE(vec[2] == first);

		vec.resize(vec.capacity(), vec[1]);
		vec.resize(vec.capacity() + 1, vec[1]);
		for (std::size_t i = 3; i < vec.size(); ++i) {
			REQUIRE(vec[i] == second);
		}

		vec.resize(vec.capacity(), vec[1]);
		const auto size = vec.size();
		vec.emplace_back(vec[0]);
		REQUIRE(vec.size() == size + 1);
		REQUIRE(vec[size] == first);
	}
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

TEST_CASE("Test reusing moved-from SmallVector", "[vector-tests]") {
	auto fillAndCheck = [](fastgltf::SmallVector<uint32_t, 4>& vec) {
		for (uint32_t i = 0; i < 16; ++i) {
			vec.emplace_back(i);
		}
		REQUIRE(vec.size() == 16);
		for (uint32_t i = 0; i < 16; ++i) {
			REQUIRE(vec[i] == i);
		}
	};

	SECTION("Move constructor") {
		fastgltf::SmallVector<uint32_t, 4> source;
		fillAndCheck(source);
		REQUIRE(!source.isUsingStack());

		fastgltf::SmallVector<uint32_t, 4> target(std::move(source));
		REQUIRE(target.size() == 16);
		REQUIRE(source.empty());
		REQUIRE(source.isUsingStack());

		fillAndCheck(source);
	}

	SECTION("Move assignment") {
		fastgltf::SmallVector<uint32_t, 4> source;
		fillAndCheck(source);

		fastgltf::SmallVector<uint32_t, 4> target;
		target = std::move(source);
		REQUIRE(target.size() == 16);
		REQUIRE(source.empty());
		REQUIRE(source.isUsingStack());

		fillAndCheck(source);
	}
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

namespace {
	struct RefCountedObject {
		static inline std::size_t aliveObjects = 0;

		RefCountedObject() {
			++aliveObjects;
		}

		RefCountedObject(const RefCountedObject& other) {
			++aliveObjects;
		}

		// Deliberately not noexcept, so that SmallVector::reserve still uses the copy constructor.
		RefCountedObject(RefCountedObject&& other) {
			++aliveObjects;
		}

		~RefCountedObject() {
			--aliveObjects;
		}
	};
}

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

	// The remaining elements fit into the inline storage again, so the heap allocation has to be freed.
	REQUIRE(!objects.isUsingStack());
	objects.shrink_to_fit();
	REQUIRE(objects.isUsingStack());
	REQUIRE(objects.size() == 4);
	REQUIRE(objects.capacity() == 4);
	REQUIRE(RefCountedObject::aliveObjects == 4);
}

TEST_CASE("Test moving vectors with inline storage", "[vector-tests]") {
	{
		fastgltf::SmallVector<RefCountedObject, 4> source(3);
		REQUIRE(RefCountedObject::aliveObjects == 3);

		fastgltf::SmallVector<RefCountedObject, 4> target(std::move(source));
		REQUIRE(source.empty());
		REQUIRE(target.size() == 3);
		REQUIRE(RefCountedObject::aliveObjects == 3);

		fastgltf::SmallVector<RefCountedObject, 4> assigned(1);
		assigned = std::move(target);
		REQUIRE(target.empty());
		REQUIRE(assigned.size() == 3);
		REQUIRE(RefCountedObject::aliveObjects == 3);
	}
	REQUIRE(RefCountedObject::aliveObjects == 0);
}

TEST_CASE("Test copying vectors", "[vector-tests]") {
	{
		fastgltf::SmallVector<RefCountedObject, 4> inlineSource(3);
		fastgltf::SmallVector<RefCountedObject, 4> heapSource(6);
		REQUIRE(RefCountedObject::aliveObjects == 9);

		fastgltf::SmallVector<RefCountedObject, 4> inlineCopy(inlineSource);
		fastgltf::SmallVector<RefCountedObject, 4> heapCopy(heapSource);
		REQUIRE(inlineCopy.size() == 3);
		REQUIRE(heapCopy.size() == 6);
		REQUIRE(RefCountedObject::aliveObjects == 18);

		const auto capacity = heapCopy.capacity();
		heapCopy = inlineSource;
		REQUIRE(heapCopy.size() == 3);
		REQUIRE(heapCopy.capacity() == capacity);
		REQUIRE(RefCountedObject::aliveObjects == 15);

		inlineCopy = heapSource;
		REQUIRE(inlineCopy.size() == 6);
		REQUIRE(RefCountedObject::aliveObjects == 18);
	}
	REQUIRE(RefCountedObject::aliveObjects == 0);
}

TEST_CASE("Test vectors with polymorphic allocators", "[vector-tests]") {
	fastgltf::pmr::SmallVector<std::uint32_t, 4> ints;
	ints.assign(10, 5);
	REQUIRE(ints.size() == 10);
	REQUIRE(ints.data() != nullptr);
	for (auto& i : ints) {
		REQUIRE(i == 5);
	}

	SECTION("Move assignment with unequal allocators") {
		std::pmr::monotonic_buffer_resource otherResource;
		fastgltf::pmr::SmallVector<std::uint32_t, 4> other(&otherResource);
		other = std::move(ints);

		REQUIRE(ints.empty());
		REQUIRE(other.size() == 10);
		for (auto& i : other) {
			REQUIRE(i == 5);
		}
	}

	SECTION("Move constructor takes over the allocator") {
		// The memory comes from a buffer on the stack, so freeing it with any other allocator would be caught by ASan.
		std::array<std::byte, 256> buffer {};
		std::pmr::monotonic_buffer_resource resource(buffer.data(), buffer.size(), std::pmr::null_memory_resource());
		fastgltf::pmr::SmallVector<std::uint32_t, 4> source(&resource);
		source.assign(10, 5);
		REQUIRE(!source.isUsingStack());

		fastgltf::pmr::SmallVector<std::uint32_t, 4> moved(std::move(source));
		REQUIRE(moved.size() == 10);
		// Growing has to allocate from, and free the old allocation back to, the same resource.
		REQUIRE(moved.capacity() < 17);
		moved.resize(17, 6);
		REQUIRE(moved.back() == 6);
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

namespace {
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
}

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

	SECTION("Heap storage move assignment into heap storage") {
		fastgltf::SmallVector<MoveOnlyObject, 2> vec;
		vec.emplace_back(1);
		vec.emplace_back(2);
		vec.emplace_back(3);

		fastgltf::SmallVector<MoveOnlyObject, 2> vec2;
		vec2.emplace_back(97);
		vec2.emplace_back(98);
		vec2.emplace_back(99);
		vec2 = std::move(vec);

		REQUIRE(vec.empty());
		REQUIRE(vec2.size() == 3);
		REQUIRE(*vec2[0].ptr == 1);
		REQUIRE(*vec2[2].ptr == 3);
	}

	SECTION("Stack storage move assignment into heap storage") {
		fastgltf::SmallVector<MoveOnlyObject, 2> vec;
		vec.emplace_back(1);

		fastgltf::SmallVector<MoveOnlyObject, 2> vec2;
		vec2.emplace_back(97);
		vec2.emplace_back(98);
		vec2.emplace_back(99);
		vec2 = std::move(vec);

		REQUIRE(vec.empty());
		REQUIRE(vec.isUsingStack());
		REQUIRE(vec2.size() == 1);
		REQUIRE(*vec2[0].ptr == 1);

		REQUIRE(!vec2.isUsingStack());
		REQUIRE(vec2.capacity() >= 3);

		vec2.emplace_back(2);
		vec2.emplace_back(3);
		vec2.emplace_back(4);
		REQUIRE(vec2.size() == 4);
		REQUIRE(*vec2[3].ptr == 4);
	}

	SECTION("Move assignment from an empty vector") {
		fastgltf::SmallVector<MoveOnlyObject, 2> vec;

		fastgltf::SmallVector<MoveOnlyObject, 2> vec2;
		vec2.emplace_back(98);
		vec2.emplace_back(99);
		vec2 = std::move(vec);
		REQUIRE(vec2.empty());

		fastgltf::SmallVector<MoveOnlyObject, 2> vec3;
		vec3.emplace_back(97);
		vec3.emplace_back(98);
		vec3.emplace_back(99);
		vec3 = std::move(vec);
		REQUIRE(vec3.empty());
	}

	SECTION("Shrinking") {
		fastgltf::SmallVector<MoveOnlyObject, 2> vec;
		vec.emplace_back(1);
		vec.emplace_back(2);
		vec.emplace_back(3);
		vec.reserve(16);

		vec.shrink_to_fit();
		REQUIRE(vec.capacity() == 3);
		REQUIRE(*vec[2].ptr == 3);

		vec.resize(2);
		vec.shrink_to_fit();
		REQUIRE(vec.isUsingStack());
		REQUIRE(*vec[0].ptr == 1);
		REQUIRE(*vec[1].ptr == 2);
	}
}

namespace {
	template <typename T>
	fastgltf::StaticVector<T> makeStaticVector(std::initializer_list<T> values) {
		fastgltf::StaticVector<T> vector(values.size());
		std::size_t i = 0;
		for (const auto& value : values)
			vector[i++] = value;
		return vector;
	}

	// type that only implements operator< to test the weak_ordering fallback for the operator<=> of StaticVector
	struct LessOnly {
		int value;
		friend bool operator<(const LessOnly& a, const LessOnly& b) { return a.value < b.value; }
	};
} // namespace

TEST_CASE("Test StaticVector three-way comparison", "[vector-tests]") {
	const auto a = makeStaticVector({1, 2, 3});
	const auto b = makeStaticVector({1, 2, 3});
	const auto c = makeStaticVector({1, 2, 4});
	const auto prefix = makeStaticVector({1, 2});

	REQUIRE(((a <=> b) == 0));
	REQUIRE(((a <=> c) < 0));
	REQUIRE(((c <=> a) > 0));
	REQUIRE(((prefix <=> a) < 0));
	REQUIRE(a < c);
	REQUIRE(c >= a);
	REQUIRE(prefix <= a);

	static_assert(std::is_same_v<decltype(a <=> b), std::strong_ordering>);

	SECTION("Against std::vector") {
		const std::vector<int> equal = {1, 2, 3};
		const std::vector<int> greater = {1, 3};

		REQUIRE(((a <=> equal) == 0));
		REQUIRE(((a <=> greater) < 0));
		REQUIRE(a < greater);
		REQUIRE(greater > a);
	}

	SECTION("Floating point") {
		const auto f1 = makeStaticVector({1.0f, 2.0f});
		const auto f2 = makeStaticVector({1.0f, 2.5f});

		static_assert(std::is_same_v<decltype(f1 <=> f2), std::partial_ordering>);
		REQUIRE(((f1 <=> f2) < 0));
		REQUIRE(((f1 <=> f1) == 0));
	}

	SECTION("Types without operator<=>") {
		const auto l1 = makeStaticVector({LessOnly {1}, LessOnly {2}});
		const auto l2 = makeStaticVector({LessOnly {1}, LessOnly {3}});

		static_assert(std::is_same_v<decltype(l1 <=> l2), std::weak_ordering>);
		REQUIRE(((l1 <=> l2) < 0));
		REQUIRE(((l2 <=> l1) > 0));
		REQUIRE(((l1 <=> l1) == 0));
	}
}
