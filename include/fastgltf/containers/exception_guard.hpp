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

#ifndef FASTGLTF_EXCEPTION_GUARD_HPP
#define FASTGLTF_EXCEPTION_GUARD_HPP

#if !defined(FASTGLTF_MODULE)
#include <cassert>
#include <type_traits>
#include <utility>
#endif

namespace fastgltf {
	// exception_guard is a helper class for writing code with the strong exception guarantee, such as containers like
	// small_vector, static_vector, and inplace_vector. This implementation is largely copied from libc++.

	template <typename Rollback>
	class exception_guard_exceptions {
		Rollback rollback;
		bool completed = false;

	public:
		exception_guard_exceptions() = delete;

		[[gnu::nodebug]] constexpr explicit exception_guard_exceptions(Rollback rollback) : rollback(std::move(rollback)) {}

		[[gnu::nodebug]] constexpr exception_guard_exceptions(exception_guard_exceptions&& other)
			noexcept(std::is_nothrow_move_constructible_v<Rollback>)
			: rollback(std::move(other.rollback)), completed(other.completed) {

			other.completed = true;
		}

		exception_guard_exceptions(const exception_guard_exceptions&) = delete;
		exception_guard_exceptions& operator=(const exception_guard_exceptions&) = delete;
		exception_guard_exceptions& operator=(exception_guard_exceptions&&) = delete;

		[[gnu::nodebug]] constexpr ~exception_guard_exceptions() {
			if (!completed)
				rollback();
		}

		[[gnu::nodebug]] constexpr void complete() noexcept {
			completed = true;
		}
	};

	template <typename Rollback>
	class exception_guard_noexceptions {
		bool completed = false;

	public:
		exception_guard_noexceptions() = delete;

		[[gnu::nodebug]] constexpr explicit exception_guard_noexceptions(Rollback) {}

		[[gnu::nodebug]] constexpr exception_guard_noexceptions(exception_guard_noexceptions&& other)
			noexcept(std::is_nothrow_move_constructible_v<Rollback>)
			: completed(other.completed) {

			other.completed = true;
		}

		exception_guard_noexceptions(const exception_guard_noexceptions&) = delete;
		exception_guard_noexceptions& operator=(const exception_guard_noexceptions&) = delete;
		exception_guard_noexceptions& operator=(exception_guard_noexceptions&&) = delete;

		[[gnu::nodebug]] constexpr ~exception_guard_noexceptions() {
			assert(completed);
		}

		[[gnu::nodebug]] constexpr void complete() noexcept {
			completed = true;
		}
	};

#ifdef __cpp_exceptions
	template <typename Rollback>
	using exception_guard [[gnu::nodebug]] = exception_guard_exceptions<Rollback>;
#else
	template <typename Rollback>
	using exception_guard [[gnu::nodebug]] = exception_guard_noexceptions<Rollback>;
#endif

	template <typename Rollback>
	constexpr exception_guard<Rollback> make_exception_guard(Rollback rollback) {
		return exception_guard<Rollback>(std::move(rollback));
	}
}

#endif
