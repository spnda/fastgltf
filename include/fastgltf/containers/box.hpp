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

#ifndef FASTGLTF_BOX_HPP
#define FASTGLTF_BOX_HPP

#if !defined(FASTGLTF_MODULE)
#include <memory>
#include <memory_resource>
#endif

#include <fastgltf/util.hpp>
#include <fastgltf/containers/exception_guard.hpp>

namespace fastgltf {
	/**
	 * A wrapper for a dynamically allocated object with value-like semantics, similar to std::indirect (C++26).
	 */
	FASTGLTF_EXPORT template <typename T, typename Allocator = std::allocator<T>>
	requires std::is_object_v<T> &&
		(!std::is_array_v<T>) &&
		(!std::same_as<T, std::in_place_t>) &&
		(!std::is_const_v<T>) && (!std::is_volatile_v<T>) &&
		std::same_as<typename std::allocator_traits<Allocator>::value_type, T>
	class box {
		using traits = std::allocator_traits<Allocator>;

	public:
		using value_type = T;
		using allocator_type = Allocator;
		using pointer = typename traits::pointer;
		using const_pointer = typename traits::const_pointer;

	private:
		FASTGLTF_NO_UNIQUE_ADDRESS Allocator _allocator;
		pointer _ptr = nullptr;

		template <typename... Args>
		static constexpr pointer create(Allocator& a, Args&&... args) {
			pointer ptr = traits::allocate(a, 1);
			auto guard = make_exception_guard([&] {
				traits::deallocate(a, ptr, 1);
			});
			traits::construct(a, ptr, std::forward<Args>(args)...);
			guard.complete();
			return ptr;
		}
		static constexpr void dispose(Allocator& a, pointer& p) noexcept {
			traits::destroy(a, p);
			traits::deallocate(a, p, 1);
			p = nullptr;
		}

	public:
		constexpr explicit box()
		requires std::is_default_constructible_v<Allocator>
			: _ptr(create(_allocator)) {}
		constexpr explicit box(std::allocator_arg_t, const Allocator& a)
			: _allocator(a), _ptr(create(_allocator)) {
			static_assert(std::is_default_constructible_v<T>);
		}

		template <typename U = T>
		requires std::is_constructible_v<T, U> && std::is_default_constructible_v<Allocator> &&
			is_none_of_v<std::remove_cvref_t<U>, box, std::in_place_t>
		constexpr explicit box(U&& v)
			: _ptr(create(_allocator, std::forward<U>(v))) {}
		template <typename U = T>
		requires std::is_constructible_v<T, U> &&
			is_none_of_v<std::remove_cvref_t<U>, box, std::in_place_t>
		constexpr explicit box(std::allocator_arg_t, const Allocator& a, U&& v)
			: _allocator(a), _ptr(create(_allocator, std::forward<U>(v))) {}

		template <typename... Args>
		requires std::is_constructible_v<T, Args...> && std::is_default_constructible_v<Allocator>
		constexpr explicit box(std::in_place_t, Args&&... args)
			: _ptr(create(_allocator, std::forward<Args>(args)...)) {}
		template <typename... Args>
		requires std::is_constructible_v<T, Args...>
		constexpr explicit box(std::allocator_arg_t, const Allocator& a, std::in_place_t, Args&&... args)
			: _allocator(a), _ptr(create(_allocator, std::forward<Args>(args)...)) {}

		template <typename I, typename... Args>
		requires std::is_constructible_v<T, std::initializer_list<I>&, Args...> &&
			std::is_default_constructible_v<Allocator>
		constexpr explicit box(std::in_place_t, std::initializer_list<I> ilist, Args&&... args)
			: _ptr(create(_allocator, ilist, std::forward<Args>(args)...)) {}
		template <typename I, typename... Args>
		requires std::is_constructible_v<T, std::initializer_list<I>&, Args...>
		constexpr explicit box(std::allocator_arg_t, const Allocator& a,
			std::in_place_t, std::initializer_list<I> ilist, Args&&... args)
			: _allocator(a), _ptr(create(_allocator, ilist, std::forward<Args>(args)...)) {}

		constexpr box(const box& other)
			: box(std::allocator_arg, traits::select_on_container_copy_construction(other._allocator), other) {}
		constexpr box(std::allocator_arg_t, const Allocator& a, const box& other)
			: _allocator(a) {
			static_assert(std::is_copy_constructible_v<T>);
			if (other._ptr != nullptr) {
				_ptr = create(_allocator, *other);
			}
		}

		constexpr box(box&& other) noexcept
		: _allocator(std::move(other._allocator)), _ptr(std::exchange(other._ptr, nullptr)) {}
		constexpr box(std::allocator_arg_t, const Allocator& a, box&& other)
		noexcept(traits::is_always_equal::value)
			: _allocator(a) {
			static_assert(traits::is_always_equal::value || std::is_move_constructible_v<T>);
			if (other._ptr != nullptr) {
				if (_allocator == other._allocator) {
					_ptr = std::exchange(other._ptr, nullptr);
				} else {
					_ptr = create(_allocator, *std::move(other));

					dispose(other._allocator, other._ptr);
				}
			}
		}

		constexpr ~box() noexcept {
			if (_ptr != nullptr) {
				dispose(_allocator, _ptr);
			}
		}

		constexpr box& operator=(const box& other) {
			static_assert(std::is_copy_assignable_v<T> && std::is_copy_constructible_v<T>);
			if (std::addressof(other) != this) {
				constexpr auto need_update = traits::propagate_on_container_copy_assignment::value;
				if (other._ptr == nullptr) {
					if (_ptr != nullptr) {
						dispose(_allocator, _ptr);
					}
				} else if (_allocator == other._allocator && _ptr != nullptr) {
					**this = *other;
				} else {
					pointer ptr = create(need_update ? other._allocator : _allocator, *other);

					if (_ptr != nullptr) {
						dispose(_allocator, _ptr);
					}
					_ptr = ptr;
				}

				if constexpr (need_update)
					_allocator = other._allocator;
			}
			return *this;
		}
		constexpr box& operator=(box&& other)
		noexcept(traits::propagate_on_container_move_assignment::value || traits::is_always_equal::value) {
			if (std::addressof(other) != this) {
				constexpr auto need_update = traits::propagate_on_container_move_assignment::value;
				if (other._ptr == nullptr) {
					if (_ptr != nullptr) {
						dispose(_allocator, _ptr);
					}
				} else if (need_update || _allocator == other._allocator) {
					if (_ptr != nullptr) {
						dispose(_allocator, _ptr);
					}
					_ptr = std::exchange(other._ptr, nullptr);
				} else {
					pointer ptr = create(_allocator, *std::move(other));

					dispose(other._allocator, other._ptr);

					if (_ptr != nullptr) {
						dispose(_allocator, _ptr);
					}
					_ptr = ptr;
				}

				if constexpr (need_update)
					_allocator = other._allocator;
			}
			return *this;
		}
		template <typename U = T>
		requires (!std::same_as<std::remove_cvref_t<U>, box>) && std::is_constructible_v<T, U> && std::is_assignable_v<T&, U>
		constexpr box& operator=(U&& value) {
			if (_ptr == nullptr) {
				_ptr = create(_allocator, std::forward<U>(value));
			} else {
				**this = std::forward<U>(value);
			}
			return *this;
		}

		constexpr const_pointer operator->() const noexcept {
			return _ptr;
		}
		constexpr pointer operator->() noexcept {
			return _ptr;
		}

		constexpr const T& operator*() const& noexcept {
			return *_ptr;
		}
		constexpr T& operator*() & noexcept {
			return *_ptr;
		}

		constexpr const T&& operator*() const&& noexcept {
			return std::move(*_ptr);
		}
		constexpr T&& operator*() && noexcept {
			return std::move(*_ptr);
		}

		[[nodiscard]] constexpr bool valueless_after_move() const noexcept {
			return _ptr == nullptr;
		}

		[[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
			return _allocator;
		}

		constexpr void swap(box& other) noexcept(traits::propagate_on_container_swap::value || traits::is_always_equal::value) {
			using std::swap;
			if constexpr (traits::propagate_on_container_swap::value) {
				swap(_allocator, other._allocator);
			} else {
				assert(_allocator == other._allocator);
			}
			swap(_ptr, other._ptr);
		}

		friend constexpr void swap(box& lhs, box& rhs) noexcept(noexcept(lhs.swap(rhs))) {
			lhs.swap(rhs);
		}
	};

	namespace pmr {
		FASTGLTF_EXPORT template< class T >
		using box = box<T, std::pmr::polymorphic_allocator<T>>;
	}

	FASTGLTF_EXPORT template <typename T, typename A1, typename U, typename A2>
	[[nodiscard]] constexpr bool operator==(const box<T, A1>& lhs, const box<U, A2>& rhs) noexcept(noexcept(*lhs == *rhs)) {
		if (lhs.valueless_after_move() || rhs.valueless_after_move())
			return lhs.valueless_after_move() == rhs.valueless_after_move();
		return *lhs == *rhs;
	}
	FASTGLTF_EXPORT template <typename T, typename A1, typename U, typename A2>
	[[nodiscard]] constexpr synth_three_way_result<T, U> operator<=>(const box<T, A1>& lhs, const box<U, A2>& rhs) {
		if (lhs.valueless_after_move() || rhs.valueless_after_move()) {
			return !lhs.valueless_after_move() <=> !rhs.valueless_after_move();
		}
		return synth_three_way(*lhs, *rhs);
	}
	FASTGLTF_EXPORT template <typename T, typename A, typename U>
	[[nodiscard]] constexpr bool operator==(const box<T, A>& lhs, const U& rhs) noexcept(noexcept(*lhs == rhs)) {
		return !lhs.valueless_after_move() && *lhs == rhs;
	}
	FASTGLTF_EXPORT template <typename T, typename A, typename U>
	[[nodiscard]] constexpr synth_three_way_result<T, U> operator<=>(const box<T, A>& lhs, const U& rhs) {
		if (lhs.valueless_after_move())
			return std::strong_ordering::less;
		return synth_three_way(*lhs, rhs);
	}

	template <typename Value>
	box(Value) -> box<Value>;

	template <typename Allocator, typename Value>
	box(std::allocator_arg_t, Allocator, Value) -> box<Value, typename std::allocator_traits<Allocator>::template rebind_alloc<Value>>;
}

template <typename T, typename Allocator>
requires std::is_default_constructible_v<std::hash<T>>
struct std::hash<fastgltf::box<T, Allocator>> {
	[[nodiscard]] constexpr std::size_t operator()(const fastgltf::box<T, Allocator>& x) const
		noexcept(noexcept(std::hash<T>{}(std::declval<const T&>()))) {
		return !x.valueless_after_move() ? std::hash<T>{}(*x) : 0;
	}
};

#endif
