#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <variant>

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

    /**
     * Represents the type of element in the buffer pointed to by the accessor.
     *
     * We encode these values with the number of components in their top 8 bits for fast
     * access & storage. Therefore, use the fastgltf::getNumComponents and fastgltf::getElementByteSize
     * functions to extract data from this enum.
     */
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

    /**
     * Represents the various types of components an accessor could point at. This describes the
     * format each component of the structure, which in return is described by fastgltf::AccessorType, is in.
     *
     * We use the top 16-bits to encode the amount of bits this component type needs.
     * The lower 16-bits are used to store the glTF ID for the type. Therefore, use the fastgltf::getComponentBitSize
     * and fastgltf::getGLComponentType functions should be used to extract data from this enum.
     */
    enum class ComponentType : uint32_t {
        Invalid         = 0,
        Byte            = ( 8 << 16) | 5120,
        UnsignedByte    = ( 8 << 16) | 5121,
        Short           = (16 << 16) | 5122,
        UnsignedShort   = (16 << 16) | 5123,
        /**
         * Signed integers are not officially allowed by the glTF spec, but are placed here for
         * the sake of completeness.
         */
        Int             = (32 << 16) | 5124,
        UnsignedInt     = (32 << 16) | 5125,
        Float           = (32 << 16) | 5126,
        /**
         * Doubles are not officially allowed by the glTF spec, but can be enabled by passing
         * Options::AllowDouble if you require it.
         */
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

    /**
     * Represents the intended OpenGL GPU buffer type to use with this buffer view.
     */
    enum class BufferTarget : uint16_t {
        ArrayBuffer = 34962,
        ElementArrayBuffer = 34963,
    };

    enum class MimeType : uint16_t {
        None = 0,
        JPEG = 1,
        PNG = 2,
        KTX2 = 3,
        DDS = 4,
        GltfBuffer = 5,
        OctetStream = 6,
    };

    enum class AnimationInterpolation : uint16_t {
        /**
         * The animated values are linearly interpolated between keyframes. When targeting a
         * rotation, spherical linear interpolation (slerp) SHOULD be used to interpolate quaternions.
         */
        Linear = 0,
        /**
         * The animated values remain constant to the output of the first keyframe, until the next
         * keyframe.
         */
        Step = 1,
        /**
         * The animation’s interpolation is computed using a cubic spline with specified tangents.
         * The number of output elements MUST equal three times the number of input elements. For
         * each input element, the output stores three elements, an in-tangent, a spline vertex,
         * and an out-tangent.
         */
        CubicSpline = 2,
    };

    enum class AnimationPath : uint16_t {
        /**
         * The values are the translation along the X, Y, and Z axes.
         */
        Translation = 1,
        /**
         * The values are a quaternion in the order x, y, z, w where w is the scalar.
         */
        Rotation = 2,
        /**
         * The values are scaling factors along the X, Y, and Z axes.
         */
        Scale = 3,
        Weights = 4,
    };

    enum class CameraType : uint8_t {
        Perspective = 0,
        Orthographic = 1,
    };

    enum class AlphaMode : uint8_t {
        Opaque,
        Mask,
        Blend,
    };

    enum class MeshoptCompressionMode : uint8_t {
        None = 0,
        Attributes,
        Triangles,
        Indices,
    };

    enum class MeshoptCompressionFilter : uint8_t {
        None = 0,
        Octahedral,
        Quaternion,
        Exponential,
    };
    // clang-format on
#pragma endregion

#pragma region ConversionFunctions
    /**
     * Gets the number of components for each element for the given accessor type. For example, with
     * a Vec3 accessor type this will return 3, as a Vec3 contains 3 components.
     */
    constexpr uint32_t getNumComponents(AccessorType type) noexcept {
        return static_cast<uint32_t>(
            (static_cast<decltype(std::underlying_type_t<AccessorType>())>(type) >> 8) & 0xFF);
    }

    constexpr uint32_t getComponentBitSize(ComponentType componentType) noexcept {
        auto masked =
            static_cast<decltype(std::underlying_type_t<ComponentType>())>(componentType) & 0xFFFF0000;
        return (masked >> 16);
    }

    constexpr uint32_t getElementByteSize(AccessorType type, ComponentType componentType) noexcept {
        return getNumComponents(type) * (getComponentBitSize(componentType) / 8);
    }

    constexpr uint32_t getGLComponentType(ComponentType type) noexcept {
        return to_underlying(type) & 0xFFFF;
    }

    /**
     * Don't use this, use getComponenType instead.
     * This order matters as we assume that their glTF constant is ascending to index it.
     */
    static constexpr std::array<ComponentType, 11> components = {
        ComponentType::Byte,
        ComponentType::UnsignedByte,
        ComponentType::Short,
        ComponentType::UnsignedShort,
        ComponentType::Int,
        ComponentType::UnsignedInt,
        ComponentType::Float,
        ComponentType::Invalid,
        ComponentType::Invalid,
        ComponentType::Invalid,
        ComponentType::Double,
    };

    constexpr ComponentType getComponentType(std::underlying_type_t<ComponentType> componentType) noexcept {
        size_t index = componentType - 5120;
        if (index >= components.size())
            return ComponentType::Invalid;
        return components[index];
    }

    // This order matters as we assume that their glTF constant is ascending to index it.
    static constexpr std::array<AccessorType, 7> accessorTypes = {
        AccessorType::Scalar,
        AccessorType::Vec2,
        AccessorType::Vec3,
        AccessorType::Vec4,
        AccessorType::Mat2,
        AccessorType::Mat3,
        AccessorType::Mat4,
    };

    /**
     * Gets the AccessorType by its string representation found in glTF files.
     */
    constexpr AccessorType getAccessorType(std::string_view accessorTypeName) noexcept {
        assert(!accessorTypeName.empty());
        switch (accessorTypeName[0]) {
            case 'S': return AccessorType::Scalar;
            case 'V': {
                auto componentCount = accessorTypeName[3] - '2';
                if (1ULL + componentCount >= accessorTypes.size())
                    return AccessorType::Invalid;
                return accessorTypes[1ULL + componentCount];
            }
            case 'M': {
                auto componentCount = accessorTypeName[3] - '2';
                if (1ULL + componentCount >= accessorTypes.size())
                    return AccessorType::Invalid;
                return accessorTypes[4ULL + componentCount];
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
        CustomBufferWithId,
    };

    struct DataSource {
        // Corresponds to DataLocation::BufferViewWithMime
        size_t bufferViewIndex;

        // Corresponds to DataLocation::FilePathWithByteRange
        size_t fileByteOffset;
        std::filesystem::path path;

        // Corresponds to DataLocation::VectorWithMime
        std::vector<uint8_t> bytes;

        // Defined if DataLocation::BufferViewWithMime or VectorWithMime
        MimeType mimeType;

        // Defined if DataLocation::CustomBufferWithId
        uint64_t bufferId;
    };

    struct AnimationChannel {
        size_t samplerIndex;
        size_t nodeIndex;
        AnimationPath path;
    };

    struct AnimationSampler {
        size_t inputAccessor;
        size_t outputAccessor;
        AnimationInterpolation interpolation;
    };

    struct Animation {
        std::vector<AnimationChannel> channels;
        std::vector<AnimationSampler> samplers;

        std::string name;
    };

    struct Camera {
        struct Orthographic {
            float xmag;
            float ymag;
            float zfar;
            float znear;
        };
        struct Perspective {
            std::optional<float> aspectRatio;
            float yfov;
            // If omitted, use an infinite projection matrix.
            std::optional<float> zfar;
            float znear;
        };

        /**
         * Variant holding either a perspective or a orthographic camera. Use std::holds_alternative
         * and/or std::get_if to figure out which camera type is being used.
         */
        std::variant<Perspective, Orthographic> camera;
        std::string name;
    };

    struct Skin {
        std::vector<size_t> joints;
        std::optional<size_t> skeleton;
        std::optional<size_t> inverseBindMatrices;

        std::string name;
    };

    struct Sampler {
        std::optional<Filter> magFilter;
        std::optional<Filter> minFilter;
        Wrap wrapS;
        Wrap wrapT;

        std::string name;
    };

    struct Scene {
        std::vector<size_t> nodeIndices;

        std::string name;
    };

    struct Node {
        std::optional<size_t> meshIndex;
        std::optional<size_t> skinIndex;
        std::optional<size_t> cameraIndex;
        std::vector<size_t> children;

        struct TRS {
            std::array<float, 3> translation;
            std::array<float, 4> rotation;
            std::array<float, 3> scale;
        };
        using TransformMatrix = std::array<float, 16>;

        /**
         * Variant holding either the three TRS components; transform, rotation, and scale, or a
         * transformation matrix, which cannot skew or shear. The latter can be decomposed into
         * the TRS components by specifying Options::DecomposeNodeMatrices.
         */
        std::variant<TRS, TransformMatrix> transform;

        std::string name;
    };

    struct Primitive {
        std::unordered_map<std::string, size_t> attributes;
        PrimitiveType type;

        std::optional<size_t> indicesAccessor;
        std::optional<size_t> materialIndex;
    };

    struct Mesh {
        std::vector<Primitive> primitives;

        std::string name;
    };

    struct TextureInfo {
        size_t textureIndex;
        size_t texCoordIndex;
        float scale;

        /**
         * UV counter-clockwise rotation in radians.
         * @note 0.0f unless KHR_texture_transform is specified and used by the glTF.
         */
        float rotation;
        /**
         * UV offset.
         * @note 0.0f unless KHR_texture_transform is specified and used by the glTF.
         */
        std::array<float, 2> uvOffset;
        /**
         * UV scale.
         * @note 1.0f unless KHR_texture_transform is specified and used by the glTF.
         */
        std::array<float, 2> uvScale;
    };

    struct PBRData {
        /**
         * The factors for the base color of then material. Defaults to 1,1,1,1
         */
        std::array<float, 4> baseColorFactor;

        /**
         * The factor fot eh metalness of the material. Defaults to 1
         */
        float metallicFactor;

        /**
         * The factor fot eh roughness of the material. Defaults to 1
         */
        float roughnessFactor;

        std::optional<TextureInfo> baseColorTexture;
        std::optional<TextureInfo> metallicRoughnessTexture;
    };

    struct Material {
        /**
         * A set of parameter values that are used to define the metallic-roughness material model
         * from Physically Based Rendering (PBR) methodology. When undefined, all the default
         * values of pbrMetallicRoughness MUST apply.
         */
        std::optional<PBRData> pbrData;

        /**
         * The tangent space normal texture.
         */
        std::optional<TextureInfo> normalTexture;
        std::optional<TextureInfo> occlusionTexture;
        std::optional<TextureInfo> emissiveTexture;

        /**
         * The factors for the emissive color of the material. Defaults to 0,0,0
         */
        std::array<float, 3> emissiveFactor;

        /**
         * The values used to determine the transparency of the material.
         * Defaults to Opaque, and 0.5 for alpha cutoff.
         */
        AlphaMode alphaMode;
        float alphaCutoff;

        /**
         * Determines whether back-face culling should be disabled when using this material.
         */
        bool doubleSided;

        std::string name;
    };

    struct Texture {
        std::optional<size_t> imageIndex;

        /**
         * If the imageIndex is specified by the KTX2 or DDS glTF extensions, this is supposed to
         * be used as a fallback if those file containers are not supported.
         */
        std::optional<size_t> fallbackImageIndex;

        /**
         * If no sampler is specified, use a default sampler with repeat wrap and auto filter.
         */
        std::optional<size_t> samplerIndex;

        std::string name;
    };

    struct Image {
        DataLocation location;
        DataSource data;

        std::string name;
    };

    struct Accessor {
        size_t byteOffset;
        size_t count;
        AccessorType type;
        ComponentType componentType;
        bool normalized = false;

        // Could have no value for sparse morph targets
        std::optional<size_t> bufferViewIndex;

        std::string name;
    };

    struct BufferView {
        size_t bufferIndex;
        size_t byteOffset;
        size_t byteLength;

        std::optional<size_t> byteStride;
        std::optional<BufferTarget> target;

        // From EXT_meshopt_compression
        std::optional<size_t> count;
        // From EXT_meshopt_compression
        std::optional<MeshoptCompressionMode> mode;
        // From EXT_meshopt_compression
        std::optional<MeshoptCompressionFilter> filter;

        std::string name;
    };

    struct Buffer {
        size_t byteLength;

        DataLocation location;
        DataSource data;

        std::string name;
    };

    struct Asset {
        std::optional<size_t> defaultScene;
        std::vector<Accessor> accessors;
        std::vector<Animation> animations;
        std::vector<Buffer> buffers;
        std::vector<BufferView> bufferViews;
        std::vector<Camera> cameras;
        std::vector<Image> images;
        std::vector<Material> materials;
        std::vector<Mesh> meshes;
        std::vector<Node> nodes;
        std::vector<Sampler> samplers;
        std::vector<Scene> scenes;
        std::vector<Skin> skins;
        std::vector<Texture> textures;

        explicit Asset() = default;
        explicit Asset(const Asset& scene) = delete;
        Asset& operator=(const Asset& scene) = delete;
    };
#pragma endregion
}
