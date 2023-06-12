#pragma once

#include <fastgltf/tools.hpp>

// If we find glm in the default include path, we'll also include it ourselfs.
// However, it is generally expected that the user will include glm before including this header.
#if __has_include(<glm/glm.hpp>)
#include <glm/glm.hpp>
#endif

namespace fastgltf {

template<>
struct fastgltf::ElementTraits<glm::vec2> : fastgltf::ElementTraitsBase<glm::vec2, AccessorType::Vec2, float> {};

template<>
struct fastgltf::ElementTraits<glm::vec3> : fastgltf::ElementTraitsBase<glm::vec3, AccessorType::Vec3, float> {};

template<>
struct fastgltf::ElementTraits<glm::vec4> : fastgltf::ElementTraitsBase<glm::vec4, AccessorType::Vec4, float> {};

template<>
struct fastgltf::ElementTraits<glm::i8vec2> : fastgltf::ElementTraitsBase<glm::i8vec2, AccessorType::Vec2, std::int8_t> {};

template<>
struct fastgltf::ElementTraits<glm::i8vec3> : fastgltf::ElementTraitsBase<glm::i8vec3, AccessorType::Vec3, std::int8_t> {};

template<>
struct fastgltf::ElementTraits<glm::i8vec4> : fastgltf::ElementTraitsBase<glm::i8vec4, AccessorType::Vec4, std::int8_t> {};

template<>
struct fastgltf::ElementTraits<glm::u8vec2> : fastgltf::ElementTraitsBase<glm::u8vec2, AccessorType::Vec2, std::uint8_t> {};

template<>
struct fastgltf::ElementTraits<glm::u8vec3> : fastgltf::ElementTraitsBase<glm::u8vec3, AccessorType::Vec3, std::uint8_t> {};

template<>
struct fastgltf::ElementTraits<glm::u8vec4> : fastgltf::ElementTraitsBase<glm::u8vec4, AccessorType::Vec4, std::uint8_t> {};

template<>
struct fastgltf::ElementTraits<glm::i16vec2> : fastgltf::ElementTraitsBase<glm::i16vec2, AccessorType::Vec2, std::int16_t> {};

template<>
struct fastgltf::ElementTraits<glm::i16vec3> : fastgltf::ElementTraitsBase<glm::i16vec3, AccessorType::Vec3, std::int16_t> {};

template<>
struct fastgltf::ElementTraits<glm::i16vec4> : fastgltf::ElementTraitsBase<glm::i16vec4, AccessorType::Vec4, std::int16_t> {};

template<>
struct fastgltf::ElementTraits<glm::u16vec2> : fastgltf::ElementTraitsBase<glm::u16vec2, AccessorType::Vec2, std::uint16_t> {};

template<>
struct fastgltf::ElementTraits<glm::u16vec3> : fastgltf::ElementTraitsBase<glm::u16vec3, AccessorType::Vec3, std::uint16_t> {};

template<>
struct fastgltf::ElementTraits<glm::u16vec4> : fastgltf::ElementTraitsBase<glm::u16vec4, AccessorType::Vec4, std::uint16_t> {};

template<>
struct fastgltf::ElementTraits<glm::u32vec2> : fastgltf::ElementTraitsBase<glm::u32vec2, AccessorType::Vec2, std::uint32_t> {};

template<>
struct fastgltf::ElementTraits<glm::u32vec3> : fastgltf::ElementTraitsBase<glm::u32vec3, AccessorType::Vec3, std::uint32_t> {};

template<>
struct fastgltf::ElementTraits<glm::u32vec4> : fastgltf::ElementTraitsBase<glm::u32vec4, AccessorType::Vec4, std::uint32_t> {};

template<>
struct fastgltf::ElementTraits<glm::mat2> : fastgltf::ElementTraitsBase<glm::mat2, AccessorType::Mat2, float> {};

template<>
struct fastgltf::ElementTraits<glm::mat3> : fastgltf::ElementTraitsBase<glm::mat3, AccessorType::Mat3, float> {};

template<>
struct fastgltf::ElementTraits<glm::mat4> : fastgltf::ElementTraitsBase<glm::mat4, AccessorType::Mat4, float> {};

} // namespace fastgltf
