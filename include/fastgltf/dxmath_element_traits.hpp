#pragma once

#include <fastgltf/tools.hpp>

#if __has_include(<directxmath.h>)
#include <directxmath.h>
#endif

namespace fastgltf {

FASTGLTF_EXPORT template <typename ElementType, AccessorType EnumAccessorType, typename ComponentType = ElementType>
using TransposedElementTraits = ElementTraitsBase<ElementType, EnumAccessorType, ComponentType, true>;

template <>
struct ElementTraits<DirectX::XMFLOAT2> : ElementTraitsBase<DirectX::XMFLOAT2, AccessorType::Vec2, float> {};

template <>
struct ElementTraits<DirectX::XMFLOAT3> : ElementTraitsBase<DirectX::XMFLOAT3, AccessorType::Vec3, float> {};

template <>
struct ElementTraits<DirectX::XMFLOAT4> : ElementTraitsBase<DirectX::XMFLOAT4, AccessorType::Vec4, float> {};

template<>
struct ElementTraits<DirectX::XMBYTE2> : ElementTraitsBase<DirectX::XMBYTE2, AccessorType::Vec2, std::uint8_t> {};

template<>
struct ElementTraits<DirectX::XMBYTE4> : ElementTraitsBase<DirectX::XMBYTE4, AccessorType::Vec4, std::uint8_t> {};

template<>
struct ElementTraits<DirectX::XMBYTE2> : ElementTraitsBase<DirectX::XMBYTE2, AccessorType::Vec2, std::uint8_t> {};

template<>
struct ElementTraits<DirectX::XMBYTE4> : ElementTraitsBase<DirectX::XMBYTE4, AccessorType::Vec4, std::uint8_t> {};

template<>
struct ElementTraits<DirectX::XMSHORT2> : ElementTraitsBase<DirectX::XMSHORT2, AccessorType::Vec2, std::uint16_t> {};

template<>
struct ElementTraits<DirectX::XMSHORT4> : ElementTraitsBase<DirectX::XMSHORT4, AccessorType::Vec4, std::uint16_t> {};

template<>
struct ElementTraits<DirectX::XMUSHORT2> : ElementTraitsBase<DirectX::XMUSHORT2, AccessorType::Vec2, std::uint16_t> {};

template<>
struct ElementTraits<DirectX::XMUSHORT4> : ElementTraitsBase<DirectX::XMUSHORT4, AccessorType::Vec4, std::uint16_t> {};

template<>
struct ElementTraits<DirectX::XMUINT2> : ElementTraitsBase<DirectX::XMUINT2, AccessorType::Vec2, std::uint32_t> {};

template<>
struct ElementTraits<DirectX::XMUINT3> : ElementTraitsBase<DirectX::XMUINT3, AccessorType::Vec3, std::uint32_t> {};

template<>
struct ElementTraits<DirectX::XMUINT4> : ElementTraitsBase<DirectX::XMUINT4, AccessorType::Vec4, std::uint32_t> {};

template<>
struct ElementTraits<DirectX::XMFLOAT3x3> : TransposedElementTraits<DirectX::XMFLOAT3x3, AccessorType::Mat3, float> {};

template<>
struct ElementTraits<DirectX::XMFLOAT4X4> : TransposedElementTraits<DirectX::XMFLOAT4X4, AccessorType::Mat4, float> {};

} // namespace fastgltf
