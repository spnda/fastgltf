#pragma once

#include <fastgltf/tools.hpp>

#if __has_include(<DirectXMath.h>)
#include <DirectXMath.h>
#endif

#if __has_include(<DirectXPackedVector.h>)
#include <DirectXPackedVector.h>
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
struct ElementTraits<DirectX::PackedVector::XMBYTE2> : ElementTraitsBase<DirectX::PackedVector::XMBYTE2, AccessorType::Vec2, std::int8_t> {};

template<>
struct ElementTraits<DirectX::PackedVector::XMBYTE4> : ElementTraitsBase<DirectX::PackedVector::XMBYTE4, AccessorType::Vec4, std::int8_t> {};

template<>
struct ElementTraits<DirectX::PackedVector::XMUBYTE2> : ElementTraitsBase<DirectX::PackedVector::XMUBYTE2, AccessorType::Vec2, std::uint8_t> {};

template<>
struct ElementTraits<DirectX::PackedVector::XMUBYTE4> : ElementTraitsBase<DirectX::PackedVector::XMUBYTE4, AccessorType::Vec4, std::uint8_t> {};

template<>
struct ElementTraits<DirectX::PackedVector::XMSHORT2> : ElementTraitsBase<DirectX::PackedVector::XMSHORT2, AccessorType::Vec2, std::int16_t> {};

template<>
struct ElementTraits<DirectX::PackedVector::XMSHORT4> : ElementTraitsBase<DirectX::PackedVector::XMSHORT4, AccessorType::Vec4, std::int16_t> {};

template<>
struct ElementTraits<DirectX::PackedVector::XMUSHORT2> : ElementTraitsBase<DirectX::PackedVector::XMUSHORT2, AccessorType::Vec2, std::uint16_t> {};

template<>
struct ElementTraits<DirectX::PackedVector::XMUSHORT4> : ElementTraitsBase<DirectX::PackedVector::XMUSHORT4, AccessorType::Vec4, std::uint16_t> {};

template<>
struct ElementTraits<DirectX::XMUINT2> : ElementTraitsBase<DirectX::XMUINT2, AccessorType::Vec2, std::uint32_t> {};

template<>
struct ElementTraits<DirectX::XMUINT3> : ElementTraitsBase<DirectX::XMUINT3, AccessorType::Vec3, std::uint32_t> {};

template<>
struct ElementTraits<DirectX::XMUINT4> : ElementTraitsBase<DirectX::XMUINT4, AccessorType::Vec4, std::uint32_t> {};

template<>
struct ElementTraits<DirectX::XMFLOAT3X3> : TransposedElementTraits<DirectX::XMFLOAT3X3, AccessorType::Mat3, float> {};

template<>
struct ElementTraits<DirectX::XMFLOAT4X4> : TransposedElementTraits<DirectX::XMFLOAT4X4, AccessorType::Mat4, float> {};

} // namespace fastgltf
