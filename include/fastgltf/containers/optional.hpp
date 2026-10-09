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

#if !defined(FASTGLTF_MODULE)
#include <functional>
#include <optional>
#endif

#include <fastgltf/util.hpp>
#include <fastgltf/containers/box.hpp>

namespace fastgltf {
	FASTGLTF_EXPORT template <typename>
	struct optional_flag_value {};

	FASTGLTF_EXPORT template <typename T, T Sentinel>
	struct sentinel_flag_value {
		static constexpr bool is_empty(const T& value) noexcept {
			return value == Sentinel;
		}
		static constexpr void set_empty(T& value) noexcept {
			value = Sentinel;
		}
	};

	FASTGLTF_EXPORT template <typename T, auto Sentinel>
	requires std::is_enum_v<T> && std::same_as<decltype(Sentinel), std::underlying_type_t<T>>
	struct enum_sentinel_flag_value {
		static constexpr bool is_empty(const T& value) noexcept {
			return to_underlying(value) == Sentinel;
		}
		static constexpr void set_empty(T& value) noexcept {
			value = static_cast<T>(Sentinel);
		}
	};

	template<>
	struct optional_flag_value<std::size_t> :
		sentinel_flag_value<std::size_t, std::numeric_limits<std::size_t>::max()> {};

	template <typename T>
	requires std::same_as<T, float> && std::numeric_limits<float>::is_iec559
	struct optional_flag_value<T> {
		// This float is a quiet NaN with a specific bit pattern to be able to differentiate
		// between this flag value and any result from FP operations.
		static constexpr std::uint32_t nan_sentinel = 0x7fedb6db;

		static constexpr bool is_empty(const float& value) noexcept {
			return std::bit_cast<std::uint32_t>(value) == nan_sentinel;
		}
		static constexpr void set_empty(float& value) noexcept {
			value = std::bit_cast<float>(nan_sentinel);
		}
	};

	template <typename T>
	requires std::same_as<T, double> && std::numeric_limits<double>::is_iec559
	struct optional_flag_value<T> {
		static constexpr uint64_t nan_sentinel = 0x7ffdb6db6db6db6d;

		static constexpr bool is_empty(const double& value) noexcept {
			return std::bit_cast<std::uint64_t>(value) == nan_sentinel;
		}
		static constexpr void set_empty(double& value) noexcept {
			value = std::bit_cast<double>(nan_sentinel);
		}
	};

	FASTGLTF_EXPORT template <typename T, typename Traits = optional_flag_value<T>>
	concept has_flag_traits = requires(const T& cv, T& v) {
		{ Traits::is_empty(cv) } noexcept -> std::same_as<bool>;
		{ Traits::set_empty(v) } noexcept;
	};

	FASTGLTF_EXPORT template <typename T>
	class flagged_storage {
		static_assert(has_flag_traits<T>, "flagged_storage<T> requires flag traits to exist");

		using value_type = T;
		using traits = optional_flag_value<value_type>;

		static constexpr value_type make_empty() noexcept {
			value_type val {};
			traits::set_empty(val);
			return val;
		}

		std::remove_const_t<T> _value = make_empty();

	public:
		constexpr flagged_storage() noexcept = default;
		template <typename... Args>
		constexpr explicit flagged_storage(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
			: _value(std::forward<Args>(args)...) {}

		[[nodiscard]] constexpr bool has_value() const noexcept { return !traits::is_empty(_value); }
		[[nodiscard]] constexpr T& get() noexcept { return _value; }
		[[nodiscard]] constexpr const T& get() const noexcept { return _value; }
		constexpr void reset() noexcept { traits::set_empty(_value); }

		template <typename... Args>
		constexpr T& emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...> && std::is_nothrow_move_assignable_v<T>) {
			// Construct first, so a throwing constructor leaves the old value intact.
			_value = T(std::forward<Args>(args)...);
			return _value;
		}
	};
	FASTGLTF_EXPORT template <typename T, typename Allocator = std::allocator<T>>
	class boxed_storage {
		using box_type = box<T, Allocator>;
		box_type _value { valueless_tag };

	public:
		constexpr boxed_storage() = default;
		template <typename... Args>
		constexpr explicit boxed_storage(std::in_place_t, Args&&... args)
			: _value(std::in_place, std::forward<Args>(args)...) {}

		[[nodiscard]] constexpr bool has_value() const noexcept { return !_value.valueless_after_move(); }
		[[nodiscard]] constexpr T& get() noexcept { return *_value; }
		[[nodiscard]] constexpr const T& get() const noexcept { return *_value; }
		constexpr void reset() noexcept { _value = box_type(valueless_tag); }

		template <typename... Args>
		constexpr T& emplace(Args&&... args) {
			_value = box_type(std::allocator_arg, _value.get_allocator(), std::in_place, std::forward<Args>(args)...);
			return *_value;
		}
	};

	FASTGLTF_EXPORT template <typename T, typename Storage>
	class basic_optional;

	FASTGLTF_EXPORT template <typename T>
	using flagged_optional = basic_optional<T, flagged_storage<T>>;

	FASTGLTF_EXPORT template <typename T, typename Allocator = std::allocator<T>>
	using boxed_optional = basic_optional<T, boxed_storage<T, Allocator>>;

	/**
	 * Uses flagged_optional<T> when available but falls back to std::optional<T>.
	 */
	FASTGLTF_EXPORT template <typename T>
	using optional = std::conditional_t<has_flag_traits<T>, flagged_optional<T>, std::optional<T>>;

	namespace internal {
		/** Excludes optional types and std::nullopt_t from the comparison operators taking a plain value, like std::optional does. */
		template <typename T>
		inline constexpr bool is_optional_impl_v = std::is_same_v<T, std::nullopt_t>;
		template <typename T, typename S>
		inline constexpr bool is_optional_impl_v<basic_optional<T, S>> = true;
		template <typename T>
		inline constexpr bool is_optional_impl_v<std::optional<T>> = true;

		template <typename T>
		inline constexpr bool is_optional_v = is_optional_impl_v<std::remove_cv_t<T>>;
	} // namespace internal

	/**
	 * A custom optional class for fastgltf which uses so-called "flag values" which are specific values of T that
	 * will never be present as an actual value. We can therefore use those values as flags for whether there is an
	 * actual value stored instead of the additional bool used by std::optional.
	 *
	 * Setting and clearing these flag values is handled by optional_flag_value<T>::set_empty and
	 * optional_flag_value<T>::is_empty, see the has_flag_traits<T> concept.
	 */
	template <typename T, typename Storage>
	class basic_optional final {
		Storage _storage;

	public:
		using value_type = T;

		constexpr basic_optional() noexcept = default;
		constexpr basic_optional(std::nullopt_t) noexcept {}

		constexpr basic_optional(const basic_optional&) = default;
		constexpr basic_optional(basic_optional&&) = default;
		constexpr basic_optional& operator=(const basic_optional&) = default;
		constexpr basic_optional& operator=(basic_optional&&) = default;

		template <typename U, typename S>
		requires std::is_constructible_v<T, const U&>
		constexpr explicit(!std::is_convertible_v<const U&, T>) basic_optional(const basic_optional<U, S>& other) {
			if (other) {
				_storage.emplace(*other);
			}
		}

		template <typename U, typename S>
		requires std::is_constructible_v<T, U&&>
		constexpr explicit(!std::is_convertible_v<U&&, T>) basic_optional(basic_optional<U, S>&& other) {
			if (other.has_value())
				_storage.emplace(std::move(*other));
		}

		template <typename... Args>
		requires std::is_constructible_v<T, Args...>
		constexpr explicit basic_optional(std::in_place_t, Args&&... args) noexcept(
			std::is_nothrow_constructible_v<Storage, std::in_place_t, Args...>)
			: _storage(std::in_place, std::forward<Args>(args)...) {
			assert(has_value());
		}

		template <typename U, typename... Args>
		requires std::is_constructible_v<T, std::initializer_list<U>&, Args...>
		constexpr explicit basic_optional(std::in_place_t, std::initializer_list<U> ilist, Args&&... args )
			: _storage(std::in_place, ilist, std::forward<Args>(args)...) {
			assert(has_value());
		}

		template <typename U = std::remove_cv_t<T>>
		requires std::is_constructible_v<T, U&&> && is_none_of_v<std::remove_cvref_t<U>, std::in_place_t, basic_optional>
		constexpr explicit(!std::is_convertible_v<U&&, T>) basic_optional(U&& value)
		noexcept(std::is_nothrow_constructible_v<Storage, std::in_place_t, U>)
			: _storage(std::in_place, std::forward<U>(value)) {
			assert(has_value());
		}

		constexpr ~basic_optional() = default;

		constexpr basic_optional& operator=(std::nullopt_t) noexcept {
			reset();
			return *this;
		}

		template <typename U = std::remove_cv_t<T>>
		requires (!std::same_as<std::remove_cvref_t<U>, basic_optional> &&
			std::is_assignable_v<T&, U> && (!std::is_scalar_v<T> || !std::same_as<std::decay_t<U>, T>))
		constexpr basic_optional& operator=(U&& _new)
		noexcept(std::is_nothrow_assignable_v<T&, U> && noexcept(std::declval<Storage&>().emplace(std::declval<U>()))) {
			if (has_value()) _storage.get() = std::forward<U>(_new);
			else _storage.emplace(std::forward<U>(_new));
			assert(has_value());
			return *this;
		}

		template <typename U, typename S>
		requires std::conjunction_v<std::is_constructible<T, const U&>,
									std::is_assignable<T&, const U&>>
		constexpr basic_optional& operator=(const basic_optional<U, S>& other) {
			if (other.has_value()) {
				if (has_value()) _storage.get() = *other;
				else _storage.emplace(*other);
			} else {
				reset();
			}
			return *this;
		}

		template <typename U, typename S>
		requires std::conjunction_v<std::is_constructible<T, U>, std::is_assignable<T&, U>>
		constexpr basic_optional& operator=(basic_optional<U, S>&& other)
		noexcept(std::is_nothrow_assignable_v<T&, U> && noexcept(std::declval<Storage&>().emplace(std::declval<U>()))) {
			if (other.has_value()) {
				if (has_value()) _storage.get() = std::move(*other);
				else _storage.emplace(std::move(*other));
			} else {
				reset();
			}
			return *this;
		}

		[[nodiscard]] constexpr bool has_value() const noexcept {
			return _storage.has_value();
		}

		[[nodiscard]] constexpr value_type& value() & {
			if (!has_value()) {
				raise<std::bad_optional_access>();
			}
			return _storage.get();
		}

		[[nodiscard]] constexpr const value_type& value() const& {
			if (!has_value()) {
				raise<std::bad_optional_access>();
			}
			return _storage.get();
		}

		[[nodiscard]] constexpr value_type&& value() && {
			if (!has_value()) {
				raise<std::bad_optional_access>();
			}
			return std::move(_storage.get());
		}

		[[nodiscard]] constexpr const value_type&& value() const&& {
			if (!has_value()) {
				raise<std::bad_optional_access>();
			}
			return std::move(_storage.get());
		}

		template <typename U = std::remove_cv_t<T>>
		[[nodiscard]] constexpr value_type value_or(U&& default_value) const& {
			return has_value() ? **this : static_cast<value_type>(std::forward<U>(default_value));
		}

		template <typename U = std::remove_cv_t<T>>
		[[nodiscard]] constexpr value_type value_or(U&& default_value) && {
			return has_value() ? std::move(**this) : static_cast<value_type>(std::forward<U>(default_value));
		}

		template <typename F>
		[[nodiscard]] constexpr auto and_then(F&& func) & {
			using U = std::remove_cvref_t<std::invoke_result_t<F, value_type&>>;
			if (!has_value())
				return U(std::nullopt);
			return std::invoke(std::forward<F>(func), **this);
		}

		template <typename F>
		[[nodiscard]] constexpr auto and_then(F&& func) const& {
			using U = std::remove_cvref_t<std::invoke_result_t<F, const value_type&>>;
			if (!has_value())
				return U(std::nullopt);
			return std::invoke(std::forward<F>(func), **this);
		}

		template <typename F>
		[[nodiscard]] constexpr auto and_then(F&& func) && {
			using U = std::remove_cvref_t<std::invoke_result_t<F, value_type>>;
			if (!has_value())
				return U(std::nullopt);
			return std::invoke(std::forward<F>(func), std::move(**this));
		}

		template <typename F>
		[[nodiscard]] constexpr auto and_then(F&& func) const&& {
			using U = std::remove_cvref_t<std::invoke_result_t<F, const value_type>>;
			if (!has_value())
				return U(std::nullopt);
			return std::invoke(std::forward<F>(func), std::move(**this));
		}

		template <typename F>
		[[nodiscard]] constexpr auto transform(F&& func) & {
			using U = std::remove_cv_t<std::invoke_result_t<F, value_type&>>;
			if (!has_value())
				return optional<U>(std::nullopt);
			return optional<U>(std::invoke(std::forward<F>(func), **this));
		}

		template <typename F>
		[[nodiscard]] constexpr auto transform(F&& func) const& {
			using U = std::remove_cv_t<std::invoke_result_t<F, const value_type&>>;
			if (!has_value())
				return optional<U>(std::nullopt);
			return optional<U>(std::invoke(std::forward<F>(func), **this));
		}

		template <typename F>
		[[nodiscard]] constexpr auto transform(F&& func) && {
			using U = std::remove_cv_t<std::invoke_result_t<F, value_type>>;
			if (!has_value())
				return optional<U>(std::nullopt);
			return optional<U>(std::invoke(std::forward<F>(func), std::move(**this)));
		}

		template <typename F>
		[[nodiscard]] constexpr auto transform(F&& func) const&& {
			using U = std::remove_cv_t<std::invoke_result_t<F, const value_type>>;
			if (!has_value())
				return optional<U>(std::nullopt);
			return optional<U>(std::invoke(std::forward<F>(func), std::move(**this)));
		}

		template <typename F>
		requires std::same_as<std::remove_cvref_t<std::invoke_result_t<F>>, basic_optional>
		[[nodiscard]] constexpr basic_optional or_else(F&& func) const& {
			return *this ? *this : std::invoke(std::forward<F>(func));
		}

		template <typename F>
		requires std::same_as<std::remove_cvref_t<std::invoke_result_t<F>>, basic_optional>
		[[nodiscard]] constexpr basic_optional or_else(F&& func) && {
			return *this ? std::move(*this) : std::invoke(std::forward<F>(func));
		}

		constexpr void swap(basic_optional& other) noexcept(std::is_nothrow_swappable_v<Storage>) {
			using std::swap;
			swap(_storage, other._storage);
		}

		constexpr void reset() noexcept {
			_storage.reset();
		}

		template <typename... Args>
		requires std::is_constructible_v<T, Args...>
		constexpr value_type& emplace(Args&&... args) {
			return _storage.emplace(std::forward<Args>(args)...);
		}

		template <typename U, typename... Args>
		requires std::is_constructible_v<T, std::initializer_list<U>&, Args...>
		constexpr value_type& emplace(std::initializer_list<U> list, Args&&... args) {
			static_assert(std::is_constructible_v<value_type, std::initializer_list<U>&, Args&&...>);
			return _storage.emplace(list, std::forward<Args>(args)...);
		}

		constexpr explicit operator bool() const noexcept {
			return has_value();
		}

		constexpr value_type* operator->() noexcept {
			assert(has_value());
			return std::addressof(_storage.get());
		}

		constexpr const value_type* operator->() const noexcept {
			assert(has_value());
			return std::addressof(_storage.get());
		}

		constexpr value_type& operator*() & noexcept {
			assert(has_value());
			return _storage.get();
		}

		constexpr const value_type& operator*() const& noexcept {
			assert(has_value());
			return _storage.get();
		}

		constexpr value_type&& operator*() && noexcept {
			assert(has_value());
			return std::move(_storage.get());
		}

		constexpr const value_type&& operator*() const&& noexcept {
			assert(has_value());
			return std::move(_storage.get());
		}

		operator std::optional<value_type>() const& noexcept(std::is_nothrow_copy_constructible_v<value_type>) {
			return has_value() ? std::optional<value_type>(_storage.get()) : std::nullopt;
		}

		operator std::optional<value_type>() && noexcept(std::is_nothrow_move_constructible_v<value_type>) {
			return has_value() ? std::optional<value_type>(std::move(_storage.get())) : std::nullopt;
		}
	};

	// Containers like std::vector only move elements when reallocating if the move constructor is noexcept
	static_assert(std::is_nothrow_move_constructible_v<flagged_optional<std::size_t>>);
	static_assert(std::is_nothrow_move_constructible_v<flagged_optional<float>>);
	static_assert(std::is_trivially_copyable_v<flagged_optional<std::size_t>>);
	static_assert(std::is_trivially_copyable_v<flagged_optional<float>>);

	FASTGLTF_EXPORT template <typename T, typename S1, typename U, typename S2>
	constexpr bool operator==(const basic_optional<T, S1>& lhs,
							  const basic_optional<U, S2>& rhs) {
		return lhs.has_value() != rhs.has_value() ? false
												  : (lhs.has_value() == false ? true : *lhs == *rhs);
	}

	FASTGLTF_EXPORT template <typename T, typename S1, std::three_way_comparable_with<T> U, typename S2>
	constexpr std::compare_three_way_result_t<T, U> operator<=>(const basic_optional<T, S1>& lhs,
																const basic_optional<U, S2>& rhs) {
		return lhs && rhs ? *lhs <=> *rhs : lhs.has_value() <=> rhs.has_value();
	}

	FASTGLTF_EXPORT template <typename T, typename S>
	constexpr bool operator==(const basic_optional<T, S>& opt, std::nullopt_t) noexcept {
		return !opt.has_value();
	}

	FASTGLTF_EXPORT template <typename T, typename S>
	constexpr std::strong_ordering operator<=>(const basic_optional<T, S>& opt,
											   std::nullopt_t) noexcept {
		return opt.has_value() <=> false;
	}

	FASTGLTF_EXPORT template <typename T, typename S, typename U>
	requires (!internal::is_optional_v<U>)
	constexpr bool operator==(const basic_optional<T, S>& opt, const U& value) {
		return opt.has_value() && (*opt) == value;
	}

	FASTGLTF_EXPORT template <typename T, typename S, typename U>
	requires (!internal::is_optional_v<U>) && std::three_way_comparable_with<T, U>
	constexpr std::compare_three_way_result_t<T, U> operator<=>(const basic_optional<T, S>& opt,
																const U& value) {
		return opt.has_value() ? *opt <=> value : std::strong_ordering::less;
	}

	template <typename T, typename S>
	constexpr void swap(basic_optional<T, S>& lhs, basic_optional<T, S>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
		lhs.swap(rhs);
	}
} // namespace fastgltf

#endif
