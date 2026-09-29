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

#ifndef FASTGLTF_ALLOCATOR_UTILS_HPP
#define FASTGLTF_ALLOCATOR_UTILS_HPP

#if !defined(FASTGLTF_MODULE)
#include <cstring>
#include <iterator>
#include <memory>
#endif

#include <fastgltf/containers/exception_guard.hpp>

namespace fastgltf::internal {
	template <typename Allocator, typename Iterator, typename Sentinel>
	void allocator_destroy(Allocator& a, Iterator first, Sentinel last) {
		while (first != last) {
			std::allocator_traits<Allocator>::destroy(a, std::addressof(*first));
			++first;
		}
	}

	template <typename Allocator, typename Iterator, typename... Args>
	Iterator allocator_construct(Allocator& a, Iterator first, Iterator last, const Args&... args) {
		const auto destruct_last = first;
		auto guard = make_exception_guard([&] {
			allocator_destroy(a, std::reverse_iterator(first), std::reverse_iterator(destruct_last));
		});
		while (first != last) {
			std::allocator_traits<Allocator>::construct(a, std::to_address(first), args...);
			++first;
		}
		guard.complete();
		return first;
	}

	template <typename Allocator, typename Iterator, typename... Args>
	Iterator allocator_construct_n(Allocator& a, Iterator first, const std::size_t count, const Args&... args) {
		return allocator_construct(a, first, first + count, args...);
	}

	template <typename Allocator, typename Iterator, typename OutputIterator>
	std::pair<Iterator, OutputIterator> uninitialized_allocator_copy_n(
		Allocator& a, Iterator first, std::size_t n, OutputIterator out) {

		using T = std::iter_value_t<OutputIterator>;
		if constexpr (std::contiguous_iterator<Iterator> && std::contiguous_iterator<OutputIterator>
				&& std::same_as<std::remove_cv_t<std::iter_value_t<Iterator>>, T>
				&& std::is_trivially_copyable_v<T> && !std::uses_allocator_v<T, Allocator>) {

			if (n != 0)
				std::memcpy(std::to_address(out), std::to_address(first), n * sizeof(T));
			return { first + n, out + n };
		} else {
			const auto destruct_last = out;
			auto guard = make_exception_guard([&] {
				allocator_destroy(a, std::reverse_iterator(out), std::reverse_iterator(destruct_last));
			});

			while (n > 0) {
				std::allocator_traits<Allocator>::construct(a, std::to_address(out), *first);
				--n;
				++first;
				++out;
			}

			guard.complete();
			return { first, out };
		}
	}

	template <typename Allocator, typename Iterator, typename Sentinel, typename OutputIterator>
	std::pair<Iterator, OutputIterator> uninitialized_allocator_copy(Allocator& a, Iterator first, Sentinel last, OutputIterator out) {
		const auto destruct_last = out;
		auto guard = make_exception_guard([&] {
			allocator_destroy(a, std::reverse_iterator(out), std::reverse_iterator(destruct_last));
		});

		while (first != last) {
			std::allocator_traits<Allocator>::construct(a, std::to_address(out), *first);
			++first;
			++out;
		}

		guard.complete();
		return { first, out };
	}

	template <typename Allocator, std::contiguous_iterator Iterator>
	void uninitialized_allocator_relocate(Allocator& a, Iterator first, Iterator last, Iterator result) {
		using value_type = std::iterator_traits<Iterator>::value_type;
		if constexpr (std::is_trivially_copyable_v<value_type> && !std::uses_allocator_v<value_type, Allocator>) {
			std::memcpy(std::to_address(result), std::to_address(first),
				sizeof(value_type) * static_cast<std::size_t>(std::distance(first, last)));
		} else {
			auto destruct_last = result;
			auto guard = make_exception_guard([&] {
				allocator_destroy(a, std::reverse_iterator(result), std::reverse_iterator(destruct_last));
			});

			auto it = first;
			while (it != last) {
#if __cpp_exceptions
				std::allocator_traits<Allocator>::construct(a, std::to_address(result), std::move_if_noexcept(*it));
#else
				std::allocator_traits<Allocator>::construct(a, std::to_address(result), std::move(*it));
#endif
				++it;
				++result;
			}

			guard.complete();
			allocator_destroy(a, first, last);
		}
	}
}

#endif
