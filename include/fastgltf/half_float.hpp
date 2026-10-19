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
#ifndef FASTGLTF_HALF_FLOAT_HPP
#define FASTGLTF_HALF_FLOAT_HPP

#if !defined(FASTGLTF_MODULE)
#include <bit>
#include <limits>
#endif

#include <fastgltf/util.hpp>

// GCC predefines __FLT16_MAX__ in C++ mode on x86 since GCC 12, but only accepts _Float16 in C++ since GCC 13.
#if defined(__FLT16_MAX__) && (defined(__clang__) || __GNUC__ >= 13)
#define FASTGLTF_HAS_FLOAT16 1
#else
#define FASTGLTF_HAS_FLOAT16 0
#endif

namespace fastgltf {
	namespace internal {
		static_assert(std::numeric_limits<float>::is_iec559 &&
		              std::numeric_limits<float>::radix == 2 &&
		              std::numeric_limits<float>::digits == 24 &&
		              std::numeric_limits<float>::max_exponent == 128,
		              "Half float conversions are only supported on IEEE754 platforms");

		/**
		 * Converts a IEEE754 binary16 float stored in @ref half to a IEEE754 binary32
		 */
		[[nodiscard]] constexpr float halfToFloat(std::uint16_t half) noexcept {
			const std::uint32_t sign = static_cast<std::uint32_t>(half & 0x8000u) << 16;
			const std::uint32_t exponent = (half >> 10) & 0x1Fu;
			std::uint32_t mantissa = half & 0x3FFu;

			std::uint32_t bits;
			if (exponent == 0x1F) { // Inf/NaN
				bits = sign | 0x7F800000u | (mantissa << 13);
			} else if (exponent != 0) {
				bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
			} else if (mantissa == 0) {
				bits = sign;
			} else {
				std::uint32_t e = 113;
				while ((mantissa & 0x400u) == 0) {
					mantissa <<= 1;
					--e;
				}
				bits = sign | (e << 23) | ((mantissa & 0x3FFu) << 13);
			}
			return std::bit_cast<float>(bits);
		}

		/**
		 * Converts a IEEE754 binary32 float to a IEEE754 binary16
		 */
		[[nodiscard]] constexpr std::uint16_t floatToHalf(const float value) noexcept {
			const auto bits = std::bit_cast<std::uint32_t>(value);
			const std::uint32_t sign = (bits >> 16) & 0x8000U;
			const std::uint32_t exponent = (bits >> 23) & 0xFFU;
			std::uint32_t mantissa = bits & 0x7FFFFFU;

			std::uint32_t result;
			if (exponent == 0xFF) {
				result = 0x7C00U | (mantissa != 0 ? 0x200U | (mantissa >> 13) : 0U);
			} else if (const int e = static_cast<int>(exponent) - 112; e >= 0x1F) {
				result = 0x7C00U;
			} else if (e <= 0) {
				if (e < -10) {
					result = 0;
				} else {
					mantissa |= 0x800000U;
					const auto shift = static_cast<std::uint32_t>(14 - e);
					const std::uint32_t rem = mantissa & ((1U << shift) - 1);
					const std::uint32_t halfway = 1U << (shift - 1);
					result = mantissa >> shift;
					if (rem > halfway || (rem == halfway && (result & 1U)))
						++result;
				}
			} else {
				const std::uint32_t rem = mantissa & 0x1FFFU;
				result = (static_cast<std::uint32_t>(e) << 10) | (mantissa >> 13);
				if (rem > 0x1000U || (rem == 0x1000U && (result & 1U)))
					++result;
			}
			return static_cast<std::uint16_t>(sign | result);
		}
	} // namespace internal

	/**
	 * Utility type that only exists to hold a 16-bit value and be a tagged
	 * type to use when reading IEEE754 binary16 values from accessors.
	 */
	FASTGLTF_EXPORT class HalfFloat {
		std::uint16_t bits;

		[[nodiscard]] constexpr std::uint16_t floatToHalf(const float value) noexcept {
#if FASTGLTF_HAS_FLOAT16
			// Special case for some architectures where the compiler implements the _Float16 type. For
			// example, on arm64 (or other architectures that have scalar fp16 support) this compiles down
			// to a single fcvt instruction. Both GCC and Clang support _Float16 on arm and x86 in recent
			// versions, see the following for a description:
			// https://releases.llvm.org/16.0.0/tools/clang/docs/LanguageExtensions.html#half-precision-floating-point
			return std::bit_cast<std::uint16_t>(static_cast<_Float16>(value));
#else
			return internal::floatToHalf(value);
#endif
		}

	public:
		constexpr HalfFloat() noexcept = default;
		constexpr explicit HalfFloat(const float value) noexcept : bits(floatToHalf(value)) {}

		template <typename T>
		requires (!std::same_as<T, float> && !std::same_as<T, HalfFloat>)
		HalfFloat(T) = delete;

		template <typename T>
		requires (std::is_arithmetic_v<T> && !std::same_as<T, bool>)
		[[nodiscard]] constexpr explicit operator T() const noexcept {
#if FASTGLTF_HAS_FLOAT16
			// Again special handling of 16-bit floats depending on compiler support, see above.
			return static_cast<T>(std::bit_cast<_Float16>(bits));
#else
			return static_cast<T>(internal::halfToFloat(bits));
#endif
		}

		[[nodiscard]] static constexpr HalfFloat fromBits(const std::uint16_t value) noexcept {
			HalfFloat ret;
			ret.bits = value;
			return ret;
		}
		[[nodiscard]] constexpr std::uint16_t toBits() const noexcept {
			return bits;
		}
	};
	static_assert(sizeof(HalfFloat) == 2);
    static_assert(std::is_trivially_copyable_v<HalfFloat> && std::is_standard_layout_v<HalfFloat>);
} // namespace fastgltf

#endif
