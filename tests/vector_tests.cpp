#include <memory_resource>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include <fastgltf/containers/small_vector.hpp>
#include <fastgltf/containers/static_vector.hpp>

TEST_CASE("Test resizing and allocation behaviour", "[small-vector]") {
	SECTION("Resizing") {
		fastgltf::small_vector<std::uint32_t, 4> vec = {1, 2, 3};
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
		fastgltf::small_vector<std::uint32_t, 4> vec = {1, 2, 3};
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
		fastgltf::small_vector<std::string, 2> vec;
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

		fastgltf::small_vector<std::string, 2> vec;
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

namespace {
	struct CopyCounter {
		static inline std::size_t constructions = 0, assignments = 0, destructions = 0;
		static void reset() { constructions = assignments = destructions = 0; }

		int value = 0;

		CopyCounter(int v) : value(v) {}
		CopyCounter(const CopyCounter& other) : value(other.value) { ++constructions; }
		CopyCounter& operator=(const CopyCounter& other) { value = other.value; ++assignments; return *this; }
		~CopyCounter() { ++destructions; }
	};
}

TEST_CASE("Test constructors", "[small-vector]") {
	SECTION("Constructors") {
		fastgltf::small_vector<std::uint32_t, 4> vec = {0, 1, 2, 3};
		for (std::uint32_t i = 0; i < vec.size(); ++i) {
			REQUIRE(vec[i] == i);
		}

		fastgltf::small_vector<std::uint32_t, 4> vec2(vec);
		REQUIRE(vec2.size() == 4);
		for (std::uint32_t i = 0; i < vec2.size(); ++i) {
			REQUIRE(vec2[i] == i);
		}

		fastgltf::small_vector<std::uint32_t, 4> vec3 = std::move(vec2);
		REQUIRE(vec2.empty());
		REQUIRE(vec3.size() == 4);
		vec3.resize(6);
		REQUIRE(vec3.size() == 6);
		for (std::uint32_t i = 0; i < 4; ++i) {
			REQUIRE(vec3[i] == i);
		}
		REQUIRE(vec3[4] == 0);
		REQUIRE(vec3[5] == 0);

		SECTION("Copy assignment reuses existing elements") {
			SECTION("Larger vector") {
				fastgltf::small_vector<CopyCounter, 4> vec1 { 1, 2, 3, 4 };
				fastgltf::small_vector<CopyCounter, 4> vec2 { 5, 6 };
				CopyCounter::reset();

				vec1 = vec2;
				REQUIRE(CopyCounter::assignments == 2);   // the first two elements were assigned over
				REQUIRE(CopyCounter::constructions == 0); // nothing was newly constructed
				REQUIRE(CopyCounter::destructions == 2);  // the extra two were destroyed
				REQUIRE(vec1.size() == 2);
				REQUIRE((vec1[0].value == 5 && vec1[1].value == 6));
			}
			SECTION("Into a smaller vector") {
				fastgltf::small_vector<CopyCounter, 4> vec1 { 1, 2 };
				fastgltf::small_vector<CopyCounter, 4> vec2 { 3, 4, 5 };
				CopyCounter::reset();

				vec1 = vec2;
				REQUIRE(CopyCounter::assignments == 2);
				REQUIRE(CopyCounter::constructions == 1); // only the new tail element is constructed
				REQUIRE(CopyCounter::destructions == 0);
				REQUIRE(vec1.size() == 3);
				REQUIRE((vec1[0].value == 3 && vec1[1].value == 4 && vec1[2].value == 5));
			}
		}

		SECTION("Copy assignment into itself") {
			fastgltf::small_vector<std::uint32_t, 4> vec { 0, 1, 2, 3 };
			auto& self = vec; vec = self;
			REQUIRE(vec.size() == 4);
			for (std::uint32_t i = 0; i < vec.size(); ++i) {
				REQUIRE(vec[i] == i);
			}
		}
	}

	SECTION("Assigning values") {
		SECTION("Assigning to an empty vector within SVO") {
			fastgltf::small_vector<std::uint32_t, 4> vec;
			vec.assign(3, 25);
			REQUIRE(vec.size() == 3);

			std::size_t count = 0;
			for (auto& element : vec) {
				REQUIRE(element == 25);
				++count;
			}
			REQUIRE(count == 3);
		}

		SECTION("Assigning to an empty vector exceeding SVO") {
			fastgltf::small_vector<std::uint32_t, 4> vec;
			vec.assign(5, 25);
			REQUIRE(vec.size() == 5);

			std::size_t count = 0;
			for (auto& element : vec) {
				REQUIRE(element == 25);
				++count;
			}
			REQUIRE(count == 5);
		}

		SECTION("Assigning to an equally sized vector") {
			fastgltf::small_vector<std::uint32_t, 4> vec { 0, 1 };
			REQUIRE(vec.size() == 2);
			vec.assign(2, 25);
			REQUIRE(vec.size() == 2);
			REQUIRE(vec.is_using_stack());

			std::size_t count = 0;
			for (auto& element : vec) {
				REQUIRE(element == 25);
				++count;
			}
			REQUIRE(count == 2);
		}

		SECTION("Assigning to a smaller vector within SVO") {
			fastgltf::small_vector<std::uint32_t, 6> vec { 0, 1, 2 };
			REQUIRE(vec.size() == 3);
			vec.assign(5, 25);
			REQUIRE(vec.size() == 5);
			REQUIRE(vec.is_using_stack());

			std::size_t count = 0;
			for (auto& element : vec) {
				REQUIRE(element == 25);
				++count;
			}
			REQUIRE(count == 5);
		}

		SECTION("Assigning to a smaller vector exceeding SVO") {
			fastgltf::small_vector<std::uint32_t, 4> vec { 0, 1, 2 };
			REQUIRE(vec.size() == 3);
			vec.assign(5, 25);
			REQUIRE(vec.size() == 5);
			REQUIRE(!vec.is_using_stack());

			std::size_t count = 0;
			for (auto& element : vec) {
				REQUIRE(element == 25);
				++count;
			}
			REQUIRE(count == 5);
		}

		SECTION("Assigning to a larger vector within SVO") {
			fastgltf::small_vector<std::uint32_t, 4> vec { 0, 1, 2 };
			REQUIRE(vec.size() == 3);
			vec.assign(2, 25);
			REQUIRE(vec.size() == 2);
			REQUIRE(vec.is_using_stack());

			std::size_t count = 0;
			for (auto& element : vec) {
				REQUIRE(element == 25);
				++count;
			}
			REQUIRE(count == 2);
		}

		SECTION("Assigning to a larger vector exceeding SVO") {
			fastgltf::small_vector<std::uint32_t, 2> vec { 0, 1, 2 };
			REQUIRE(vec.size() == 3);
			vec.assign(3, 25);
			REQUIRE(vec.size() == 3);
			REQUIRE(!vec.is_using_stack());

			std::size_t count = 0;
			for (auto& element : vec) {
				REQUIRE(element == 25);
				++count;
			}
			REQUIRE(count == 3);
		}

		SECTION("Assigning a value from itself") {
			fastgltf::small_vector<std::string, 4> vec { "0", "1", "2", "3" };
			REQUIRE(vec.size() == 4);
			vec.assign(2, vec[2]);
			REQUIRE(vec.size() == 2);
			for (auto& element : vec) {
				REQUIRE(element == "2");
			}
		}
	}

	SECTION("Assigning via initializer_list") {
		fastgltf::small_vector<std::uint32_t, 4> vec;
		vec.assign({ 0, 1, 2, 3, 4 });
		REQUIRE(vec.size() == 5);
		std::size_t count = 0;
		for (auto& element : vec) {
			REQUIRE(element == count++);
		}
		REQUIRE(count == 5);
	}
}

TEST_CASE("Test reusing moved-from small_vector", "[small-vector]") {
	auto fillAndCheck = [](fastgltf::small_vector<std::uint32_t, 4>& vec) {
		for (uint32_t i = 0; i < 16; ++i) {
			vec.emplace_back(i);
		}
		REQUIRE(vec.size() == 16);
		for (uint32_t i = 0; i < 16; ++i) {
			REQUIRE(vec[i] == i);
		}
	};

	SECTION("Move constructor") {
		fastgltf::small_vector<std::uint32_t, 4> source;
		fillAndCheck(source);
		REQUIRE(!source.is_using_stack());

		fastgltf::small_vector<std::uint32_t, 4> target(std::move(source));
		REQUIRE(target.size() == 16);
		REQUIRE(source.empty());
		REQUIRE(source.is_using_stack());

		fillAndCheck(source);
	}

	SECTION("Move assignment") {
		fastgltf::small_vector<std::uint32_t, 4> source;
		fillAndCheck(source);

		fastgltf::small_vector<std::uint32_t, 4> target;
		target = std::move(source);
		REQUIRE(target.size() == 16);
		REQUIRE(source.empty());
		REQUIRE(source.is_using_stack());

		fillAndCheck(source);
	}
}

TEST_CASE("Nested small_vector", "[small-vector]") {
	fastgltf::small_vector<fastgltf::small_vector<std::uint32_t, 2>, 4> vectors(6, {4}); // This should heap allocate straight away.
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

		// Deliberately not noexcept, so that small_vector::reserve still uses the copy constructor.
		RefCountedObject(RefCountedObject&& other) {
			++aliveObjects;
		}

		RefCountedObject& operator=(const RefCountedObject& other) {
			return *this;
		}

		~RefCountedObject() {
			--aliveObjects;
		}
	};
}

TEST_CASE("Test shrinking vectors", "[small-vector]") {
	fastgltf::small_vector<RefCountedObject, 4> objects;
	for (std::size_t i = 0; i < 4; ++i) {
		objects.emplace_back();
	}
	REQUIRE(RefCountedObject::aliveObjects == 4);
	objects.emplace_back();
	REQUIRE(RefCountedObject::aliveObjects == 5);
	objects.resize(4);
	REQUIRE(RefCountedObject::aliveObjects == 4);

	// The remaining elements fit into the inline storage again, so the heap allocation has to be freed.
	REQUIRE(!objects.is_using_stack());
	objects.shrink_to_fit();
	REQUIRE(objects.is_using_stack());
	REQUIRE(objects.size() == 4);
	REQUIRE(objects.capacity() == 4);
	REQUIRE(RefCountedObject::aliveObjects == 4);
}

TEST_CASE("Test moving vectors with inline storage", "[small-vector]") {
	{
		fastgltf::small_vector<RefCountedObject, 4> source(3);
		REQUIRE(RefCountedObject::aliveObjects == 3);

		fastgltf::small_vector<RefCountedObject, 4> target(std::move(source));
		REQUIRE(source.empty());
		REQUIRE(target.size() == 3);
		REQUIRE(RefCountedObject::aliveObjects == 3);

		fastgltf::small_vector<RefCountedObject, 4> assigned(1);
		assigned = std::move(target);
		REQUIRE(target.empty());
		REQUIRE(assigned.size() == 3);
		REQUIRE(RefCountedObject::aliveObjects == 3);
	}
	REQUIRE(RefCountedObject::aliveObjects == 0);
}

TEST_CASE("Test copying vectors", "[small-vector]") {
	{
		fastgltf::small_vector<RefCountedObject, 4> inlineSource(3);
		fastgltf::small_vector<RefCountedObject, 4> heapSource(6);
		REQUIRE(RefCountedObject::aliveObjects == 9);

		fastgltf::small_vector<RefCountedObject, 4> inlineCopy(inlineSource);
		fastgltf::small_vector<RefCountedObject, 4> heapCopy(heapSource);
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

TEST_CASE("Test vectors with polymorphic allocators", "[small-vector]") {
	fastgltf::pmr::small_vector<std::uint32_t, 4> ints;
	ints.assign(10, 5);
	REQUIRE(ints.size() == 10);
	REQUIRE(ints.data() != nullptr);
	for (auto& i : ints) {
		REQUIRE(i == 5);
	}

	SECTION("Move assignment with unequal allocators") {
		std::pmr::monotonic_buffer_resource resource;
		fastgltf::pmr::small_vector<std::uint32_t, 4> other(&resource);
		other = std::move(ints);
		REQUIRE(other.get_allocator() == &resource);

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
		fastgltf::pmr::small_vector<std::uint32_t, 4> source(&resource);
		source.assign(10, 5);
		REQUIRE(!source.is_using_stack());

		fastgltf::pmr::small_vector<std::uint32_t, 4> moved(std::move(source));
		REQUIRE(moved.size() == 10);
		REQUIRE(moved.get_allocator() == &resource);

		// Growing has to allocate from, and free the old allocation back to, the same resource.
		REQUIRE(moved.capacity() < 17);
		moved.resize(17, 6);
		REQUIRE(moved.back() == 6);
	}

	SECTION("Nested vectors") {
		SECTION("Resize") {
			std::pmr::monotonic_buffer_resource resource;
			fastgltf::pmr::small_vector<fastgltf::pmr::small_vector<std::string, 4>, 4> vecs(&resource);
			REQUIRE(vecs.get_allocator() == &resource);

			vecs.resize(5);
			for (auto& vec : vecs) {
				REQUIRE(vec.get_allocator() == &resource);
			}
		}

		SECTION("emplace_back()") {
			std::pmr::monotonic_buffer_resource resource;
			fastgltf::pmr::small_vector<fastgltf::pmr::small_vector<std::string, 4>, 4> vecs(&resource);
			REQUIRE(vecs.get_allocator() == &resource);

			for (std::size_t i = 0; i < 2; ++i)
				vecs.emplace_back();
			for (auto& vec : vecs) {
				REQUIRE(vec.get_allocator() == &resource);
			}
		}

		SECTION("Constructors") {
			std::pmr::monotonic_buffer_resource resource;
			fastgltf::pmr::small_vector<fastgltf::pmr::small_vector<std::string, 4>, 4> vecs(2, &resource);
			REQUIRE(vecs.get_allocator() == &resource);
			for (auto& vec : vecs) {
				REQUIRE(vec.get_allocator() == &resource);
			}

			fastgltf::pmr::small_vector default_copy(vecs);
			REQUIRE(default_copy.get_allocator() != &resource);
			REQUIRE(default_copy.get_allocator() == std::pmr::get_default_resource());
			for (auto& vec : default_copy) {
				REQUIRE(vec.get_allocator() != &resource);
				REQUIRE(vec.get_allocator() == std::pmr::get_default_resource());
			}

			decltype(vecs) resource_copy(vecs, &resource);
			REQUIRE(resource_copy.get_allocator() == &resource);
			for (auto& vec : resource_copy) {
				REQUIRE(vec.get_allocator() == &resource);
			}

			fastgltf::pmr::small_vector moved(std::move(resource_copy));
			REQUIRE(moved.get_allocator() == &resource);
			for (auto& vec : resource_copy) {
				REQUIRE(vec.get_allocator() == &resource);
			}
		}
	}
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

TEST_CASE("Test move-only types with small_vector", "[small-vector]") {
	SECTION("Stack storage move constructor") {
		fastgltf::small_vector<MoveOnlyObject, 4> vec;
		vec.emplace_back(10);
		vec.emplace_back(20);
		vec.emplace_back(30);

		fastgltf::small_vector<MoveOnlyObject, 4> vec2 = std::move(vec);
		REQUIRE(vec.empty());
		REQUIRE(vec2.size() == 3);
		REQUIRE(*vec2[0].ptr == 10);
		REQUIRE(*vec2[1].ptr == 20);
		REQUIRE(*vec2[2].ptr == 30);
	}

	SECTION("Stack storage move assignment") {
		fastgltf::small_vector<MoveOnlyObject, 4> vec;
		vec.emplace_back(10);
		vec.emplace_back(20);

		fastgltf::small_vector<MoveOnlyObject, 4> vec2;
		vec2.emplace_back(100);
		vec2 = std::move(vec);

		REQUIRE(vec.empty());
		REQUIRE(vec2.size() == 2);
		REQUIRE(*vec2[0].ptr == 10);
		REQUIRE(*vec2[1].ptr == 20);
	}

	SECTION("Heap storage move constructor") {
		fastgltf::small_vector<MoveOnlyObject, 2> vec;
		vec.emplace_back(1);
		vec.emplace_back(2);
		vec.emplace_back(3);

		REQUIRE(!vec.is_using_stack());
		fastgltf::small_vector<MoveOnlyObject, 2> vec2 = std::move(vec);
		REQUIRE(vec.empty());
		REQUIRE(vec2.size() == 3);
		REQUIRE(*vec2[0].ptr == 1);
		REQUIRE(*vec2[1].ptr == 2);
		REQUIRE(*vec2[2].ptr == 3);
	}

	SECTION("Heap storage move assignment") {
		fastgltf::small_vector<MoveOnlyObject, 2> vec;
		vec.emplace_back(1);
		vec.emplace_back(2);
		vec.emplace_back(3);

		fastgltf::small_vector<MoveOnlyObject, 2> vec2;
		vec2.emplace_back(99);
		vec2 = std::move(vec);

		REQUIRE(vec.empty());
		REQUIRE(vec2.size() == 3);
		REQUIRE(*vec2[0].ptr == 1);
		REQUIRE(*vec2[1].ptr == 2);
		REQUIRE(*vec2[2].ptr == 3);
	}

	SECTION("Heap storage move assignment into heap storage") {
		fastgltf::small_vector<MoveOnlyObject, 2> vec;
		vec.emplace_back(1);
		vec.emplace_back(2);
		vec.emplace_back(3);

		fastgltf::small_vector<MoveOnlyObject, 2> vec2;
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
		fastgltf::small_vector<MoveOnlyObject, 2> vec;
		vec.emplace_back(1);

		fastgltf::small_vector<MoveOnlyObject, 2> vec2;
		vec2.emplace_back(97);
		vec2.emplace_back(98);
		vec2.emplace_back(99);
		vec2 = std::move(vec);

		REQUIRE(vec.empty());
		REQUIRE(vec.is_using_stack());
		REQUIRE(vec2.size() == 1);
		REQUIRE(*vec2[0].ptr == 1);

		REQUIRE(!vec2.is_using_stack());
		REQUIRE(vec2.capacity() >= 3);

		vec2.emplace_back(2);
		vec2.emplace_back(3);
		vec2.emplace_back(4);
		REQUIRE(vec2.size() == 4);
		REQUIRE(*vec2[3].ptr == 4);
	}

	SECTION("Move assignment from an empty vector") {
		fastgltf::small_vector<MoveOnlyObject, 2> vec;

		fastgltf::small_vector<MoveOnlyObject, 2> vec2;
		vec2.emplace_back(98);
		vec2.emplace_back(99);
		vec2 = std::move(vec);
		REQUIRE(vec2.empty());

		fastgltf::small_vector<MoveOnlyObject, 2> vec3;
		vec3.emplace_back(97);
		vec3.emplace_back(98);
		vec3.emplace_back(99);
		vec3 = std::move(vec);
		REQUIRE(vec3.empty());
	}

	SECTION("Shrinking") {
		fastgltf::small_vector<MoveOnlyObject, 2> vec;
		vec.emplace_back(1);
		vec.emplace_back(2);
		vec.emplace_back(3);
		vec.reserve(16);

		vec.shrink_to_fit();
		REQUIRE(vec.capacity() == 3);
		REQUIRE(*vec[2].ptr == 3);

		vec.resize(2);
		vec.shrink_to_fit();
		REQUIRE(vec.is_using_stack());
		REQUIRE(*vec[0].ptr == 1);
		REQUIRE(*vec[1].ptr == 2);
	}
}

#if __cpp_exceptions
namespace {
	struct test_exception {};

	/**
	 * helper object that will throw after the copies_until_throw counter has reached zero to test
	 * the exception handling.
	 */
	struct throw_on_copy {
		static constexpr std::uint32_t live_marker = 0xDEADBEEF;

		static inline std::size_t alive = 0;
		static inline std::size_t destruct_errors = 0;
		static inline std::size_t copies_until_throw = 0;

		static void arm(const std::size_t i) {
			copies_until_throw = i;
		}
		static void disarm() {
			copies_until_throw = 0;
		}
		static void reset() {
			alive = 0;
			destruct_errors = 0;
			disarm();
		}

	private:
		static void maybe_throw() {
			if (copies_until_throw != 0 && (--copies_until_throw) == 0)
				throw test_exception {};
		}

	public:
		int value = 0;
		std::uint32_t marker = live_marker;

		throw_on_copy(const int v = 0) noexcept : value(v) {
			++alive;
		}
		throw_on_copy(const throw_on_copy& other) : value(other.value) {
			maybe_throw();
			++alive;
		}
		throw_on_copy& operator=(const throw_on_copy& other) {
			maybe_throw();
			value = other.value;
			return *this;
		}
		~throw_on_copy() {
			if (marker != live_marker)
				++destruct_errors;
			marker = 0;
			--alive;
		}
	};
}

TEST_CASE("small_vector exception behavior", "[small-vector]") {
	using vec_t = fastgltf::small_vector<throw_on_copy, 2>;
	throw_on_copy::reset();

	SECTION("Copy constructors") {
		for (const int count : { 2, 4 }) {
			for (std::size_t i = 1; i <= static_cast<std::size_t>(count); ++i) {
				vec_t vec;
				for (int j = 0; j < count; ++j)
					vec.emplace_back(j);

				// make the copy constructor copy all elements but throw at the last element
				throw_on_copy::arm(i);
				REQUIRE_THROWS_AS(vec_t(vec), test_exception);
				throw_on_copy::disarm();

				// validate that all temporary copies have been destroyed again
				REQUIRE(throw_on_copy::alive == vec.size());
				REQUIRE(throw_on_copy::destruct_errors == 0);
				REQUIRE(vec.size() == static_cast<std::size_t>(count));
				for (int j = 0; j < count; ++j) {
					REQUIRE(vec[j].value == j);
					REQUIRE(vec[j].marker == throw_on_copy::live_marker);
				}
			}
		}
	}

	SECTION("Copy assignment") {
		for (const auto& [target_count, source_count]
			: { std::pair { 4, 2 }, std::pair { 2, 4 }, std::pair { 1, 6 } }) {

			for (int i = 0; i < source_count; ++i) {
				vec_t source;
				for (int j = 0; j < source_count; ++j)
					source.emplace_back(j);

				vec_t target;
				for (int j = 0; j < target_count; ++j)
					target.emplace_back(j);

				// copy source into target and throw on the last copy
				throw_on_copy::arm(i);
				try {
					target = source;
				} catch (const test_exception&) {}
				throw_on_copy::disarm();

				// all original objects should still exist
				REQUIRE(throw_on_copy::alive == target.size() + source.size());
				REQUIRE(throw_on_copy::destruct_errors == 0);
				for (const auto& element : target)
					REQUIRE(element.marker == throw_on_copy::live_marker);

				// validate that the copied to vector still works
				target.emplace_back(7);
				REQUIRE(target.back().value == 7);
			}
		}
	}
}
#endif

#if FASTGLTF_HAS_CONTAINERS_RANGES
TEST_CASE("small_vector containers ranges compatibility", "[small-vector]") {
	static_assert(std::ranges::range<fastgltf::small_vector<std::uint32_t, 4>>, "small_vector must satisfy range");

	SECTION("Constructors") {
		SECTION("Initialise stack data from range") {
			std::array data = { 0, 1, 2 };
			fastgltf::small_vector<std::uint32_t, 4> vec(std::from_range, data);
			REQUIRE(vec.size() == 3);
			REQUIRE(vec.is_using_stack());
			REQUIRE(vec.capacity() >= 3);
			for (std::uint32_t i = 0; auto& element : vec) {
				REQUIRE(element == i++);
			}
		}

		SECTION("Initialise heap data from range") {
			std::array data = { 0, 1, 2, 3, 4, 5 };
			fastgltf::small_vector<std::uint32_t, 4> vec(std::from_range, data);
			REQUIRE(vec.size() == 6);
			REQUIRE(!vec.is_using_stack());
			REQUIRE(vec.capacity() >= 6);
			for (std::uint32_t i = 0; auto& element : vec) {
				REQUIRE(element == i++);
			}
		}
	}

	SECTION("assign_range") {
		SECTION("Assign range to empty vector within SVO") {
			std::array data = { 0u, 1u, 2u }; // TODO: Not sure if this is right that we require unsigned, but the constructor is fine???
			fastgltf::small_vector<std::uint32_t, 4> vec;
			REQUIRE(vec.empty());

			vec.assign_range(data);

			REQUIRE(vec.size() == 3);
			REQUIRE(vec.is_using_stack());
			REQUIRE(vec.capacity() >= 3);
			for (std::uint32_t i = 0; auto& element : vec) {
				REQUIRE(element == i++);
			}
		}

		SECTION("Assign range to empty vector exceeding SVO") {
			std::array data = { 0u, 1u, 2u, 3u, 4u, 5u }; // TODO: Not sure if this is right that we require unsigned, but the constructor is fine???
			fastgltf::small_vector<std::uint32_t, 4> vec;
			REQUIRE(vec.empty());

			vec.assign_range(data);

			REQUIRE(vec.size() == 6);
			REQUIRE(!vec.is_using_stack());
			REQUIRE(vec.capacity() >= 6);
			for (std::uint32_t i = 0; auto& element : vec) {
				REQUIRE(element == i++);
			}
		}

		SECTION("Assign range to larger vector within SVO") {
			std::array data = { 0u, 1u };
			fastgltf::small_vector<std::uint32_t, 4> vec { 0, 1, 2 };
			REQUIRE(vec.size() == 3);
			REQUIRE(vec.is_using_stack());

			vec.assign_range(data);

			REQUIRE(vec.size() == 2);
			REQUIRE(vec.is_using_stack());
			REQUIRE(vec.capacity() >= 2);
			for (std::uint32_t i = 0; auto& element : vec) {
				REQUIRE(element == i++);
			}
		}

		SECTION("Assign range to larger vector exceeding SVO") {
			std::array data = { 0u, 1u, 2u };
			fastgltf::small_vector<std::uint32_t, 4> vec { 0, 1, 2, 3, 4 };
			REQUIRE(vec.size() == 5);
			REQUIRE(!vec.is_using_stack());

			vec.assign_range(data);

			REQUIRE(vec.size() == 3);

			// Require that the vector does not relocate back onto the stack without shrink_to_fit
			REQUIRE(!vec.is_using_stack());
			REQUIRE(vec.capacity() >= 5);

			vec.shrink_to_fit();
			REQUIRE(vec.is_using_stack());

			for (std::uint32_t i = 0; auto& element : vec) {
				REQUIRE(element == i++);
			}
		}

		SECTION("Verify assign_range does not reduce capacity") {
			fastgltf::small_vector<std::uint32_t, 4> vec;
			REQUIRE(vec.empty());

			vec.assign({ 0u, 1u, 2u, 3u, 4u, 5u });
			REQUIRE(vec.size() == 6);
			REQUIRE(vec.capacity() >= 6);
			REQUIRE(!vec.is_using_stack());

			const auto capacity = vec.capacity();
			std::array data = { 0u, 1u };
			vec.assign_range(data);
			REQUIRE(vec.size() == 2);
			REQUIRE(vec.capacity() == capacity);
			REQUIRE(!vec.is_using_stack());
		}
	}

	SECTION("append_range") {
		SECTION("Append from range to empty vector within SVO") {
			std::array data = { 0u, 1u, 2u };
			fastgltf::small_vector<std::uint32_t, 4> vec;
			REQUIRE(vec.empty());

			vec.append_range(data);

			REQUIRE(vec.size() == 3);
			REQUIRE(vec.is_using_stack());
			REQUIRE(vec.capacity() >= 3);
			for (std::uint32_t i = 0; auto& element : vec) {
				REQUIRE(element == i++);
			}
		}

		SECTION("Append from range to empty vector exceeding SVO") {
			std::array data = { 0u, 1u, 2u };
			fastgltf::small_vector<std::uint32_t, 2> vec;
			REQUIRE(vec.empty());

			vec.append_range(data);

			REQUIRE(vec.size() == 3);
			REQUIRE(!vec.is_using_stack());
			REQUIRE(vec.capacity() >= 3);
			for (std::uint32_t i = 0; auto& element : vec) {
				REQUIRE(element == i++);
			}
		}

		SECTION("Append from range via append_range within SVO") {
			std::array data = { 2u, 3u };
			fastgltf::small_vector<std::uint32_t, 5> vec = { 0, 1 };
			REQUIRE(vec.size() == 2);
			REQUIRE(vec.is_using_stack());

			vec.append_range(data);

			REQUIRE(vec.size() == 4);
			REQUIRE(vec.is_using_stack());
			REQUIRE(vec.capacity() >= 4);
			for (std::uint32_t i = 0; auto& element : vec) {
				REQUIRE(element == i++);
			}
		}

		SECTION("Append from range via append_range exceeding SVO") {
			std::array data = { 2u, 3u };
			fastgltf::small_vector<std::uint32_t, 3> vec = { 0, 1 };
			REQUIRE(vec.size() == 2);
			REQUIRE(vec.is_using_stack());

			vec.append_range(data);

			REQUIRE(vec.size() == 4);
			REQUIRE(!vec.is_using_stack());
			REQUIRE(vec.capacity() >= 4);
			for (std::uint32_t i = 0; auto& element : vec) {
				REQUIRE(element == i++);
			}
		}

		SECTION("Append from range via append_range to heap data") {
			std::array data = { 4u, 5u };
			fastgltf::small_vector<std::uint32_t, 3> vec = { 0, 1, 2, 3 };
			REQUIRE(vec.size() == 4);
			REQUIRE(!vec.is_using_stack());

			vec.append_range(data);

			REQUIRE(vec.size() == 6);
			REQUIRE(!vec.is_using_stack());
			REQUIRE(vec.capacity() >= 6);
			for (std::uint32_t i = 0; auto& element : vec) {
				REQUIRE(element == i++);
			}
		}
	}
}
#endif

TEST_CASE("Test static_vector constructors", "[static-vector]") {
	SECTION("Basic") {
		fastgltf::static_vector<std::uint32_t> vector(10);
		REQUIRE(vector.size() == 10);
		for (std::uint32_t i = 0; i < vector.size(); ++i) {
			vector[i] = i;
		}

		for (std::uint32_t i = 0; auto& element : vector) {
			REQUIRE(element == i++);
		}
	}

	SECTION("Zero size") {
		fastgltf::static_vector<std::uint32_t> vector(0);
		REQUIRE(vector.empty());
		REQUIRE(vector.data() == nullptr);
	}

	SECTION("Initial value") {
		fastgltf::static_vector<std::uint32_t> vector(10, 25);
		std::size_t count = 0;
		for (auto& element : vector) {
			REQUIRE(element == 25);
			++count;
		}
		REQUIRE(count == 10);
	}

	SECTION("Copy vector") {
		fastgltf::static_vector<std::uint32_t> vector(10, 25);
		fastgltf::static_vector<std::uint32_t> copy(vector);

		REQUIRE(copy.size() == 10);
		for (auto& element : copy) {
			REQUIRE(element == 25);
		}
	}

	SECTION("Move vector") {
		fastgltf::static_vector<std::uint32_t> vector(10, 25);
		fastgltf::static_vector<std::uint32_t> moved(std::move(vector));
		REQUIRE(vector.empty());
		REQUIRE(vector.data() == nullptr);

		REQUIRE(moved.size() == 10);
		for (auto& element : moved) {
			REQUIRE(element == 25);
		}
	}

	SECTION("Copy into existing larger vector") {
		fastgltf::static_vector<std::uint32_t> vec1(20, 1);
		fastgltf::static_vector<std::uint32_t> vec2(10, 2);

		vec1 = vec2;
		REQUIRE(vec2.size() == 10);
		REQUIRE(vec1.size() == 10);
		for (auto& element : vec1) {
			REQUIRE(element == 2);
		}
	}

	SECTION("Copy into existing smaller vector") {
		fastgltf::static_vector<std::uint32_t> vec1(10, 1);
		fastgltf::static_vector<std::uint32_t> vec2(20, 2);

		vec1 = vec2;
		REQUIRE(vec2.size() == 20);
		REQUIRE(vec1.size() == 20);
		for (auto& element : vec1) {
			REQUIRE(element == 2);
		}
	}

	SECTION("Copy empty vector") {
		fastgltf::static_vector<std::uint32_t> vec1(10, 1);
		fastgltf::static_vector<std::uint32_t> vec2(0);

		vec1 = vec2;
		REQUIRE(vec2.empty());
		REQUIRE(vec1.empty());
		REQUIRE(vec1.data() == nullptr);
	}

	SECTION("Move into existing larger vector") {
		fastgltf::static_vector<std::uint32_t> vec1(20, 1);
		fastgltf::static_vector<std::uint32_t> vec2(10, 2);

		vec1 = std::move(vec2);
		REQUIRE(vec2.empty());
		REQUIRE(vec2.data() == nullptr);
		REQUIRE(vec1.size() == 10);
		for (auto& element : vec1) {
			REQUIRE(element == 2);
		}
	}

	SECTION("Move into existing smaller vector") {
		fastgltf::static_vector<std::uint32_t> vec1(10, 1);
		fastgltf::static_vector<std::uint32_t> vec2(20, 2);

		vec1 = std::move(vec2);
		REQUIRE(vec2.empty());
		REQUIRE(vec2.data() == nullptr);
		REQUIRE(vec1.size() == 20);
		for (auto& element : vec1) {
			REQUIRE(element == 2);
		}
	}

	SECTION("Move empty vector") {
		fastgltf::static_vector<std::uint32_t> vec1(10, 1);
		fastgltf::static_vector<std::uint32_t> vec2(0);

		vec1 = std::move(vec2);
		REQUIRE(vec2.empty());
		REQUIRE(vec1.empty());
		REQUIRE(vec1.data() == nullptr);
	}
}

TEST_CASE("Test static_vector allocator behaviour", "[static-vector]") {
	SECTION("Copy with two different polymorphic resources") {
		std::array<std::byte, 256> buffer {};
		std::pmr::monotonic_buffer_resource resource(buffer.data(), buffer.size(), std::pmr::null_memory_resource());

		fastgltf::pmr::static_vector<std::uint32_t> vec1(10, 1);
		fastgltf::pmr::static_vector<std::uint32_t> vec2(20, 2, &resource);

		vec1 = vec2;
		REQUIRE(vec1.size() == 20);
		REQUIRE(vec1.data() != nullptr);
		REQUIRE(vec1.get_allocator().resource() == std::pmr::get_default_resource());
		REQUIRE(vec2.size() == 20);
		REQUIRE(vec2.data() != nullptr);
		for (auto& element : vec1) {
			REQUIRE(element == 2);
		}
	}

	SECTION("Move with two different polymorphic resources") {
		std::array<std::byte, 256> buffer {};
		std::pmr::monotonic_buffer_resource resource(buffer.data(), buffer.size(), std::pmr::null_memory_resource());

		fastgltf::pmr::static_vector<std::uint32_t> vec1(10, 1);
		fastgltf::pmr::static_vector<std::uint32_t> vec2(20, 2, &resource);

		vec1 = std::move(vec2);
		REQUIRE(vec1.size() == 20);
		REQUIRE(vec1.data() != nullptr);
		REQUIRE(vec1.get_allocator().resource() == std::pmr::get_default_resource());
		REQUIRE(vec2.empty());
		REQUIRE(vec2.data() == nullptr);
		for (auto& element : vec1) {
			REQUIRE(element == 2);
		}
	}
}

namespace {
	template <typename T>
	fastgltf::static_vector<T> makestatic_vector(std::initializer_list<T> values) {
		fastgltf::static_vector<T> vector(values.size());
		std::size_t i = 0;
		for (const auto& value : values)
			vector[i++] = value;
		return vector;
	}

	// type that only implements operator< to test the weak_ordering fallback for the operator<=> of static_vector
	struct LessOnly {
		int value;
		friend bool operator<(const LessOnly& a, const LessOnly& b) { return a.value < b.value; }
	};
} // namespace

TEST_CASE("Test static_vector three-way comparison", "[static-vector]") {
	const auto a = makestatic_vector({1, 2, 3});
	const auto b = makestatic_vector({1, 2, 3});
	const auto c = makestatic_vector({1, 2, 4});
	const auto prefix = makestatic_vector({1, 2});

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
		const auto f1 = makestatic_vector({1.0f, 2.0f});
		const auto f2 = makestatic_vector({1.0f, 2.5f});

		static_assert(std::is_same_v<decltype(f1 <=> f2), std::partial_ordering>);
		REQUIRE(((f1 <=> f2) < 0));
		REQUIRE(((f1 <=> f1) == 0));
	}

	SECTION("Types without operator<=>") {
		const auto l1 = makestatic_vector({LessOnly {1}, LessOnly {2}});
		const auto l2 = makestatic_vector({LessOnly {1}, LessOnly {3}});

		static_assert(std::is_same_v<decltype(l1 <=> l2), std::weak_ordering>);
		REQUIRE(((l1 <=> l2) < 0));
		REQUIRE(((l2 <=> l1) > 0));
		REQUIRE(((l1 <=> l1) == 0));
	}
}
