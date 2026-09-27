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

#ifndef FASTGLTF_FLAGGED_OPTIONAL_HPP
#define FASTGLTF_FLAGGED_OPTIONAL_HPP

#include <functional>

#include <fastgltf/util.hpp>

namespace fastgltf {
	FASTGLTF_EXPORT template<typename, typename = void>
	struct OptionalFlagValue {
		static constexpr std::nullopt_t missing_value = std::nullopt;
	};

	template<>
	struct OptionalFlagValue<std::size_t> {
		static constexpr auto missing_value = std::numeric_limits<std::size_t>::max();
	};

	template<>
	struct OptionalFlagValue<float, std::enable_if_t<std::numeric_limits<float>::is_iec559>> {
		// This float is a quiet NaN with a specific bit pattern to be able to differentiate
		// between this flag value and any result from FP operations.
		static constexpr auto missing_value = std::bit_cast<float>(0x7fedb6db);
	};

	template<>
	struct OptionalFlagValue<double, std::enable_if_t<std::numeric_limits<double>::is_iec559>> {
		static constexpr auto missing_value = std::bit_cast<double>(0x7ffdb6db6db6db6d);
	};

	FASTGLTF_EXPORT template<typename T>
	class OptionalWithFlagValue;

	namespace internal {
		/** Excludes optional types and std::nullopt_t from the comparison operators taking a plain value, like std::optional does. */
		template <typename T>
		inline constexpr bool is_optional_impl_v = std::is_same_v<T, std::nullopt_t>;
		template <typename T>
		inline constexpr bool is_optional_impl_v<OptionalWithFlagValue<T>> = true;
		template <typename T>
		inline constexpr bool is_optional_impl_v<std::optional<T>> = true;

		template <typename T>
		inline constexpr bool is_optional_v = is_optional_impl_v<std::remove_cv_t<T>>;
	} // namespace internal

	/**
	 * A type alias which checks if there is a specialization of OptionalFlagValue for T and "switches"
	 * between fastgltf::OptionalWithFlagValue and std::optional.
	 */
	FASTGLTF_EXPORT template <typename T>
	using Optional = std::conditional_t<
		!std::is_same_v<std::nullopt_t, std::remove_const_t<decltype(OptionalFlagValue<T>::missing_value)>>,
		OptionalWithFlagValue<T>,
		std::optional<T>>;

	/**
	 * A custom optional class for fastgltf,
	 * which uses so-called "flag values" which are specific values of T that will never be present as an actual value.
	 * We can therefore use those values as flags for whether there is an actual value stored,
	 * instead of the additional bool used by std::optional.
	 *
	 * These flag values are obtained from the specializations of OptionalFlagValue.
	 * If no specialization for T of OptionalFlagValue is provided, a static assert will be triggered.
	 * In those cases, use std::optional or fastgltf::Optional instead.
	 */
	template<typename T>
	class OptionalWithFlagValue final {
		static_assert(!std::is_same_v<std::nullopt_t, std::remove_const_t<decltype(OptionalFlagValue<T>::missing_value)>>,
			"OptionalWithFlagValue can only be used when there is an appropriate specialization of OptionalFlagValue<T>.");

		struct NonTrivialDummy {
			constexpr NonTrivialDummy() noexcept {}
		};

		union {
			NonTrivialDummy dummy;
			std::remove_const_t<T> _value;
		};

	public:
		constexpr OptionalWithFlagValue() noexcept { reset(); }

		constexpr OptionalWithFlagValue(std::nullopt_t) noexcept { reset(); }

		static_assert(std::is_trivially_copyable_v<T>, "OptionalWithFlagValue only supports trivially copyable types.");

		constexpr OptionalWithFlagValue(const OptionalWithFlagValue&) = default;
		constexpr OptionalWithFlagValue(OptionalWithFlagValue&&) = default;
		constexpr OptionalWithFlagValue& operator=(const OptionalWithFlagValue&) = default;
		constexpr OptionalWithFlagValue& operator=(OptionalWithFlagValue&&) = default;

		template <typename U = T>
		requires std::is_copy_constructible_v<T>
		constexpr OptionalWithFlagValue(const OptionalWithFlagValue<U>& other) {
			if (other.has_value()) {
				std::construct_at(std::addressof(_value), *other);
			} else {
				reset();
			}
		}

		template <typename U = T>
		requires std::is_move_constructible_v<T>
		constexpr OptionalWithFlagValue(OptionalWithFlagValue<U>&& other) {
			if (other.has_value()) {
				std::construct_at(std::addressof(_value), std::move(*other));
			} else {
				reset();
			}
		}

		template<typename... Args>
		requires std::is_constructible_v<T, Args...>
		constexpr explicit OptionalWithFlagValue(std::in_place_t, Args&&... args) noexcept(
			std::is_nothrow_constructible_v<T, Args...>)
			: _value(std::forward<Args>(args)...) {}

		template <typename U = T>
		requires std::is_constructible_v<T, U&&>
		constexpr OptionalWithFlagValue(U&& _new) noexcept(std::is_nothrow_assignable_v<T&, U> &&
														   std::is_nothrow_constructible_v<T, U>) {
			std::construct_at(std::addressof(_value), std::forward<U>(_new));
		}

		constexpr ~OptionalWithFlagValue() { reset(); }

		constexpr OptionalWithFlagValue& operator=(std::nullopt_t) noexcept {
			reset();
			return *this;
		}

		template <typename U = T>
		requires std::is_constructible_v<T, U&&>
		constexpr OptionalWithFlagValue& operator=(U&& _new) noexcept(std::is_nothrow_assignable_v<T&, U> && std::is_nothrow_constructible_v<T, U>) {
			if (has_value()) {
				_value = std::forward<U>(_new);
			} else {
				std::construct_at(std::addressof(_value), std::forward<U>(_new));
			}
			return *this;
		}

		template <typename U>
		requires std::conjunction_v<std::is_constructible<T, const U&>,
									std::is_assignable<T&, const U&>>
		constexpr OptionalWithFlagValue& operator=(const OptionalWithFlagValue<U>& other) {
			if (other.has_value()) {
				if (has_value()) {
					_value = *other;
				} else {
					std::construct_at(std::addressof(_value), *other);
				}
			} else {
				reset();
			}
			return *this;
		}

		template <typename U>
		requires std::conjunction_v<std::is_constructible<T, U>, std::is_assignable<T&, U>>
		constexpr OptionalWithFlagValue& operator=(OptionalWithFlagValue<U>&& other) noexcept(
		std::is_nothrow_assignable_v<T&, T> && std::is_nothrow_constructible_v<T, T>) {
			if (other.has_value()) {
				if (has_value()) {
					_value = std::move(*other);
				} else {
					std::construct_at(std::addressof(_value), std::move(*other));
				}
			} else {
				reset();
			}
			return *this;
		}

		[[nodiscard]] constexpr bool has_value() const {
			if constexpr (std::is_floating_point_v<T>) {
				// The sentinels are NaNs, which never compare equal, so compare the bit patterns instead.
				using Bits = std::conditional_t<sizeof(T) == sizeof(std::uint32_t), std::uint32_t, std::uint64_t>;
				static_assert(sizeof(Bits) == sizeof(T));
				return std::bit_cast<Bits>(_value) != std::bit_cast<Bits>(OptionalFlagValue<T>::missing_value);
			}
			return this->_value != OptionalFlagValue<T>::missing_value;
		}

		[[nodiscard]] constexpr T& value() & {
			if (!has_value()) {
				raise<std::bad_optional_access>();
			}
			return _value;
		}

		[[nodiscard]] constexpr const T& value() const& {
			if (!has_value()) {
				raise<std::bad_optional_access>();
			}
			return _value;
		}

		[[nodiscard]] constexpr T&& value() && {
			if (!has_value()) {
				raise<std::bad_optional_access>();
			}
			return std::move(_value);
		}

		[[nodiscard]] constexpr const T&& value() const&& {
			if (!has_value()) {
				raise<std::bad_optional_access>();
			}
			return std::move(_value);
		}

		template <typename U = std::remove_cv_t<T>>
		[[nodiscard]] constexpr T value_or(U&& default_value) const& {
			return has_value() ? **this : static_cast<T>(std::forward<U>(default_value));
		}

		template <typename U = std::remove_cv_t<T>>
		[[nodiscard]] constexpr T value_or(U&& default_value) && {
			return has_value() ? std::move(**this) : static_cast<T>(std::forward<U>(default_value));
		}

		template <typename F>
		[[nodiscard]] constexpr auto and_then(F&& func) & {
			using U = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
			if (!has_value())
				return U(std::nullopt);
			return std::invoke(std::forward<F>(func), **this);
		}

		template <typename F>
		[[nodiscard]] constexpr auto and_then(F&& func) const& {
			using U = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
			if (!has_value())
				return U(std::nullopt);
			return std::invoke(std::forward<F>(func), **this);
		}

		template <typename F>
		[[nodiscard]] constexpr auto and_then(F&& func) && {
			using U = std::remove_cvref_t<std::invoke_result_t<F, T>>;
			if (!has_value())
				return U(std::nullopt);
			return std::invoke(std::forward<F>(func), std::move(**this));
		}

		template <typename F>
		[[nodiscard]] constexpr auto and_then(F&& func) const&& {
			using U = std::remove_cvref_t<std::invoke_result_t<F, const T>>;
			if (!has_value())
				return U(std::nullopt);
			return std::invoke(std::forward<F>(func), std::move(**this));
		}

		template <typename F>
		[[nodiscard]] constexpr auto transform(F&& func) & {
			using U = std::remove_cv_t<std::invoke_result_t<F, T&>>;
			if (!has_value())
				return Optional<U>(std::nullopt);
			return Optional<U>(std::invoke(std::forward<F>(func), **this));
		}

		template <typename F>
		[[nodiscard]] constexpr auto transform(F&& func) const& {
			using U = std::remove_cv_t<std::invoke_result_t<F, const T&>>;
			if (!has_value())
				return Optional<U>(std::nullopt);
			return Optional<U>(std::invoke(std::forward<F>(func), **this));
		}

		template <typename F>
		[[nodiscard]] constexpr auto transform(F&& func) && {
			using U = std::remove_cv_t<std::invoke_result_t<F, T>>;
			if (!has_value())
				return Optional<U>(std::nullopt);
			return Optional<U>(std::invoke(std::forward<F>(func), std::move(**this)));
		}

		template <typename F>
		[[nodiscard]] constexpr auto transform(F&& func) const&& {
			using U = std::remove_cv_t<std::invoke_result_t<F, const T>>;
			if (!has_value())
				return Optional<U>(std::nullopt);
			return Optional<U>(std::invoke(std::forward<F>(func), std::move(**this)));
		}

		template <typename F>
		[[nodiscard]] constexpr Optional<T> or_else(F&& func) const& {
			return *this ? *this : std::invoke(std::forward<F>(func));
		}

		template <typename F>
		[[nodiscard]] constexpr Optional<T> or_else(F&& func) && {
			return *this ? std::move(*this) : std::invoke(std::forward<F>(func));
		}

		constexpr void swap(OptionalWithFlagValue<T>& other) noexcept(
			std::is_nothrow_move_constructible_v<T> && std::is_nothrow_swappable_v<T>) {
			static_assert(std::is_move_constructible_v<T>);
			if (has_value() && other.has_value()) {
				std::swap(_value, other._value);
			} else if (has_value() && !other.has_value()) {
				other._value = std::move(_value);
				reset();
			} else if (!has_value() && other.has_value()) {
				_value = std::move(other._value);
				other.reset();
			}
		}

		constexpr void reset() noexcept { this->_value = OptionalFlagValue<T>::missing_value; }

		template <typename... Args>
		constexpr T& emplace(Args&&... args) {
			std::construct_at(std::addressof(_value), std::forward<Args>(args)...);
			return _value;
		}

		template <typename U, typename... Args>
		constexpr T& emplace(std::initializer_list<U> list, Args&&... args) {
			static_assert(std::is_constructible_v<T, std::initializer_list<U>&, Args&&...>);
			std::construct_at(std::addressof(_value), list, std::forward<Args>(args)...);
			return _value;
		}

		constexpr explicit operator bool() const noexcept { return has_value(); }

		constexpr T* operator->() noexcept { return std::addressof(_value); }

		constexpr const T* operator->() const noexcept { return std::addressof(_value); }

		constexpr T& operator*() & noexcept { return _value; }

		constexpr const T& operator*() const& noexcept { return _value; }

		constexpr T&& operator*() && noexcept { return std::move(_value); }

		constexpr const T&& operator*() const&& noexcept { return std::move(_value); }

		operator std::optional<T>() const& noexcept {
			return has_value() ? std::optional<T>(_value) : std::nullopt;
		}

		operator std::optional<T>() && noexcept {
			return has_value() ? std::optional<T>(std::move(_value)) : std::nullopt;
		}
	};

	// Containers like std::vector only move elements when reallocating if the move constructor is noexcept
	static_assert(std::is_nothrow_move_constructible_v<OptionalWithFlagValue<std::size_t>>);
	static_assert(std::is_nothrow_move_constructible_v<OptionalWithFlagValue<float>>);

	FASTGLTF_EXPORT template <typename T, typename U>
	constexpr bool operator==(const OptionalWithFlagValue<T>& lhs,
							  const OptionalWithFlagValue<U>& rhs) {
		return lhs.has_value() != rhs.has_value() ? false
												  : (lhs.has_value() == false ? true : *lhs == *rhs);
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	constexpr bool operator<(const OptionalWithFlagValue<T>& lhs, const OptionalWithFlagValue<U>& rhs) {
		return !rhs ? false : (!lhs ? true : *lhs < *rhs);
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	constexpr bool operator<=(const OptionalWithFlagValue<T>& lhs,
							  const OptionalWithFlagValue<U>& rhs) {
		return !lhs ? true : (!rhs ? false : *lhs <= *rhs);
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	constexpr bool operator>(const OptionalWithFlagValue<T>& lhs, const OptionalWithFlagValue<U>& rhs) {
		return !lhs ? false : (!rhs ? true : *lhs > *rhs);
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	constexpr bool operator>=(const OptionalWithFlagValue<T>& lhs,
							  const OptionalWithFlagValue<U>& rhs) {
		return !rhs ? true : (!lhs ? false : *lhs >= *rhs);
	}

	FASTGLTF_EXPORT template <typename T, std::three_way_comparable_with<T> U>
	constexpr std::compare_three_way_result_t<T, U> operator<=>(const OptionalWithFlagValue<T>& lhs,
																const OptionalWithFlagValue<U>& rhs) {
		return lhs && rhs ? *lhs <=> *rhs : lhs.has_value() <=> rhs.has_value();
	}

	FASTGLTF_EXPORT template <typename T>
	constexpr bool operator==(const OptionalWithFlagValue<T>& opt, std::nullopt_t) noexcept {
		return !opt.has_value();
	}

	FASTGLTF_EXPORT template <typename T>
	constexpr std::strong_ordering operator<=>(const OptionalWithFlagValue<T>& opt,
											   std::nullopt_t) noexcept {
		return opt.has_value() <=> false;
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	requires (!internal::is_optional_v<U>)
	constexpr bool operator==(const OptionalWithFlagValue<T>& opt, const U& value) {
		return opt.has_value() && (*opt) == value;
	}

	// No operator!= for plain values: C++20 synthesizes it from operator==, and declaring one
	// would prevent the reversed `value == opt` form from being considered (P2468R2).

	FASTGLTF_EXPORT template <typename T, typename U>
	requires (!internal::is_optional_v<U>)
	constexpr bool operator<(const OptionalWithFlagValue<T>& opt, const U& value) {
		return opt.has_value() ? *opt < value : true;
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	requires (!internal::is_optional_v<U>)
	constexpr bool operator<=(const OptionalWithFlagValue<T>& opt, const U& value) {
		return opt.has_value() ? *opt <= value : true;
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	requires (!internal::is_optional_v<U>)
	constexpr bool operator>(const OptionalWithFlagValue<T>& opt, const U& value) {
		return opt.has_value() ? *opt > value : false;
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	requires (!internal::is_optional_v<U>)
	constexpr bool operator>=(const OptionalWithFlagValue<T>& opt, const U& value) {
		return opt.has_value() ? *opt >= value : false;
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	requires (!internal::is_optional_v<U>) && std::three_way_comparable_with<T, U>
	constexpr std::compare_three_way_result_t<T, U> operator<=>(const OptionalWithFlagValue<T>& opt,
																const U& value) {
		return opt.has_value() ? *opt <=> value : std::strong_ordering::less;
	}
} // namespace fastgltf

#endif
