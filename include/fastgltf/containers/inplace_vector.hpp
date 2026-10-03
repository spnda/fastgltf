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

#ifndef FASTGLTF_INPLACE_VECTOR_HPP
#define FASTGLTF_INPLACE_VECTOR_HPP

#if !defined(FASTGLTF_MODULE)
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <new>
#include <memory>
#include <ranges>
#include <stdexcept>
#endif

#include <fastgltf/util.hpp>

namespace fastgltf {
	/**
	 * Similar idea to C++26's std::inplace_vector, or eastl::fixed_vector.
	 * Not a full reimplementation of either, but enough to use for now.
	 */
	FASTGLTF_EXPORT template <typename T, std::size_t N>
	class inplace_vector {
		static_assert(N != 0, "Can't create an inplace_vector with no storage");

	public:
		using value_type = T;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;

		using reference = value_type&;
		using const_reference = const value_type&;

		using pointer = value_type*;
		using const_pointer = const value_type*;

		using iterator = T*;
		using const_iterator = const T*;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	private:
		using size_storage_type =
			std::conditional_t<N <= std::numeric_limits<std::uint8_t>::max(), std::uint8_t,
			std::conditional_t<N <= std::numeric_limits<std::uint16_t>::max(), std::uint16_t,
			std::conditional_t<N <= std::numeric_limits<std::uint32_t>::max(), std::uint32_t,
			std::size_t>>>;

		size_storage_type _size = 0;
		alignas(T) std::byte storage[N * sizeof(T)];

		/**
		 * Appends all elements from [first, last), and raises std::bad_alloc if they don't fit.
		 * For forward iterators the capacity is checked before anything is constructed.
		 */
		template <typename Iterator, typename Sentinel>
		void append_iterators(Iterator first, Sentinel last) {
			if constexpr (std::forward_iterator<Iterator>) {
				const auto n = static_cast<size_type>(std::ranges::distance(first, last));
				if (n > max_size() - size()) [[unlikely]]
					raise<std::bad_alloc>();
				std::ranges::uninitialized_copy_n(std::move(first), static_cast<std::iter_difference_t<Iterator>>(n), end(), end() + n);
				_size = static_cast<size_storage_type>(_size + n);
			} else {
				while (first != last) {
					emplace_back(*first);
					++first;
				}
			}
		}

	public:
		inplace_vector() noexcept = default;

		explicit inplace_vector(const size_type count) {
			if (count > max_size()) [[unlikely]]
				raise<std::bad_alloc>();
			std::uninitialized_value_construct_n(begin(), count);
			_size = count;
		}

		explicit inplace_vector(for_overwrite_t, const size_type count)
		requires std::is_trivially_default_constructible_v<T> {
			if (count > max_size()) [[unlikely]]
				raise<std::bad_alloc>();
			std::uninitialized_default_construct_n(begin(), count);
			_size = count;
		}

		inplace_vector(const size_type count, const T& value) {
			if (count > max_size()) [[unlikely]]
				raise<std::bad_alloc>();
			std::uninitialized_fill_n(begin(), count, value);
			_size = count;
		}

		// These delegate to the default constructor so that the destructor cleans up if emplace_back throws.
		template <std::input_iterator InputIt>
		inplace_vector(InputIt first, InputIt last) : inplace_vector() {
			append_iterators(std::move(first), std::move(last));
		}

#if FASTGLTF_HAS_CONTAINERS_RANGES
		template <std::ranges::input_range R>
		requires std::convertible_to<std::ranges::range_reference_t<R>, T>
		inplace_vector(std::from_range_t, R&& range) : inplace_vector() {
			append_iterators(std::ranges::begin(range), std::ranges::end(range));
		}
#endif

		inplace_vector(const inplace_vector&) requires std::is_trivially_copy_constructible_v<T> = default;
		inplace_vector(const inplace_vector& other) {
			std::uninitialized_copy(other.begin(), other.end(), data());
			_size = other._size;
		}

		inplace_vector(inplace_vector&&) requires std::is_trivially_move_constructible_v<T> = default;
		inplace_vector(inplace_vector&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
			std::uninitialized_move(other.begin(), other.end(), data());
			_size = other._size;
		}

		// The defaulted assignments copy the raw storage, which skips constructing new and destroying old elements.
		inplace_vector& operator=(const inplace_vector&)
			requires (std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_assignable_v<T> && std::is_trivially_destructible_v<T>) = default;
		inplace_vector& operator=(const inplace_vector& other) {
			if (this != std::addressof(other)) [[likely]] {
				const auto common = std::min(size(), other.size());
				std::copy_n(other.begin(), common, begin());
				if (other.size() > size()) {
					std::uninitialized_copy(other.begin() + common, other.end(), end());
				} else {
					std::destroy(begin() + other.size(), end());
				}
				_size = other._size;
			}
			return *this;
		}

		inplace_vector& operator=(inplace_vector&&)
			requires (std::is_trivially_move_constructible_v<T> && std::is_trivially_move_assignable_v<T> && std::is_trivially_destructible_v<T>) = default;
		inplace_vector& operator=(inplace_vector&& other) noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_move_constructible_v<T>) {
			if (this != std::addressof(other)) [[likely]] {
				const auto common = std::min(size(), other.size());
				std::move(other.begin(), other.begin() + common, begin());
				if (other.size() > size()) {
					std::uninitialized_move(other.begin() + common, other.end(), end());
				} else {
					std::destroy(begin() + other.size(), end());
				}
				_size = other._size;
			}
			return *this;
		}

		inplace_vector(std::initializer_list<T> init) {
			if (init.size() > max_size()) [[unlikely]]
				raise<std::bad_alloc>();
			std::uninitialized_copy(init.begin(), init.end(), begin());
			_size = init.size();
		}

		~inplace_vector() requires std::is_trivially_destructible_v<T> = default;
		~inplace_vector() {
			std::destroy(begin(), end());
		}

#if FASTGLTF_HAS_CONTAINERS_RANGES
		template <std::ranges::input_range R>
		requires std::convertible_to<std::ranges::range_reference_t<R>, T>
		void assign_range(R&& range) {
			clear();
			append_iterators(std::ranges::begin(range), std::ranges::end(range));
		}
#endif

		[[nodiscard]] reference at(const size_type pos) {
			if (pos >= size()) [[unlikely]]
				raise<std::out_of_range>("pos");
			return *std::launder(reinterpret_cast<T*>(&storage[pos * sizeof(T)]));
		}
		[[nodiscard]] const_reference at(const size_type pos) const {
			if (pos >= size()) [[unlikely]]
				raise<std::out_of_range>("pos");
			return *std::launder(reinterpret_cast<const T*>(&storage[pos * sizeof(T)]));
		}

		[[nodiscard]] reference operator[](const size_type pos) {
			assert(pos < _size);
			return *std::launder(reinterpret_cast<T*>(&storage[pos * sizeof(T)]));
		}
		[[nodiscard]] const_reference operator[](const size_type pos) const {
			assert(pos < _size);
			return *std::launder(reinterpret_cast<const T*>(&storage[pos * sizeof(T)]));
		}

		[[nodiscard]] reference front() {
			assert(!empty());
			return *begin();
		}
		[[nodiscard]] const_reference front() const {
			assert(!empty());
			return *begin();
		}
		[[nodiscard]] reference back() {
			assert(!empty());
			return *std::prev(end(), 1);
		}
		[[nodiscard]] const_reference back() const {
			assert(!empty());
			return *std::prev(end(), 1);
		}

		[[nodiscard]] pointer data() noexcept {
			return std::launder(reinterpret_cast<T*>(&storage));
		}
		[[nodiscard]] const_pointer data() const noexcept {
			return std::launder(reinterpret_cast<const T*>(&storage));
		}

		[[nodiscard]] iterator begin() noexcept { return data(); }
		[[nodiscard]] const_iterator begin() const noexcept { return data(); }
		[[nodiscard]] const_iterator cbegin() const noexcept { return data(); }
		[[nodiscard]] iterator end() noexcept { return data() + size(); }
		[[nodiscard]] const_iterator end() const noexcept { return data() + size(); }
		[[nodiscard]] const_iterator cend() const noexcept { return data() + size(); }

		[[nodiscard]] reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
		[[nodiscard]] const_reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
		[[nodiscard]] const_reverse_iterator crbegin() const noexcept { return reverse_iterator(end()); }
		[[nodiscard]] reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
		[[nodiscard]] const_reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }
		[[nodiscard]] const_reverse_iterator crend() const noexcept { return reverse_iterator(begin()); }

		[[nodiscard]] bool empty() const noexcept { return size() == 0; }
		[[nodiscard]] size_type size() const noexcept { return _size; }
		[[nodiscard]] static constexpr size_type max_size() noexcept { return N; }
		[[nodiscard]] static constexpr size_type capacity() noexcept { return N; }

		void resize(const std::size_t new_size) {
			if (new_size > max_size()) [[unlikely]]
				raise<std::bad_alloc>();

			if (new_size > size()) {
				std::uninitialized_value_construct_n(end(), new_size - size());
			} else {
				std::destroy(begin() + new_size, end());
			}
			_size = new_size;
		}

		static constexpr void reserve(const size_type new_cap) {
			if (new_cap > capacity()) [[unlikely]]
				raise<std::bad_alloc>();
		}
		static constexpr void shrink_to_fit() noexcept {
			// Only exists for compatibility
		}

		template <typename... Args>
		reference emplace_back(Args&&... args) {
			if (size() == max_size()) [[unlikely]]
				raise<std::bad_alloc>();
			return unchecked_emplace_back(std::forward<Args>(args)...);
		}

		/**
		 * std::inplace_vector returns an std::optional<T&> here, but since this needs to be compatible with
		 * C++20 and C++23 we have to stick to the proposal's initial signature which returned a pointer, with
		 * a nullptr indicating failure.
		 */
		template <typename... Args>
		pointer try_emplace_back(Args&&... args) {
			if (size() == max_size()) [[unlikely]]
				return nullptr;
			return &unchecked_emplace_back(std::forward<Args>(args)...);
		}

		template <typename... Args>
		reference unchecked_emplace_back(Args&&... args) {
			assert(size() < max_size());
			const auto i = _size;
			std::construct_at(reinterpret_cast<T*>(&storage[i * sizeof(T)]), std::forward<Args>(args)...);
			++_size;
			return (*this)[i];
		}

		reference push_back(const T& value) {
			if (size() == max_size()) [[unlikely]]
				raise<std::bad_alloc>();
			return unchecked_push_back(value);
		}

		/**
		 * std::inplace_vector returns an std::optional<T&> here, but since this needs to be compatible with
		 * C++20 and C++23 we have to stick to the proposal's initial signature which returned a pointer, with
		 * a nullptr indicating failure.
		 */
		pointer try_push_back(const T& value) {
			if (size() == max_size()) [[unlikely]]
				return nullptr;
			return &unchecked_push_back(value);
		}

		reference unchecked_push_back(const T& value) {
			assert(size() < max_size());
			const auto i = _size;
			std::construct_at(reinterpret_cast<T*>(&storage[i * sizeof(T)]), value);
			++_size;
			return (*this)[i];
		}

		void clear() noexcept {
			std::destroy(begin(), end());
			_size = 0;
		}
	};
}

#endif
