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

#ifndef FASTGLTF_UTIL_HPP
#define FASTGLTF_UTIL_HPP

#if !defined(FASTGLTF_MODULE)
#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>
#include <variant>
#include <version>
#endif

#ifndef FASTGLTF_EXPORT
#define FASTGLTF_EXPORT
#endif

#ifndef FASTGLTF_CPLUSPLUS
#ifdef _MSVC_LANG
#define FASTGLTF_CPLUSPLUS _MSVC_LANG
#else
#define FASTGLTF_CPLUSPLUS __cplusplus
#endif
#endif

// Macros to determine C++ standard version
#if !(FASTGLTF_CPLUSPLUS >= 202002L) || (defined(FASTGLTF_CPP_20) && !FASTGLTF_CPP_20)
#error "fastgltf requires C++20"
#endif

#ifndef FASTGLTF_CPP_23
#if FASTGLTF_CPLUSPLUS >= 202302L
#define FASTGLTF_CPP_23 1
#else
#define FASTGLTF_CPP_23 0
#endif
#endif

#if FASTGLTF_CPP_23
#define FASTGLTF_UNREACHABLE std::unreachable();
#elif defined(__GNUC__) || defined(__clang__)
#define FASTGLTF_UNREACHABLE __builtin_unreachable();
#elif defined(_MSC_VER)
#define FASTGLTF_UNREACHABLE __assume(false);
#else
#define FASTGLTF_UNREACHABLE assert(0);
#endif

#if defined(__cpp_lib_containers_ranges) && __cpp_lib_containers_ranges >= 202202L
#define FASTGLTF_HAS_CONTAINERS_RANGES 1
#else
#define FASTGLTF_HAS_CONTAINERS_RANGES 0
#endif

#if defined(__has_builtin)
#define FASTGLTF_HAS_BUILTIN(x) __has_builtin(x)
#else
#define FASTGLTF_HAS_BUILTIN(x) 0
#endif

#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_IX86)
#define FASTGLTF_IS_X86 1
#elif defined(_M_ARM64) || defined(__aarch64__)
// __ARM_NEON is only for general Neon availability. It does not guarantee the full A64 instruction set.
#define FASTGLTF_IS_A64 1
#endif

#if (defined(_MSC_VER) && !defined(__clang__)) || __has_cpp_attribute(msvc::intrinsic)
#define FASTGLTF_INTRINSIC [[msvc::intrinsic]]
#else
#define FASTGLTF_INTRINSIC
#endif

#if defined(_MSC_VER)
#define FASTGLTF_FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define FASTGLTF_FORCEINLINE [[gnu::always_inline]] inline
#else
// On other compilers we need the inline specifier, so that the functions in this compilation unit
// can be properly inlined without the "function body can be overwritten at link time" error.
#define FASTGLTF_FORCEINLINE inline
#endif

#if defined(_MSC_VER)
#define FASTGLTF_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define FASTGLTF_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5030) // attribute 'x' is not recognized
#pragma warning(disable : 4514) // unreferenced inline function has been removed
#endif

namespace fastgltf {
	FASTGLTF_EXPORT template<typename T>
	requires std::is_enum_v<T>
	[[nodiscard]] FASTGLTF_INTRINSIC constexpr std::underlying_type_t<T> to_underlying(T t) noexcept {
		return static_cast<std::underlying_type_t<T>>(t);
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	requires ((std::is_enum_v<T> && std::integral<std::underlying_type_t<T>>) || std::integral<T>) && requires (T t, U u) {
		{ t & u } -> std::same_as<U>;
	}
	[[nodiscard]] constexpr bool hasBit(T flags, U bit) {
		return (flags & bit) == bit;
	}

	template <typename T, typename U>
	[[nodiscard]] constexpr T alignUp(T base, U alignment) {
		static_assert(std::is_signed_v<U>, "alignUp requires type U to be signed.");
		return (base + alignment - 1) & -alignment;
	}

	template <typename T>
	[[nodiscard]] constexpr T alignDown(T base, T alignment) {
		return base - (base % alignment);
	}

	FASTGLTF_EXPORT template <typename T>
	requires requires (T t) {
		{ t > t } -> std::same_as<bool>;
	}
	[[nodiscard]] constexpr const T& max(const T& a, const T& b) noexcept {
		return (a > b) ? a : b;
	}

	FASTGLTF_EXPORT template <typename T>
	requires requires (T t) {
		{ t < t } -> std::same_as<bool>;
	}
	[[nodiscard]] constexpr const T& min(const T& a, const T& b) noexcept {
		return (a < b) ? a : b;
	}

	template<typename T, typename... A>
	[[noreturn]] constexpr void raise([[maybe_unused]] A&&... args) {
#ifdef __cpp_exceptions
		throw T(std::forward<A>(args)...);
#else
		std::abort();
#endif
	}

	/**
	 * Helper to force evaluation of constexpr functions at compile-time in C++17. One example of
	 * this is with crc32: force_consteval<crc32("string")>. No matter the context, this will
	 * always be evaluated to a constant.
	 */
	template <auto V>
	inline constexpr auto force_consteval = V;

	/**
	 * Essentially the same as std::same<T, U> but it accepts multiple different types for U,
	 * checking if T is any of U...
	 */
	template <typename T, typename... Ts>
	using is_any_of = std::disjunction<std::is_same<T, Ts>...>;

	template <typename T, typename... Ts>
	inline constexpr bool is_any_of_v = is_any_of<T, Ts...>::value;

	/**
	 * Helper type in order to allow building a visitor out of multiple lambdas within a call to
	 * std::visit
	 */
	FASTGLTF_EXPORT template<class... Ts>
	struct visitor : Ts... {
		using Ts::operator()...;
	};

	FASTGLTF_EXPORT template<class... Ts> visitor(Ts...) -> visitor<Ts...>;

	template <typename Visitor, typename Variant, std::size_t... i>
	constexpr bool is_exhaustive_visitor(std::integer_sequence<std::size_t, i...>) noexcept {
		return std::conjunction_v<std::is_invocable<Visitor, std::variant_alternative_t<i, std::remove_cvref_t<Variant>>>...>;
	}

	/**
	 * Simple wrapper around std::visit for a single variant that checks at compile-time if the given visitor contains
	 * overloads for *all* required alternatives. This is meant to guarantee correctness, since using something like
	 * fastgltf::visitor could fail unexpectedly due to const-issues without any compile-time errors or warnings.
	 * @note This currently does not support auto parameters.
	 */
	FASTGLTF_EXPORT template<typename Visitor, typename Variant>
	constexpr decltype(auto) visit_exhaustive(Visitor&& visitor, Variant&& variant) {
		static_assert(is_exhaustive_visitor<Visitor, Variant>(std::make_index_sequence<std::variant_size_v<std::remove_cvref_t<Variant>>>()),
			"The visitor does not include all necessary overloads for the given variant");
		return std::visit(std::forward<Visitor>(visitor), std::forward<Variant>(variant));
	}

	template <auto callback>
	struct UniqueDeleter {
		template <typename T>
		constexpr void operator()(T* t) const {
			callback(t);
		}
	};

	/** Allows the quick creation of a unique_ptr type that can take any function of the form void(T*) for its deleter */
	template <typename T, auto callback>
	using deletable_unique_ptr = std::unique_ptr<T, UniqueDeleter<callback>>;

	// For simple ops like &, |, +, - taking a left and right operand.
#define FASTGLTF_ARITHMETIC_OP_TEMPLATE_MACRO(T1, T2, op) \
	FASTGLTF_EXPORT constexpr T1 operator op(const T1& a, const T2& b) noexcept { \
		static_assert(std::is_enum_v<T1> && std::is_enum_v<T2>); \
		return static_cast<T1>(to_underlying(a) op to_underlying(b)); \
	}

	// For any ops like |=, &=, +=, -=
#define FASTGLTF_ASSIGNMENT_OP_TEMPLATE_MACRO(T1, T2, op) \
	FASTGLTF_EXPORT constexpr T1& operator op##=(T1& a, const T2& b) noexcept { \
		static_assert(std::is_enum_v<T1> && std::is_enum_v<T2>); \
		return a = static_cast<T1>(to_underlying(a) op to_underlying(b)), a; \
	}

	// For unary +, unary -, and bitwise NOT
#define FASTGLTF_UNARY_OP_TEMPLATE_MACRO(T, op) \
	FASTGLTF_EXPORT constexpr T operator op(const T& a) noexcept { \
		static_assert(std::is_enum_v<T>); \
		return static_cast<T>(op to_underlying(a)); \
	}

	/**
	 * Returns the absolute value of the given integer in its unsigned type.
	 * This avoids the issue with two complementary signed integers not being able to represent INT_MIN.
	 */
	template <std::integral T>
	[[nodiscard]] constexpr std::make_unsigned_t<T> uabs(T val) {
		if constexpr (std::is_signed_v<T>) {
			using unsigned_t = std::make_unsigned_t<T>;
			return (val < 0)
				? static_cast<unsigned_t>(-(val + 1)) + 1
				: static_cast<unsigned_t>(val);
		} else {
			return val;
		}
	}

	/**
	 * Simple function to check if the given string starts with a given set of characters.
	 */
	[[nodiscard]] inline bool startsWith(const std::string_view str, const std::string_view search) {
		return str.rfind(search, 0) == 0;
	}

	FASTGLTF_EXPORT struct for_overwrite_t { explicit for_overwrite_t() = default; };
	FASTGLTF_EXPORT inline constexpr for_overwrite_t for_overwrite {};
} // namespace fastgltf

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
