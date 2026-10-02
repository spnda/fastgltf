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
#include <fastgltf/containers/allocator_utils.hpp>

#if !defined(FASTGLTF_MODULE) && FASTGLTF_HAS_CONTAINERS_RANGES
#include <ranges>
#endif

namespace fastgltf {
	/**
	 * A static vector which cannot be resized freely. When constructed, the backing array is allocated once.
	 */
	FASTGLTF_EXPORT template <typename T, typename Allocator = std::allocator<T>>
	class static_vector final {
		using traits = std::allocator_traits<Allocator>;

	public:
		using value_type = T;
		using allocator_type = Allocator;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;

		using reference = value_type&;
		using const_reference = const value_type&;

		using pointer = traits::pointer;
		using const_pointer = traits::const_pointer;

		using iterator = T*;
		using const_iterator = const T*;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	private:
		FASTGLTF_NO_UNIQUE_ADDRESS Allocator _allocator;

		size_type _size = 0;
		pointer _data = nullptr;

		void clear_and_deallocate() {
			if (_data != nullptr) [[likely]] {
				internal::allocator_destroy(_allocator, begin(), end());
				traits::deallocate(_allocator, _data, _size);
				_data = nullptr;
				_size = 0;
			}
		}

		/**
		 * Allocates storage for n elements and calls construct with a pointer to it, which has to construct all
		 * n elements. If construct throws, the storage is freed again and the vector stays empty.
		 * Expects the vector to currently be empty and without an allocation.
		 */
		template <typename Construct>
		void allocate_and_construct(const size_type n, Construct&& construct) {
			assert(_data == nullptr);
			if (n == 0) {
				return;
			}

			pointer data = traits::allocate(_allocator, n);
			auto guard = make_exception_guard([&] {
				traits::deallocate(_allocator, data, n);
			});

			construct(data);
			guard.complete();

			_data = data;
			_size = n;
		}

	public:
		static_vector() noexcept(std::is_nothrow_default_constructible_v<allocator_type>) = default;

		explicit static_vector(const Allocator& allocator) noexcept : _allocator(allocator) {}

		explicit static_vector(const size_type size, const Allocator& allocator = Allocator()) : _allocator(allocator) {
			allocate_and_construct(size, [&](pointer data) {
				if constexpr (std::is_trivially_default_constructible_v<T> && !std::uses_allocator_v<T, Allocator>) {
					std::uninitialized_default_construct_n(data, size);
				} else {
					internal::allocator_construct_n(_allocator, data, size);
				}
			});
		}
		explicit static_vector(const size_type size, const T& initialValue, const Allocator& allocator = Allocator()) : _allocator(allocator) {
			allocate_and_construct(size, [&](pointer data) {
				internal::allocator_construct_n(_allocator, data, size, initialValue);
			});
		}

#if FASTGLTF_HAS_CONTAINERS_RANGES
		template <std::ranges::input_range R>
		requires (std::ranges::forward_range<R> || std::ranges::sized_range<R>) && std::convertible_to<std::ranges::range_reference_t<R>, T>
		static_vector(std::from_range_t, R&& range, const Allocator& allocator = Allocator()) : static_vector(allocator) {
			const auto n = static_cast<size_type>(std::ranges::distance(range));
			allocate_and_construct(n, [&](pointer data) {
				// This uses memcpy when possible
				internal::uninitialized_allocator_copy_n(
					_allocator, std::ranges::begin(range), n, data);
			});
		}
#endif

		static_vector(const static_vector& other) : _allocator(traits::select_on_container_copy_construction(other._allocator)) {
			allocate_and_construct(other.size(), [&](pointer data) {
				internal::uninitialized_allocator_copy(
					_allocator, other.begin(), other.end(), data);
			});
		}

		static_vector(const static_vector& other, const Allocator& allocator) : _allocator(allocator) {
			allocate_and_construct(other.size(), [&](pointer data) {
				internal::uninitialized_allocator_copy(
					_allocator, other.begin(), other.end(), data);
			});
		}

		static_vector(static_vector&& other) noexcept : _allocator(std::move(other._allocator)) {
			_data = std::exchange(other._data, nullptr);
			_size = std::exchange(other._size, 0);
		}

		static_vector(static_vector&& other, const Allocator& allocator) : _allocator(allocator) {
			if (_allocator == other._allocator) {
				_data = std::exchange(other._data, nullptr);
				_size = std::exchange(other._size, 0);
			} else {
				allocate_and_construct(other.size(), [&](pointer data) {
					internal::uninitialized_allocator_copy(
						_allocator,
						std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()),
						data);
				});
				other.clear_and_deallocate();
			}
		}

		static_vector& operator=(const static_vector& other) {
			if (std::addressof(other) == this) [[unlikely]]{
				return *this;
			}

			clear_and_deallocate();

			if constexpr (traits::propagate_on_container_copy_assignment::value) {
				_allocator = other._allocator;
			}

			allocate_and_construct(other.size(), [&](pointer data) {
				internal::uninitialized_allocator_copy(
					_allocator, other.begin(), other.end(), data);
			});
			return *this;
		}

		static_vector& operator=(static_vector&& other) noexcept(traits::propagate_on_container_move_assignment::value || traits::is_always_equal::value) {
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
			} else {
				allocate_and_construct(other.size(), [&](pointer data) {
					internal::uninitialized_allocator_copy(
						_allocator,
						std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()),
						data);
				});
				other.clear_and_deallocate();
			}

			return *this;
		}

		~static_vector() {
			clear_and_deallocate();
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

		[[nodiscard]] reference operator[](size_type idx) {
			assert(idx < size());
			return begin()[idx];
		}
		[[nodiscard]] const_reference operator[](size_type idx) const {
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
		constexpr bool operator==(const static_vector& other) const {
			return size() == other.size() && std::equal(begin(), end(), other.begin());
		}
		constexpr auto operator<=>(const static_vector& other) const {
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
		using static_vector = static_vector<T, std::pmr::polymorphic_allocator<T>>;
	} // namespace pmr
#endif
} // namespace fastgltf

#endif
