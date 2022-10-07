#pragma once

#include <type_traits>

namespace fastgltf {
    template<typename T>
    constexpr std::underlying_type_t<T> to_underlying(T t) noexcept {
        static_assert(std::is_enum_v<T>, "to_underlying only works with enum types.");
        return static_cast<std::underlying_type_t<T>>(t);
    }

    template <typename T, typename U>
    constexpr bool hasBit(T flags, U bit) {
        static_assert((std::is_enum_v<T> && std::is_integral_v<std::underlying_type_t<T>>) || std::is_integral_v<T>);
        return (flags & bit) == bit;
    }

    template <typename T>
    constexpr T alignUp(T base, T alignment) {
        return (base + alignment - 1) & -alignment;
    }

    template <typename T>
    constexpr T alignDown(T base, T alignment) {
        return base - (base % alignment);
    }
}
