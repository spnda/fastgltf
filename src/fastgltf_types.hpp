#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <unordered_map>

#include "fastgltf_util.hpp"

namespace fastgltf {
#pragma region Enums
    // clang-format off
    enum class PrimitiveType : uint8_t {
        Points = 0,
        Lines = 1,
        LineLoop = 2,
        LineStrip = 3,
        Triangles = 4,
        TriangleStrip = 5,
        TriangleFan = 6,
    };

    // We encode these values with the number of components in their top 8 bits.
    constexpr uint16_t accessorTypeMask = 0x00FF;
    constexpr uint16_t accessorTypeBitSizeMask = 0xFF00;
    enum class AccessorType : uint16_t {
        Invalid = 0,
        Scalar  = ( 1 << 8) | 1,
        Vec2    = ( 2 << 8) | 2,
        Vec3    = ( 3 << 8) | 3,
        Vec4    = ( 4 << 8) | 4,
        Mat2    = ( 4 << 8) | 5,
        Mat3    = ( 9 << 8) | 6,
        Mat4    = (16 << 8) | 7,
    };

    // We use the top 32-bits to encode the amount of bits this component type needs.
    // The lower 32-bits are used to store the glTF ID for the type.
    constexpr uint32_t componentTypeMask = 0xFFFF;
    constexpr uint32_t componentTypeBitSizeMask = 0xFFFF0000;
    enum class ComponentType : uint32_t {
        Invalid         = 0,
        Byte            = ( 8 << 16) | 5120,
        UnsignedByte    = ( 8 << 16) | 5121,
        Short           = (16 << 16) | 5122,
        UnsignedShort   = (16 << 16) | 5123,
        UnsignedInt     = (32 << 16) | 5125,
        Float           = (32 << 16) | 5126,
        // Not officially in the glTF spec.
        Double          = (64 << 16) | 5130,
    };

    enum class Filter : uint16_t {
        Nearest = 9728,
        Linear = 9729,
        NearestMipMapNearest = 9984,
        LinearMipMapNearest = 9985,
        NearestMipMapLinear = 9986,
        LinearMipMapLinear = 9987,
    };

    enum class Wrap : uint16_t {
        ClampToEdge = 33071,
        MirroredRepeat = 33648,
        Repeat = 10497,
    };

    // Represents the intended GPU buffer type to use with this buffer view.
    enum class BufferTarget : uint16_t {
        ArrayBuffer = 34962,
        ElementArrayBuffer = 34963,
    };
    // clang-format on
#pragma endregion

#pragma region ConversionFunctions
    constexpr uint32_t getNumComponents(AccessorType type) noexcept {
        return (static_cast<uint16_t>(type) >> 8) & 0xFF;
    }

    constexpr uint32_t getComponentBitSize(ComponentType componentType) noexcept {
        auto masked = static_cast<uint32_t>(componentType) & componentTypeBitSizeMask;
        return (masked >> 16);
    }

    constexpr uint32_t getElementSize(AccessorType type, ComponentType componentType) noexcept {
        return getNumComponents(type) * (getComponentBitSize(componentType) / 8);
    }

    constexpr ComponentType getComponentType(std::underlying_type_t<ComponentType> componentType) noexcept {
        constexpr std::array<ComponentType, 7> components = {
            ComponentType::Byte,
            ComponentType::UnsignedByte,
            ComponentType::Short,
            ComponentType::UnsignedShort,
            ComponentType::UnsignedInt,
            ComponentType::Float,
            ComponentType::Double
        };
        // This shouldn't bee too slow if called multiple times when parsing...
        for (auto component : components) {
            if ((to_underlying(component) & componentTypeMask) == componentType) {
                return component;
            }
        }
        return ComponentType::Invalid;
    }

    constexpr AccessorType getAccessorType(std::string_view accessorTypeName) noexcept {
        constexpr std::array<std::pair<std::string_view, AccessorType>, 7> accessorPairs = {
            {
                {"SCALAR", AccessorType::Scalar},
                {"VEC2", AccessorType::Vec2},
                {"VEC3", AccessorType::Vec3},
                {"VEC4", AccessorType::Vec4},
                {"MAT2", AccessorType::Mat2},
                {"MAT3", AccessorType::Mat3},
                {"MAT4", AccessorType::Mat4},
            },
        };
        for (auto [key, value] : accessorPairs) {
            if (key == accessorTypeName) {
                return value;
            }
        }
        return AccessorType::Invalid;
    }
#pragma endregion

#pragma region Structs
    enum class DataLocation : uint8_t {
        // This should never occur.
        None = 0,
        VectorWithMime,
        BufferViewWithMime,
        FilePathWithByteRange,
    };

    struct DataSource {
        // Corresponds to DataLocation::BufferViewWithMime
        size_t bufferViewIndex;

        // Corresponds to DataLocation::FilePathWithByteRange
        std::filesystem::path path;

        // Corresponds to DataLocation::VectorWithMime
        std::vector<uint8_t> bytes;

        // Defined if DataLocation::BufferViewWithMime or VectorWithMime
        std::string mimeType;
    };

    struct Scene {
        std::string name;
        std::vector<size_t> nodeIndices;
    };

    struct Node {
        std::string name;
        size_t meshIndex;

        bool hasMatrix = false;
        std::array<float, 16> matrix;
    };

    struct Primitive {
        size_t indicesAccessorIndex;
        size_t materialIndex;
        PrimitiveType type;
        std::unordered_map<std::string, size_t> attributes;
    };

    struct Mesh {
        std::string name;
        std::vector<Primitive> primitives;
    };

    struct Texture {
        size_t imageIndex;
        // if numeric_limits<size_t>::max, use a default sampler with repeat wrap and auto filter
        size_t samplerIndex;
        std::string name;
    };

    struct Image {
        std::string name;

        DataLocation location;
        DataSource data;
    };

    struct Accessor {
        size_t bufferViewIndex;
        size_t byteOffset;
        size_t count;

        AccessorType type;
        ComponentType componentType;
        bool normalized = false;
        std::string name;
    };

    struct BufferView {
        size_t bufferIndex;
        size_t byteOffset;
        size_t byteLength;
        BufferTarget target = BufferTarget::ArrayBuffer;
        std::string name;
    };

    struct Buffer {
        size_t byteLength;
        std::string name;

        DataLocation location;
        DataSource data;
    };

    struct Asset {
        // A value of std::numeric_limits<size_t>::max() indicates no default scene.
        size_t defaultScene = std::numeric_limits<size_t>::max();
        std::vector<Scene> scenes;
        std::vector<Node> nodes;
        std::vector<Mesh> meshes;
        std::vector<Accessor> accessors;
        std::vector<BufferView> bufferViews;
        std::vector<Buffer> buffers;
        std::vector<Texture> textures;
        std::vector<Image> images;

        explicit Asset() = default;
        explicit Asset(const Asset& scene) = delete;
        Asset& operator=(const Asset& scene) = delete;
    };
#pragma endregion
}
