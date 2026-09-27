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

#if !defined(FASTGLTF_USE_STD_MODULE) || !FASTGLTF_USE_STD_MODULE
#include <algorithm>
#include <cassert>
#include <vector>
#endif

#include <fastgltf/util.hpp>

namespace fastgltf {
	/**
	 * A static vector which cannot be resized freely. When constructed, the backing array is allocated once.
	 */
	FASTGLTF_EXPORT template <typename T>
	class StaticVector final {
	public:
		using value_type = T;
		using size_type = std::size_t;
		using array_t = value_type[];

	private:
		size_type _size = 0;
		std::unique_ptr<array_t> _array;

		void copy(const T* first, const size_type count, T* result) {
			if (count > 0) {
				if constexpr (std::is_trivially_copyable_v<T>) {
					std::memcpy(result, first, count * sizeof(T));
				} else {
					*result++ = *first;
					for (size_type i = 1; i < count; ++i) {
						*result++ = *++first;
					}
				}
			}
		}

	public:
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = value_type*;
		using const_pointer = const value_type*;
		using iterator = pointer;
		using const_iterator = const_pointer;

        explicit StaticVector(std::size_t size) : _size(size), _array(std::make_unique_for_overwrite<array_t>(size)) {}
		explicit StaticVector(std::size_t size, const T& initialValue) : _size(size), _array(std::make_unique_for_overwrite<array_t>(size)) {
			for (auto& value : *this) {
				value = initialValue;
			}
		}

		StaticVector(const StaticVector& other) {
			if (other.size() == 0) {
				_array.reset();
				_size = 0;
			} else {
				_array.reset(new std::remove_extent_t<array_t>[other.size()]);
				_size = other.size();
				copy(other.begin(), _size, begin());
			}
		}

		StaticVector(StaticVector&& other) noexcept {
			_array = std::move(other._array);
			_size = other.size();
		}

		StaticVector& operator=(StaticVector&& other) noexcept {
			_array = std::move(other._array);
			_size = other.size();
			return *this;
		}

		/**
		 * Copies the contents of the given vector into a new StaticVector.
		 */
		static auto fromVector(const std::vector<T>& vector) {
			StaticVector staticVector(vector.size());
			std::ranges::copy(vector.begin(), vector.end(), staticVector.begin());
			return staticVector;
		}

		[[nodiscard]] pointer data() noexcept {
			return &_array.get()[0];
		}

		[[nodiscard]] const_pointer data() const noexcept {
			return &_array.get()[0];
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

		constexpr bool operator==(const StaticVector& other) const {
			return size() == other.size() && std::equal(begin(), end(), other.begin());
		}
		constexpr auto operator<=>(const StaticVector& other) const {
			return std::lexicographical_compare_three_way(
				begin(), end(), other.begin(), other.end(),
				[]<typename U, typename V>(const U& u, const V& v) {
				if constexpr (std::three_way_comparable_with<U, V>) {
					return u <=> v;
				} else {
					if (u < v) return std::weak_ordering::less;
					if (v < u) return std::weak_ordering::greater;
					return std::weak_ordering::equivalent;
				}
			});
		}

		constexpr bool operator==(const std::vector<value_type>& other) const {
			return size() == other.size() && std::equal(begin(), end(), other.begin());
		}
		constexpr auto operator<=>(const std::vector<value_type>& other) const {
			return std::lexicographical_compare_three_way(
				begin(), end(), other.begin(), other.end(),
				[]<typename U, typename V>(const U& u, const V& v) {
				if constexpr (std::three_way_comparable_with<U, V>) {
					return u <=> v;
				} else {
					if (u < v) return std::weak_ordering::less;
					if (v < u) return std::weak_ordering::greater;
					return std::weak_ordering::equivalent;
				}
			});
		}
	};
} // namespace fastgltf

#endif
