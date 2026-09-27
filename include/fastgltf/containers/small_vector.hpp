/*
 * Copyright (C) 2022 - 2026 Sean Apeler
 * This file is part of fastgltf <https://github.com/spnda/fastgltf>.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef FASTGLTF_SMALL_VECTOR_HPP
#define FASTGLTF_SMALL_VECTOR_HPP

#if !defined(FASTGLTF_USE_STD_MODULE) || !FASTGLTF_USE_STD_MODULE
#include <vector>
#endif

#include <fastgltf/util.hpp>

namespace fastgltf {
	/*
	 * The amount of items that the SmallVector can initially store in the storage
	 * allocated within the object itself.
	 */
	static constexpr auto initialSmallVectorStorage = 8;

	/**
	 * A custom vector class for fastgltf, which can store up to N objects within itself.
	 * This is useful for cases where the vector is expected to only ever hold a tiny amount of small objects,
	 * such as a node's children.
	 * SmallVector is also mostly conformant to C++17's std::vector, and can therefore be used as a drop-in replacement.
	 * @note It is also available with polymorphic allocators in the fastgltf::pmr namespace.
	 */
	FASTGLTF_EXPORT template <typename T, std::size_t N = initialSmallVectorStorage, typename Allocator = std::allocator<T>>
	class SmallVector final {
		static_assert(N != 0, "Cannot create a SmallVector with 0 initial capacity");

		alignas(T) std::array<std::byte, N * sizeof(T)> storage = {};

		Allocator allocator;

		T* _data;
		std::size_t _size = 0, _capacity = N;

		void copy(const T* first, const std::size_t count, T* result) {
			if constexpr (std::is_trivially_copyable_v<T>) {
				std::memcpy(result, first, count * sizeof(T));
			} else {
				for (std::size_t i = 0; i < count; ++i) {
					result[i] = first[i];
				}
			}
		}

		void move_elements(T* first, const std::size_t count, T* result) {
			if constexpr (std::is_trivially_copyable_v<T>) {
				std::memcpy(result, first, count * sizeof(T));
			} else {
				for (std::size_t i = 0; i < count; ++i) {
					result[i] = std::move(first[i]);
				}
			}
		}

	public:
		using iterator = T*;
		using const_iterator = const T*;

		SmallVector() : _data(reinterpret_cast<T*>(storage.data())) {}

		explicit SmallVector(const Allocator& allocator) noexcept : allocator(allocator), _data(reinterpret_cast<T*>(storage.data())) {}

		explicit SmallVector(std::size_t size, const Allocator& allocator = Allocator()) : allocator(allocator), _data(reinterpret_cast<T*>(storage.data())) {
			resize(size);
		}

		SmallVector(std::size_t size, const T& value, const Allocator& allocator = Allocator()) : allocator(allocator), _data(reinterpret_cast<T*>(storage.data())) {
			assign(size, value);
		}

		SmallVector(std::initializer_list<T> init, const Allocator& allocator = Allocator()) : allocator(allocator), _data(reinterpret_cast<T*>(storage.data())) {
			assign(init);
		}

		SmallVector(const SmallVector& other) noexcept : _data(reinterpret_cast<T*>(storage.data())) {
			resize(other.size());
			copy(other.begin(), other.size(), begin());
		}

		SmallVector(SmallVector&& other) noexcept : _data(reinterpret_cast<T*>(storage.data())) {
			if (other.isUsingStack()) {
				if (!other.empty()) {
					resize(other.size());
					move_elements(other.begin(), other.size(), begin());
					other._data = reinterpret_cast<T*>(other.storage.data()); // Reset pointer
					_size = std::exchange(other._size, 0);
					_capacity = std::exchange(other._capacity, N);
				}
			} else {
				_data = std::exchange(other._data, nullptr);
				_size = std::exchange(other._size, 0);
				_capacity = std::exchange(other._capacity, N);
			}
		}

		SmallVector& operator=(const SmallVector& other) {
			if (std::addressof(other) != this) {
				if (!isUsingStack() && _data) {
					std::destroy(begin(), end());
					allocator.deallocate(_data, _capacity);
					_data = reinterpret_cast<T*>(storage.data());
					_size = _capacity = 0;
				}

				resize(other.size());
				copy(other.begin(), other.size(), begin());
			}
			return *this;
		}

		SmallVector& operator=(SmallVector&& other) noexcept {
			if (std::addressof(other) != this) {
				if (other.isUsingStack()) {
					if (!other.empty()) {
						resize(other.size());
						move_elements(other.begin(), other.size(), begin());
						other._data = reinterpret_cast<T*>(other.storage.data()); // Reset pointer
						_size = std::exchange(other._size, 0);
						_capacity = std::exchange(other._capacity, N);
					}
				} else {
					_data = std::exchange(other._data, nullptr);
					_size = std::exchange(other._size, 0);
					_capacity = std::exchange(other._capacity, N);
				}
			}
			return *this;
		}

		~SmallVector() {
			// As we use an array of std::byte for the stack storage, we have to destruct those manually too.
			std::destroy(begin(), end());

			if (!isUsingStack() && _data) {
				// Not using the stack, we'll have to free.
				allocator.deallocate(_data, _capacity);
			}
		}

		[[nodiscard]] iterator begin() noexcept { return _data; }
		[[nodiscard]] const_iterator begin() const noexcept { return _data; }
		[[nodiscard]] const_iterator cbegin() const noexcept { return _data; }
		[[nodiscard]] iterator end() noexcept { return begin() + size(); }
		[[nodiscard]] const_iterator end() const noexcept { return begin() + size(); }
		[[nodiscard]] const_iterator cend() const noexcept { return begin() + size(); }

		[[nodiscard]] std::reverse_iterator<T*> rbegin() { return end(); }
		[[nodiscard]] std::reverse_iterator<const T*> rbegin() const { return end(); }
		[[nodiscard]] std::reverse_iterator<const T*> crbegin() const { return end(); }
		[[nodiscard]] std::reverse_iterator<T*> rend() { return begin(); }
		[[nodiscard]] std::reverse_iterator<const T*> rend() const { return begin(); }
		[[nodiscard]] std::reverse_iterator<const T*> crend() const { return begin(); }

		[[nodiscard]] T* data() noexcept { return _data; }
		[[nodiscard]] const T* data() const noexcept { return _data; }
		[[nodiscard]] std::size_t size() const noexcept { return _size; }
		[[nodiscard]] std::size_t size_in_bytes() const noexcept { return _size * sizeof(T); }
		[[nodiscard]] std::size_t capacity() const noexcept { return _capacity; }

		[[nodiscard]] bool empty() const noexcept { return _size == 0; }
		[[nodiscard]] bool isUsingStack() const noexcept { return data() == reinterpret_cast<const T*>(storage.data()); }

		void reserve(std::size_t newCapacity) {
			static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>, "T needs to be copy constructible.");

			// We don't want to reduce capacity with reserve, only with shrink_to_fit.
			if (newCapacity <= capacity()) {
				return;
			}

			// If the new capacity is lower than what we can hold on the stack, we ignore this request.
			if (newCapacity <= N && isUsingStack()) {
				_capacity = newCapacity;
				if (_size > _capacity) {
					_size = _capacity;
				}
				return;
			}

			// We use geometric growth, similarly to std::vector.
            newCapacity = static_cast<std::size_t>(1) << (std::numeric_limits<decltype(newCapacity)>::digits - std::countl_zero(newCapacity));

			T* alloc = allocator.allocate(newCapacity);

			// Copy/Move the old data into the new memory
			for (std::size_t i = 0; i < size(); ++i) {
				auto& x = (*this)[i];
				if constexpr (std::is_nothrow_move_constructible_v<T>) {
					new(alloc + i) T(std::move(x));
				} else if constexpr (std::is_copy_constructible_v<T>) {
					new(alloc + i) T(x);
				} else {
					new(alloc + i) T(std::move(x));
				}
			}

			// Destroy all objects in the old allocation
			std::destroy(begin(), end());

			if (!isUsingStack() && _data && size() != 0) {
				allocator.deallocate(_data, _capacity);
			}

			_data = alloc;
			_capacity = newCapacity;
		}

		void resize(std::size_t newSize) {
			static_assert(std::is_constructible_v<T>, "T has to be constructible");
			if (newSize == size()) {
				return;
			}

			if (newSize < size()) {
				// Just destroy the "overflowing" elements.
				std::destroy(begin() + newSize, end());
			} else {
				// Reserve enough capacity and copy the new value over.
				auto oldSize = _size;
				reserve(newSize);
				for (auto it = begin() + oldSize; it != begin() + newSize; ++it) {
					new (it) T();
				}
			}

			_size = newSize;
		}

		void resize(std::size_t newSize, const T& value) {
			static_assert(std::is_copy_constructible_v<T>, "T needs to be copy constructible.");
			if (newSize == size()) {
				return;
			}

			if (newSize < size()) {
				// Just destroy the "overflowing" elements.
				std::destroy(begin() + newSize, end());
			} else {
				// Reserve enough capacity and copy the new value over.
				auto oldSize = _size;
				reserve(newSize);
				for (auto it = begin() + oldSize; it != begin() + newSize; ++it) {
					if (it == nullptr)
						break;

					if constexpr (std::is_move_constructible_v<T>) {
						new (it) T(std::move(value));
					} else if constexpr (std::is_trivially_copyable_v<T>) {
						std::memcpy(it, std::addressof(value), sizeof(T));
					} else {
						new (it) T(value);
					}
				}
			}

			_size = newSize;
		}

		void shrink_to_fit() {
			// Only have to shrink if there's any unused capacity.
			if (capacity() == size() || size() == 0) {
				return;
			}

			// If we can use the object's memory again, we'll copy everything over.
			if (size() <= N) {
				copy(begin(), size(), reinterpret_cast<T*>(storage.data()));
				_data = reinterpret_cast<T*>(storage.data());
			} else {
				// We have to use heap allocated memory.
				auto* alloc = allocator.allocate(size());
				for (std::size_t i = 0; i < size(); ++i) {
					new(alloc + i) T((*this)[i]);
				}

				if (_data && !isUsingStack()) {
					std::destroy(begin(), end());
					allocator.deallocate(_data, _capacity);
				}

				_data = alloc;
			}

			_capacity = _size;
		}

		void assign(std::size_t count, const T& value) {
			clear();
			resize(count, value);
		}

		void assign(std::initializer_list<T> init) {
			static_assert(std::is_trivially_copyable_v<T> || std::is_copy_constructible_v<T>, "T needs to be trivially copyable or be copy constructible");
			clear();
			reserve(init.size());
			_size = init.size();

			if constexpr (std::is_trivially_copyable_v<T>) {
				std::memcpy(begin(), init.begin(), init.size() * sizeof(T));
			} else if constexpr (std::is_copy_constructible_v<T>) {
				for (auto it = init.begin(); it != init.end(); ++it) {
					new (_data + std::distance(init.begin(), it)) T(*it);
				}
			}
		}

		void clear() noexcept {
			std::destroy(begin(), end());

			if (!isUsingStack() && size() != 0) {
				allocator.deallocate(_data, _capacity);
				_data = reinterpret_cast<T*>(storage.data());
			}

			_size = 0;
		}

		template <typename... Args>
		decltype(auto) emplace_back(Args&&... args) {
			// We reserve enough capacity for the new element, and then just increment the size.
			reserve(_size + 1);
			++_size;
			new (std::addressof(back())) T(std::forward<Args>(args)...);
			return (back());
		}

		[[nodiscard]] T& at(std::size_t idx) {
			if (idx >= size()) {
				raise<std::out_of_range>("Index is out of range for SmallVector");
			}
			return begin()[idx];
		}
		[[nodiscard]] const T& at(std::size_t idx) const {
			if (idx >= size()) {
				raise<std::out_of_range>("Index is out of range for SmallVector");
			}
			return begin()[idx];
		}

		[[nodiscard]] T& operator[](std::size_t idx) {
			assert(idx < size());
			return begin()[idx];
		}
		[[nodiscard]] const T& operator[](std::size_t idx) const {
			assert(idx < size());
			return begin()[idx];
		}

		[[nodiscard]] T& front() {
			assert(!empty());
			return begin()[0];
		}
		[[nodiscard]] const T& front() const {
			assert(!empty());
			return begin()[0];
		}

		[[nodiscard]] T& back() {
			assert(!empty());
			return end()[-1];
		}
		[[nodiscard]] const T& back() const {
			assert(!empty());
			return end()[-1];
		}
	};

#if !FASTGLTF_MISSING_MEMORY_RESOURCE
	namespace pmr {
		FASTGLTF_EXPORT template<typename T, std::size_t N>
		using SmallVector = SmallVector<T, N, std::pmr::polymorphic_allocator<T>>;
	} // namespace pmr
#endif

#ifndef FASTGLTF_USE_CUSTOM_SMALLVECTOR
#define FASTGLTF_USE_CUSTOM_SMALLVECTOR 0
#endif

#if FASTGLTF_USE_CUSTOM_SMALLVECTOR
	FASTGLTF_EXPORT template <typename T, std::size_t N = initialSmallVectorStorage>
	using MaybeSmallVector = SmallVector<T, N>;
#else
	FASTGLTF_EXPORT template <typename T, std::size_t N = 0>
	using MaybeSmallVector = std::vector<T>;
#endif

#if !FASTGLTF_MISSING_MEMORY_RESOURCE
	namespace pmr {
#if FASTGLTF_USE_CUSTOM_SMALLVECTOR
		FASTGLTF_EXPORT template <typename T, std::size_t N = initialSmallVectorStorage>
		using MaybeSmallVector = pmr::SmallVector<T, N>;
#else
		FASTGLTF_EXPORT template <typename T, std::size_t N = 0>
		using MaybeSmallVector = std::pmr::vector<T>;
#endif
	} // namespace pmr
#endif
} // namespace fastgltf

#endif
