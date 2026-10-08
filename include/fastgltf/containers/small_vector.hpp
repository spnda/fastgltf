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

#if !defined(FASTGLTF_MODULE)
#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <vector>
#endif

#include <fastgltf/util.hpp>
#include <fastgltf/containers/allocator_utils.hpp>

namespace fastgltf {
	/*
	 * The amount of items that the small_vector can initially store in the storage
	 * allocated within the object itself.
	 */
	inline constexpr auto initial_small_vector_storage = 8;

	/**
	 * A custom vector class for fastgltf, which can store up to N objects within itself.
	 * This is useful for cases where the vector is expected to only ever hold a tiny amount of small objects,
	 * such as a node's children.
	 * small_vector is also mostly conformant to C++17's std::vector, and can therefore be used as a drop-in replacement.
	 * @note It is also available with polymorphic allocators in the fastgltf::pmr namespace.
	 */
	FASTGLTF_EXPORT template <typename T, std::size_t N = initial_small_vector_storage, typename Allocator = std::allocator<T>>
	class small_vector final {
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
		static_assert(N != 0, "Cannot create a small_vector with 0 initial capacity");

		alignas(T) std::array<std::byte, N * sizeof(T)> _storage;

		FASTGLTF_NO_UNIQUE_ADDRESS Allocator _allocator;

		pointer _data = reinterpret_cast<pointer>(_storage.data());
		size_type _size = 0, _capacity = N;

		void destroy_vector() {
			clear();
			if (!is_using_stack()) {
				traits::deallocate(_allocator, _data, _capacity);
				_data = reinterpret_cast<pointer>(_storage.data());
				_capacity = N;
			}
		}

		/**
		 * Moves all elements into the uninitialized memory at dest, destroys the old elements, and frees the
		 * old allocation if it was on the heap. The caller is responsible for updating _capacity afterwards.
		 */
		void relocate(pointer dest) {
			internal::uninitialized_allocator_relocate(_allocator, begin(), end(), dest);

			if (!is_using_stack()) {
				traits::deallocate(_allocator, _data, _capacity);
			}
			_data = dest;
		}

		/**
		 * Moves all elements into a new heap allocation of the given capacity. If moving the elements throws,
		 * the new allocation is freed and the vector is left unchanged.
		 */
		void reallocate(const size_type newCapacity) {
			pointer alloc = traits::allocate(_allocator, newCapacity);
			auto guard = make_exception_guard([&] {
				traits::deallocate(_allocator, alloc, newCapacity);
			});

			relocate(alloc);
			guard.complete();
			_capacity = newCapacity;
		}

		template <std::input_iterator Iterator, typename Sentinel>
		void init_with_size(Iterator first, Sentinel last, const size_type n) {
			auto guard = make_exception_guard([&] {
				destroy_vector();
			});

			if (n > 0) {
				reserve(n);
				internal::uninitialized_allocator_copy(_allocator, std::move(first), std::move(last), begin());
				_size = n;
			}

			guard.complete();
		}

		template <typename Iterator>
		void assign_sized(Iterator first, size_type n) {
			if (n > capacity()) {
				// we have to reallocate anyway
				clear();
				reserve(n);
				internal::uninitialized_allocator_copy_n(_allocator, std::move(first), n, begin());
				_size = n;
				return;
			}

			if (n > size()) {
				auto [in, out] = std::ranges::copy_n(std::move(first),
					static_cast<std::iter_difference_t<Iterator>>(size()), begin());
				internal::uninitialized_allocator_copy_n(
					_allocator, std::move(in), n - size(), end());
			} else {
				std::ranges::copy_n(std::move(first),
					static_cast<std::iter_difference_t<Iterator>>(n), begin());

				internal::allocator_destroy(_allocator, begin() + n, end());
			}
			_size = n;
		}

		template <typename Iterator, typename Sentinel>
		void assign_sentinel(Iterator first, Sentinel last) {
			iterator cur = begin();
			while (first != last && cur != end()) {
				*cur = *first;
				++first;
				++cur;
			}

			if (cur != end()) {
				internal::allocator_destroy(_allocator, cur, end());
				_size = static_cast<size_type>(std::distance(begin(), cur));
			} else {
				while (first != last) {
					emplace_back(*first);
					++first;
				}
			}
		}

		template <std::ranges::input_range R>
		void append_range_impl(R&& range) {
			if constexpr (std::ranges::forward_range<R> || std::ranges::sized_range<R>) {
				const auto n = static_cast<size_type>(std::ranges::distance(range));
				reserve(size() + n);
				internal::uninitialized_allocator_copy_n(_allocator, std::ranges::begin(range), n, end());
				_size += n;
			} else {
				for (auto&& element : range) {
					emplace_back(std::forward<decltype(element)>(element));
				}
			}
		}

	public:
		small_vector() noexcept(std::is_nothrow_default_constructible_v<allocator_type>) = default;

		explicit small_vector(const Allocator& allocator) noexcept : _allocator(allocator) {}

		explicit small_vector(const size_type size, const Allocator& allocator = Allocator()) : small_vector(allocator) {
			resize(size);
		}

		small_vector(const size_type size, const_reference value, const Allocator& allocator = Allocator()) : small_vector(allocator) {
			assign(size, value);
		}

		small_vector(std::initializer_list<value_type> init, const Allocator& allocator = Allocator()) : small_vector(allocator) {
			init_with_size(init.begin(), init.end(), init.size());
		}

#if FASTGLTF_HAS_CONTAINERS_RANGES
		template <std::ranges::input_range R>
		requires std::convertible_to<std::ranges::range_reference_t<R>, value_type>
		small_vector(std::from_range_t, R&& range, const Allocator& allocator = Allocator()) : small_vector(allocator) {
			append_range_impl(range);
		}
#endif

		small_vector(const small_vector& other)
				: _allocator(traits::select_on_container_copy_construction(other._allocator)) {
			init_with_size(other.begin(), other.end(), other.size());
		}

		small_vector(const small_vector& other, const Allocator& allocator) : _allocator(allocator) {
			init_with_size(other.begin(), other.end(), other.size());
		}

		small_vector(small_vector&& other) noexcept(std::is_nothrow_move_constructible_v<value_type>) : _allocator(std::move(other._allocator)) {
			if (other.is_using_stack()) {
				// since N is the same we can assume that capacity() == other.capacity(), so just move each element over
				internal::uninitialized_allocator_copy(_allocator,
					std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()),
					begin());
				_size = other.size();
				other.clear();
			} else {
				// allow other to be re-used by assigning its data with its internal buffer again
				_data = std::exchange(other._data, reinterpret_cast<pointer>(other._storage.data()));
				_size = std::exchange(other._size, 0);
				_capacity = std::exchange(other._capacity, N);
			}
		}

		small_vector(small_vector&& other, const Allocator& allocator) : small_vector(allocator) {
			if (!other.is_using_stack() && _allocator == other._allocator) {
				// Same allocator, so just re-use the allocation.
				_data = std::exchange(other._data, reinterpret_cast<pointer>(other._storage.data()));
				_size = std::exchange(other._size, 0);
				_capacity = std::exchange(other._capacity, N);
			} else {
				reserve(other.size());
				internal::uninitialized_allocator_copy(_allocator,
					std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()),
					begin());
				_size = other.size();
				other.clear();
			}
		}

		small_vector& operator=(const small_vector& other) {
			if (std::addressof(other) != this) {
				if constexpr (traits::propagate_on_container_copy_assignment::value) {
					// Memory from our current allocator can't be freed by the new one, so release it first.
					if (_allocator != other._allocator) {
						destroy_vector();
					}
					_allocator = other._allocator;
				}
				assign_sized(other.begin(), other.size());
			}
			return *this;
		}

		small_vector& operator=(small_vector&& other) noexcept(
				(traits::propagate_on_container_move_assignment::value
				 || traits::is_always_equal::value)
				&& std::is_nothrow_move_constructible_v<value_type>) {
			if (std::addressof(other) == this) {
				return *this;
			}

			clear();

			if (!other.is_using_stack() && (traits::propagate_on_container_move_assignment::value || _allocator == other._allocator)) {
				if (!is_using_stack()) {
					traits::deallocate(_allocator, _data, _capacity);
				}
				if constexpr (traits::propagate_on_container_move_assignment::value) {
					_allocator = std::move(other._allocator);
				}

				_data = std::exchange(other._data, reinterpret_cast<pointer>(other._storage.data()));
				_size = std::exchange(other._size, 0);
				_capacity = std::exchange(other._capacity, N);
			} else {
				// we can't reuse the allocation, so just re-allocate and move everything over
				reserve(other.size());
				internal::uninitialized_allocator_copy(_allocator,
					std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()),
					begin());
				_size = other.size();
				other.clear();
			}
			return *this;
		}

		~small_vector() {
			internal::allocator_destroy(_allocator, begin(), end());

			if (!is_using_stack()) {
				// Not using the stack, we'll have to free.
				traits::deallocate(_allocator, _data, _capacity);
				_data = reinterpret_cast<pointer>(_storage.data());
				_capacity = N;
			}
		}

		[[nodiscard]] iterator begin() noexcept { return _data; }
		[[nodiscard]] const_iterator begin() const noexcept { return _data; }
		[[nodiscard]] const_iterator cbegin() const noexcept { return _data; }
		[[nodiscard]] iterator end() noexcept { return begin() + size(); }
		[[nodiscard]] const_iterator end() const noexcept { return begin() + size(); }
		[[nodiscard]] const_iterator cend() const noexcept { return begin() + size(); }

		[[nodiscard]] reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
		[[nodiscard]] const_reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
		[[nodiscard]] const_reverse_iterator crbegin() const noexcept { return reverse_iterator(end()); }
		[[nodiscard]] reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
		[[nodiscard]] const_reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }
		[[nodiscard]] const_reverse_iterator crend() const noexcept { return reverse_iterator(begin()); }

		[[nodiscard]] pointer data() noexcept { return _data; }
		[[nodiscard]] const_pointer data() const noexcept { return _data; }
		[[nodiscard]] size_type size() const noexcept { return _size; }
		[[nodiscard]] size_type size_in_bytes() const noexcept { return _size * sizeof(T); }
		[[nodiscard]] size_type capacity() const noexcept { return _capacity; }

		[[nodiscard]] size_type max_size() const noexcept {
			return std::min<size_type>(traits::max_size(_allocator), std::numeric_limits<difference_type>::max());
		}

		[[nodiscard]] bool empty() const noexcept { return _size == 0; }
		[[nodiscard]] bool is_using_stack() const noexcept { return data() == reinterpret_cast<const_pointer>(_storage.data()); }

		[[nodiscard]] allocator_type get_allocator() const noexcept(std::is_nothrow_copy_constructible_v<allocator_type>) {
			return _allocator;
		}

		void reserve(const size_type newCapacity) requires (std::is_move_constructible_v<value_type> || std::is_copy_constructible_v<value_type>) {
			// We don't want to reduce capacity with reserve, only with shrink_to_fit.
			// This also covers everything that fits into the inline storage, whose capacity is always N.
			if (newCapacity <= capacity()) {
				return;
			}

			if (newCapacity > max_size()) {
				raise<std::length_error>("small_vector::reserve()");
			}

			reallocate(std::bit_ceil(newCapacity));
		}

		void resize(size_type newSize) requires std::is_constructible_v<value_type> {
			if (newSize == size()) {
				return;
			}

			if (newSize < size()) {
				// Just destroy the "overflowing" elements.
				internal::allocator_destroy(_allocator, begin() + newSize, end());
			} else {
				// Reserve enough capacity and copy the new value over.
				auto oldSize = _size;
				reserve(newSize);
				internal::allocator_construct(_allocator, begin() + oldSize, begin() + newSize);
			}

			_size = newSize;
		}

		void resize(size_type newSize, const_reference value) requires std::is_copy_constructible_v<value_type> {
			if (newSize <= size()) {
				// Just destroy the "overflowing" elements.
				internal::allocator_destroy(_allocator, begin() + newSize, end());
			} else if (newSize > capacity()) {
				// value might be a reference to an object within the existing array, so copy it
				const T copy(value);
				reserve(newSize);
				internal::allocator_construct(_allocator, begin() + size(), begin() + newSize, copy);
			} else {
				internal::allocator_construct(_allocator, begin() + size(), begin() + newSize, value);
			}

			_size = newSize;
		}

		void shrink_to_fit() {
			if (is_using_stack() || capacity() == size()) {
				return;
			}

			if (size() <= N) {
				// Move everything back into the inline storage, and free the heap allocation.
				relocate(reinterpret_cast<pointer>(_storage.data()));
				_capacity = N;
			} else {
				reallocate(size());
			}
		}

		void assign(const size_type count, const_reference value) {
			if (count <= capacity()) {
				const auto s = size();
				std::fill_n(begin(), std::min(s, count), value);
				if (s > count) {
					internal::allocator_destroy(_allocator, begin() + count, end());
				} else {
					internal::allocator_construct_n(_allocator, begin() + s, count - s, value);
				}
				_size = count;
			} else {
				// value might be a reference to an object within the existing array, so copy it
				const T copy(value);
				clear();
				reserve(count);
				internal::allocator_construct_n(_allocator, begin(), count, copy);
				_size = count;
			}
		}

		template <std::input_iterator Iterator>
		void assign(Iterator first, Iterator last) {
			if constexpr (std::forward_iterator<Iterator>) {
				assign_sized(first, static_cast<size_type>(std::distance(first, last)));
			} else {
				assign_sentinel(first, last);
			}
		}

		void assign(std::initializer_list<value_type> init) requires std::is_copy_constructible_v<value_type> {
			assign(init.begin(), init.end());
		}

#if FASTGLTF_HAS_CONTAINERS_RANGES
		template <std::ranges::input_range R>
		requires std::convertible_to<std::ranges::range_reference_t<R>, value_type>
			&& std::assignable_from<reference, std::ranges::range_reference_t<R>>
		void assign_range(R&& range) {
			if constexpr (std::ranges::forward_range<R> || std::ranges::sized_range<R>) {
				const auto n = static_cast<size_type>(std::ranges::distance(range));
				assign_sized(std::ranges::begin(range), n);
			} else {
				assign_sentinel(std::ranges::begin(range), std::ranges::end(range));
			}
		}
#endif

		void clear() noexcept {
			internal::allocator_destroy(_allocator, begin(), end());
			_size = 0;
		}

		template <typename... Args>
		decltype(auto) emplace_back(Args&&... args) {
			if (size() < capacity()) {
				traits::construct(_allocator, std::to_address(_data + size()), std::forward<Args>(args)...);
			} else {
				// The new element is constructed before relocating, as args might reference an element of this vector.
				const auto newCapacity = std::bit_ceil(size() + 1);
				pointer alloc = traits::allocate(_allocator, newCapacity);
				auto allocGuard = make_exception_guard([&] {
					traits::deallocate(_allocator, alloc, newCapacity);
				});

				const auto newElement = std::to_address(alloc + size());
				traits::construct(_allocator, newElement, std::forward<Args>(args)...);
				auto elementGuard = make_exception_guard([&] {
					traits::destroy(_allocator, newElement);
				});

				relocate(alloc);
				elementGuard.complete();
				allocGuard.complete();
				_capacity = newCapacity;
			}
			++_size;
			return (back());
		}

		void push_back(const_reference value) {
			emplace_back(value);
		}

		void push_back(value_type&& value) {
			emplace_back(std::move(value));
		}

#if FASTGLTF_HAS_CONTAINERS_RANGES
		template <std::ranges::input_range R>
		requires std::convertible_to<std::ranges::range_reference_t<R>, value_type>
		void append_range(R&& range) {
			append_range_impl(std::forward<R>(range));
		}
#endif

		void pop_back() {
			assert(!empty());
			traits::destroy(_allocator, std::addressof(back()));
			--_size;
		}

		[[nodiscard]] reference at(size_type idx) {
			if (idx >= size()) {
				raise<std::out_of_range>("idx");
			}
			return begin()[idx];
		}
		[[nodiscard]] const_reference at(size_type idx) const {
			if (idx >= size()) {
				raise<std::out_of_range>("idx");
			}
			return begin()[idx];
		}

		[[nodiscard]] reference operator[](size_type idx) {
			assert(idx < size());
			return begin()[idx];
		}
		[[nodiscard]] const_reference operator[](size_type idx) const {
			assert(idx < size());
			return begin()[idx];
		}

		[[nodiscard]] reference front() {
			assert(!empty());
			return begin()[0];
		}
		[[nodiscard]] const_reference front() const {
			assert(!empty());
			return begin()[0];
		}

		[[nodiscard]] reference back() {
			assert(!empty());
			return end()[-1];
		}
		[[nodiscard]] const_reference back() const {
			assert(!empty());
			return end()[-1];
		}
	};

	template <typename T, std::size_t N, typename Allocator>
	[[nodiscard]] constexpr bool operator==(const small_vector<T, N, Allocator>& lhs, const small_vector<T, N, Allocator>& rhs) {
		return std::ranges::equal(lhs, rhs);
	}

	template <typename T, std::size_t N, typename Allocator>
	[[nodiscard]] constexpr synth_three_way_result<T>
	operator<=>(const small_vector<T, N, Allocator>& lhs, const small_vector<T, N, Allocator>& rhs) {
		return std::lexicographical_compare_three_way(
			lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
			synth_three_way);
	}

#if !FASTGLTF_MISSING_MEMORY_RESOURCE
	namespace pmr {
		FASTGLTF_EXPORT template <typename T, std::size_t N = initial_small_vector_storage>
		using small_vector = small_vector<T, N, std::pmr::polymorphic_allocator<T>>;
	} // namespace pmr
#endif

#ifndef FASTGLTF_USE_CUSTOM_SMALLVECTOR
#define FASTGLTF_USE_CUSTOM_SMALLVECTOR 0
#endif

#if FASTGLTF_USE_CUSTOM_SMALLVECTOR
	FASTGLTF_EXPORT template <typename T, std::size_t N = initial_small_vector_storage>
	using maybe_small_vector = small_vector<T, N>;
#else
	FASTGLTF_EXPORT template <typename T, std::size_t N = 0>
	using maybe_small_vector = std::vector<T>;
#endif

#if !FASTGLTF_MISSING_MEMORY_RESOURCE
	namespace pmr {
#if FASTGLTF_USE_CUSTOM_SMALLVECTOR
		FASTGLTF_EXPORT template <typename T, std::size_t N = initial_small_vector_storage>
		using maybe_small_vector = pmr::small_vector<T, N>;
#else
		FASTGLTF_EXPORT template <typename T, std::size_t N = 0>
		using maybe_small_vector = std::pmr::vector<T>;
#endif
	} // namespace pmr
#endif
} // namespace fastgltf

#endif
