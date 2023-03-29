/*
 * Copyright (C) 2022 - 2023 spnda
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

#pragma once

#include <array>
#include <cmath>
#include <type_traits>

// Macros to determine C++ standard version
#if (!defined(_MSVC_LANG) && __cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define FASTGLTF_CPP_17 1
#else
#error "fastgltf requires C++17"
#endif

#if (!defined(_MSVC_LANG) && __cplusplus >= 202002L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#define FASTGLTF_CPP_20 1
#else
#define FASTGLTF_CPP_20 0
#endif

#if FASTGLTF_CPP_20 && defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
#define FASTGLTF_HAS_BIT 1
#include <bit>
#else
#define FASTGLTF_HAS_BIT 0
#endif

#if FASTGLTF_CPP_20 && defined(__cpp_concepts) && __cpp_concepts >= 202002L
#define FASTGLTF_HAS_CONCEPTS 1
#include <concepts>
#else
#define FASTGLTF_HAS_CONCEPTS 0
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5030) // attribute 'x' is not recognized
#pragma warning(disable : 4514) // unreferenced inline function has been removed
#endif

namespace fastgltf {
    template<typename T>
#if FASTGLTF_HAS_CONCEPTS
    requires std::is_enum_v<T>
#endif
    [[nodiscard]] constexpr std::underlying_type_t<T> to_underlying(T t) noexcept {
#if !FASTGLTF_HAS_CONCEPTS
        static_assert(std::is_enum_v<T>, "to_underlying only works with enum types.");
#endif
        return static_cast<std::underlying_type_t<T>>(t);
    }

    template <typename T, typename U>
#if FASTGLTF_HAS_CONCEPTS
    requires ((std::is_enum_v<T> && std::integral<std::underlying_type_t<T>>) || std::integral<T>) && requires (T t, U u) {
        { t & u } -> std::same_as<U>;
    }
#endif
    [[nodiscard]] constexpr bool hasBit(T flags, U bit) {
#if !FASTGLTF_HAS_CONCEPTS
        static_assert((std::is_enum_v<T> && std::is_integral_v<std::underlying_type_t<T>>) || std::is_integral_v<T>);
#endif
        return (flags & bit) == bit;
    }

    template <typename T>
    [[nodiscard]] constexpr T alignUp(T base, T alignment) {
        static_assert(std::is_signed_v<T>, "alignUp requires type T to be signed.");
        return (base + alignment - 1) & -alignment;
    }

    template <typename T>
    [[nodiscard]] constexpr T alignDown(T base, T alignment) {
        return base - (base % alignment);
    }

    template <typename T>
#if FASTGLTF_HAS_CONCEPTS
    requires requires (T t) {
        { t > t } -> std::same_as<bool>;
    }
#endif
    [[nodiscard]] inline T max(T a, T b) noexcept {
        return (a > b) ? a : b;
    }

    /**
     * Decomposes a transform matrix into the translation, rotation, and scale components. This
     * function does not support skew, shear, or perspective. This currently uses a quick algorithm
     * to calculate the quaternion from the rotation matrix, which might occasionally loose some
     * precision, though we try to use doubles here.
     */
    inline void decomposeTransformMatrix(std::array<float, 16> matrix, std::array<float, 3>& scale, std::array<float, 4>& rotation, std::array<float, 3>& translation) {
        // Extract the translation. We zero the translation out, as we reuse the matrix as
        // the rotation matrix at the end.
        translation = {matrix[12], matrix[13], matrix[14]};
        matrix[12] = matrix[13] = matrix[14] = 0;

        // Extract the scale. We calculate the euclidean length of the columns. We then
        // construct a vector with those lengths. My gcc's stdlib doesn't include std::sqrtf
        // for some reason...
        auto s1 = sqrtf(matrix[0] * matrix[0] + matrix[1] * matrix[1] +  matrix[2] * matrix[2]);
        auto s2 = sqrtf(matrix[4] * matrix[4] + matrix[5] * matrix[5] +  matrix[6] * matrix[6]);
        auto s3 = sqrtf(matrix[8] * matrix[8] + matrix[9] * matrix[9] + matrix[10] * matrix[10]);
        scale = {s1, s2, s3};

        // Remove the scaling from the matrix, leaving only the rotation. matrix is now the
        // rotation matrix.
        matrix[0] /= s1; matrix[1] /= s1;  matrix[2] /= s1;
        matrix[4] /= s2; matrix[5] /= s2;  matrix[6] /= s2;
        matrix[8] /= s3; matrix[9] /= s3; matrix[10] /= s3;

        // Construct the quaternion. This algo is copied from here:
        // https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/christian.htm.
        // glTF orders the components as x,y,z,w
        rotation = {
            max(.0f, 1 + matrix[0] - matrix[5] - matrix[10]),
            max(.0f, 1 - matrix[0] + matrix[5] - matrix[10]),
            max(.0f, 1 - matrix[0] - matrix[5] + matrix[10]),
            max(.0f, 1 + matrix[0] + matrix[5] + matrix[10]),
        };
        for (auto& x : rotation) {
            x = static_cast<float>(std::sqrt(static_cast<double>(x)) / 2);
        }
        rotation[0] = std::copysignf(rotation[0], matrix[6] - matrix[9]);
        rotation[1] = std::copysignf(rotation[1], matrix[8] - matrix[2]);
        rotation[2] = std::copysignf(rotation[2], matrix[1] - matrix[4]);
    }

    static constexpr std::array<std::uint32_t, 256> crcHashTable = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };

    [[gnu::hot, gnu::const]] constexpr std::uint32_t crc32(std::string_view str) noexcept {
        std::uint32_t crc = 0xffffffff;
        for (auto c : str)
            crc = (crc >> 8) ^ crcHashTable[(crc ^ c) & 0xff];
        return crc ^ 0xffffffff;
    }

    /**
     * Helper to force evaluation of constexpr functions at compile-time in C++17. One example of
     * this is with crc32: force_consteval<crc32("string")>. No matter the context, this will
     * always be evaluated to a constant.
     */
    template <auto V>
    static constexpr auto force_consteval = V;

    /**
     * Counts the leading zeros from starting the most significant bit. Returns a std::uint8_t as there
     * can only ever be 2^6 zeros with 64-bit types.
     */
     template <typename T>
#if FASTGLTF_HAS_CONCEPTS
    requires std::integral<T>
#endif
    [[gnu::const]] inline std::uint8_t clz(T value) {
        static_assert(std::is_integral_v<T>);
#if FASTGLTF_HAS_BIT
        return std::countl_zero(value);
#else
        // Very naive but working implementation of counting zero bits. Any sane compiler will
        // optimise this away, like instead use the bsr x86 instruction.
        if (value == 0) return 64;
        std::uint8_t count = 0;
        for (int i = std::numeric_limits<T>::digits; i > 0; --i) {
            if ((value >> i) == 1) {
                return count;
            }
            ++count;
        }
        return count;
#endif
    }

    /**
     * Essentially the same as std::same<T, U> but it accepts multiple different types for U,
     * checking if T is any of U...
     */
    template <typename T, typename... Ts>
    using is_any = std::disjunction<std::is_same<T, Ts>...>;

    /**
     * Simple function to check if the given string starts with a given set of characters.
     */
    inline bool startsWith(std::string_view str, std::string_view search) {
        return str.rfind(search, 0) == 0;
    }

    // For simple ops like &, |, +, - taking a left and right operand.
#define FASTGLTF_ARITHMETIC_OP_TEMPLATE_MACRO(T1, T2, op) \
    constexpr T1 operator op(const T1& a, const T2& b) noexcept { \
        static_assert(std::is_enum_v<T1> && std::is_enum_v<T2>); \
        return static_cast<T1>(to_underlying(a) op to_underlying(b)); \
    }

    // For any ops like |=, &=, +=, -=
#define FASTGLTF_ASSIGNMENT_OP_TEMPLATE_MACRO(T1, T2, op) \
    constexpr T1& operator op##=(T1& a, const T2& b) noexcept { \
        static_assert(std::is_enum_v<T1> && std::is_enum_v<T2>); \
        return a = static_cast<T1>(to_underlying(a) op to_underlying(b)), a; \
    }

    // For unary +, unary -, and bitwise NOT
#define FASTGLTF_UNARY_OP_TEMPLATE_MACRO(T, op) \
    constexpr T operator op(const T& a) noexcept { \
        static_assert(std::is_enum_v<T>); \
        return static_cast<T>(op to_underlying(a)); \
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
