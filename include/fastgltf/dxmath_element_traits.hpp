#pragma once

#include <fastgltf/tools.hpp>

#if __has_include(<directxmath.h>)
#include <directxmath.h>
#endif

namespace fastgltf {

template <>
struct ElementTraits<DirectX::XMFLOAT2> : ElementTraitsBase<DirectX::XMFLOAT2, AccessorType::Vec2, float> {};

template <>
struct ElementTraits<DirectX::XMFLOAT3> : ElementTraitsBase<DirectX::XMFLOAT3, AccessorType::Vec3, float> {};

template <>
struct ElementTraits<DirectX::XMFLOAT4> : ElementTraitsBase<DirectX::XMFLOAT4, AccessorType::Vec4, float> {};

} // namespace fastgltf
