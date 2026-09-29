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

#ifndef FASTGLTF_STATIC_VECTOR_HPP
#define FASTGLTF_STATIC_VECTOR_HPP

#if !defined(FASTGLTF_MODULE)
#include <algorithm>
#include <cassert>
#include <vector>
#endif

#include <fastgltf/util.hpp>

namespace fastgltf {
	/**
	 * A static vector which cannot be resized freely. When constructed, the backing array is allocated once.
	 */
	FASTGLTF_EXPORT template <typename T, typename Allocator = std::allocator<T>>
	class StaticVector final {
	public:
		using value_type = T;
		using size_type = std::size_t;

	private:
		FASTGLTF_NO_UNIQUE_ADDRESS Allocator _allocator;
		using traits = std::allocator_traits<Allocator>;

		size_type _size = 0;
		T* _data = nullptr;

		void clear_and_deallocate() {
			if (_data != nullptr) [[likely]] {
				std::destroy(begin(), end());
				traits::deallocate(_allocator, _data, _size);
				_data = nullptr;
				_size = 0;
			}
		}

	public:
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = value_type*;
		using const_pointer = const value_type*;
		using iterator = pointer;
		using const_iterator = const_pointer;

		explicit StaticVector(const std::size_t size, const Allocator& allocator = Allocator()) : _allocator(allocator), _size(size) {
			if (_size != 0) {
				_data = traits::allocate(_allocator, size);
				std::uninitialized_default_construct(begin(), end());
			}
		}
		explicit StaticVector(const std::size_t size, const T& initialValue, const Allocator& allocator = Allocator()) : _allocator(allocator), _size(size) {
			if (_size != 0) {
				_data = traits::allocate(_allocator, size);
				std::uninitialized_fill(begin(), end(), initialValue);
			}
		}

		StaticVector(const StaticVector& other) : _allocator(traits::select_on_container_copy_construction(other._allocator)) {
			if (!other.empty()) [[likely]] {
				_data = traits::allocate(_allocator, other.size());
				_size = other.size();
				std::uninitialized_copy(other.begin(), other.end(), begin());
			}
		}

		StaticVector(StaticVector&& other) noexcept : _allocator(std::move(other._allocator)) {
			_data = std::exchange(other._data, nullptr);
			_size = std::exchange(other._size, 0);
		}

		StaticVector& operator=(const StaticVector& other) {
			if (std::addressof(other) == this) [[unlikely]]{
				return *this;
			}

			clear_and_deallocate();

			if constexpr (traits::propagate_on_container_copy_assignment::value) {
				_allocator = other._allocator;
			}

			if (!other.empty()) [[likely]] {
				_data = traits::allocate(_allocator, other.size());
				_size = other.size();
				std::uninitialized_copy(other.begin(), other.end(), begin());
			}
			return *this;
		}

		StaticVector& operator=(StaticVector&& other) noexcept(traits::propagate_on_container_move_assignment::value || traits::is_always_equal::value) {
			if (std::addressof(other) == this) [[unlikely]] {
				return *this;
			}

			clear_and_deallocate();

			if (traits::propagate_on_container_move_assignment::value || _allocator == other._allocator) {
				if constexpr (traits::propagate_on_container_move_assignment::value) {
					_allocator = std::move(other._allocator);
				}

				_data = std::exchange(other._data, nullptr);
				_size = std::exchange(other._size, 0);
			} else if (!other.empty()) [[likely]] {
				_data = traits::allocate(_allocator, other.size());
				_size = other.size();
				std::uninitialized_move(other.begin(), other.end(), begin());
				other.clear_and_deallocate();
			}

			return *this;
		}

		~StaticVector() {
			clear_and_deallocate();
		}

		/**
		 * Copies the contents of the given vector into a new StaticVector.
		 */
		static auto fromVector(const std::vector<T>& vector) {
			StaticVector staticVector(vector.size());
			std::ranges::copy(vector.begin(), vector.end(), staticVector.begin());
			return staticVector;
		}

		[[nodiscard]] Allocator get_allocator() const noexcept {
			return _allocator;
		}

		[[nodiscard]] pointer data() noexcept {
			return _data;
		}

		[[nodiscard]] const_pointer data() const noexcept {
			return _data;
		}

		[[nodiscard]] size_type size() const noexcept {
			return _size;
		}

		[[nodiscard]] size_type size_bytes() const noexcept {
			return _size * sizeof(value_type);
		}

		[[nodiscard]] bool empty() const noexcept {
			return _size == 0;
		}

		[[nodiscard]] iterator begin() noexcept { return data(); }
		[[nodiscard]] const_iterator begin() const noexcept { return data(); }
		[[nodiscard]] const_iterator cbegin() const noexcept { return data(); }
		[[nodiscard]] iterator end() noexcept { return begin() + size(); }
		[[nodiscard]] const_iterator end() const noexcept { return begin() + size(); }
		[[nodiscard]] const_iterator cend() const noexcept { return begin() + size(); }

		[[nodiscard]] T& operator[](std::size_t idx) {
			assert(idx < size());
			return begin()[idx];
		}
		[[nodiscard]] const T& operator[](std::size_t idx) const {
			assert(idx < size());
			return begin()[idx];
		}

	private:
		static constexpr auto compare_three_way = []<typename U, typename V>(const U& u, const V& v) {
			if constexpr (std::three_way_comparable_with<U, V>) {
				return u <=> v;
			} else {
				if (u < v) return std::weak_ordering::less;
				if (v < u) return std::weak_ordering::greater;
				return std::weak_ordering::equivalent;
			}
		};

	public:
		constexpr bool operator==(const StaticVector& other) const {
			return size() == other.size() && std::equal(begin(), end(), other.begin());
		}
		constexpr auto operator<=>(const StaticVector& other) const {
			return std::lexicographical_compare_three_way(
				begin(), end(), other.begin(), other.end(),
				compare_three_way);
		}

		constexpr bool operator==(const std::vector<value_type>& other) const {
			return size() == other.size() && std::equal(begin(), end(), other.begin());
		}
		constexpr auto operator<=>(const std::vector<value_type>& other) const {
			return std::lexicographical_compare_three_way(
				begin(), end(), other.begin(), other.end(),
				compare_three_way);
		}
	};

#if !FASTGLTF_MISSING_MEMORY_RESOURCE
	namespace pmr {
		FASTGLTF_EXPORT template <typename T>
		using StaticVector = StaticVector<T, std::pmr::polymorphic_allocator<T>>;
	} // namespace pmr
#endif
} // namespace fastgltf

#endif
