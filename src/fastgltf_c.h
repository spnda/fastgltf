#ifndef FASTGLTF_C_H
#define FASTGLTF_C_H

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

enum fastgltf_extensions {
    KHR_texture_transform = 1 << 1,
    KHR_texture_basisu = 1 << 2,
    MSFT_texture_dds = 1 << 3,
    KHR_mesh_quantization = 1 << 4,
    EXT_meshopt_compression = 1 << 5,
    KHR_lights_punctual = 1 << 6,
    EXT_mesh_gpu_instancing = 1 << 7,
    EXT_texture_webp = 1 << 8,
};

enum fastgltf_options {
    OptionsAllowDouble = 1 << 0,
    OptionsDontRequireValidAssetMember = 1 << 1,
    OptionsLoadGLBBuffers = 1 << 3,
    OptionsLoadExternalBuffers = 1 << 4,
    OptionsDecomposeNodeMatrices = 1 << 5,
    OptionsMinimiseJsonBeforeParsing = 1 << 6,
};

enum fastgltf_error {
    ErrorNone = 0,
    ErrorInvalidPath = 1,
    ErrorMissingExtensions = 2,
    ErrorUnsupportedExtensions = 3,
    ErrorInvalidJson = 4,
    ErrorInvalidGltf = 5,
    ErrorInvalidOrMissingAssetField = 6,
    ErrorInvalidGLB = 6,
    ErrorMissingField = 7,
    ErrorMissingExternalBuffer = 8,
    ErrorUnsupportedVersion = 9,
};

enum fastgltf_category {
    CategoryNone = 0,

    CategoryBuffers     = 1 << 0,
    CategoryBufferViews = 1 << 1 | CategoryBuffers,
    CategoryAccessors   = 1 << 2 | CategoryBufferViews,
    CategoryImages      = 1 << 3 | CategoryBufferViews,
    CategorySamplers    = 1 << 4,
    CategoryTextures    = 1 << 5 | CategoryImages | CategorySamplers,
    CategoryAnimations  = 1 << 6 | CategoryAccessors,
    CategoryCameras     = 1 << 7,
    CategoryMaterials   = 1 << 8 | CategoryTextures,
    CategoryMeshes      = 1 << 9 | CategoryAccessors | CategoryMaterials,
    CategorySkins       = 1 << 10 | CategoryAccessors | (1 << 11),
    CategoryNodes       = 1 << 11 | CategoryCameras | CategoryMeshes | CategorySkins,
    CategoryScenes      = 1 << 12 | CategoryNodes,
    CategoryAsset       = 1 << 13,

    CategoryAll         = CategoryAsset | CategoryScenes | CategoryAnimations,
};

// These indices here correlate to the index of the corresponding type in the std::variant
// declaration of fastgltf::DataSource. These need to be updated accordingly.
enum fastgltf_data_source {
    DataSourceNone = 0,
    DataSourceBufferView = 1,
    DataSourceFilePath = 2,
    DataSourceVector = 3,
    DataSourceCustomBuffer = 4,

    DataSourceCount = 5,
};

enum fastgltf_primitive_type {
    PrimitiveTypePoints         = 0,
    PrimitiveTypeLines          = 1,
    PrimitiveTypeLineLoop       = 2,
    PrimitiveTypeLineStrip      = 3,
    PrimitiveTypeTriangles      = 4,
    PrimitiveTypeTriangleStrip  = 5,
    PrimitiveTypeTriangleFan    = 6,
};

enum fastgltf_accessor_type {
    AccessorTypeInvalid = 0,
    AccessorTypeScalar  = (1 << 8) | 1,
    AccessorTypeVec2    = (2 << 8) | 2,
    AccessorTypeVec3    = (3 << 8) | 3,
    AccessorTypeVec4    = ( 4 << 8) | 4,
    AccessorTypeMat2    = ( 4 << 8) | 5,
    AccessorTypeMat3    = ( 9 << 8) | 6,
    AccessorTypeMat4    = (16 << 8) | 7,
};

enum fastgltf_component_type {
    ComponentTypeInvalid         = 0,
    ComponentTypeByte            = ( 8 << 16) | 5120,
    ComponentTypeUnsignedByte    = ( 8 << 16) | 5121,
    ComponentTypeShort           = (16 << 16) | 5122,
    ComponentTypeUnsignedShort   = (16 << 16) | 5123,
    ComponentTypeUnsignedInt     = (32 << 16) | 5125,
    ComponentTypeFloat           = (32 << 16) | 5126,
    ComponentTypeDouble          = (64 << 16) | 5130,
};

enum fastgltf_filter {
    FilterNearest = 9728,
    FilterLinear = 9729,
    FilterNearestMipMapNearest = 9984,
    FilterLinearMipMapNearest = 9985,
    FilterNearestMipMapLinear = 9986,
    FilterLinearMipMapLinear = 9987,
};

enum fastgltf_wrap {
    WrapClampToEdge = 33071,
    WrapMirroredRepeat = 33648,
    WrapRepeat = 10497,
};

enum BufferTarget {
    BufferTargetArrayBuffer = 34962,
    BufferTargetElementArrayBuffer = 34963,
};

enum MimeType {
    MimeTypeNone = 0,
    MimeTypeJPEG = 1,
    MimeTypePNG = 2,
    MimeTypeKTX2 = 3,
    MimeTypeDDS = 4,
    MimeTypeGltfBuffer = 5,
    MimeTypeOctetStream = 6,
};

enum AnimationInterpolation {
    AnimationInterpolationLinear = 0,
    AnimationInterpolationStep = 1,
    AnimationInterpolationCubicSpline = 2,
};

enum AnimationPath {
    AnimationPathTranslation = 1,
    AnimationPathRotation = 2,
    AnimationPathScale = 3,
    AnimationPathWeights = 4,
};

enum CameraType {
    CameraTypePerspective = 0,
    CameraTypeOrthographic = 1,
};

enum AlphaMode {
    AlphaModeOpaque = 0,
    AlphaModeMask = 1,
    AlphaModeBlend = 2,
};

enum MeshoptCompressionMode {
    MeshoptCompressionModeNone = 0,
    MeshoptCompressionModeAttributes = 1,
    MeshoptCompressionModeTriangles = 2,
    MeshoptCompressionModeIndices = 3,
};

enum MeshoptCompressionFilter {
    MeshoptCompressionFilterNone = 0,
    MeshoptCompressionFilterOctahedral = 1,
    MeshoptCompressionFilterQuaternion = 2,
    MeshoptCompressionFilterExponential = 3,
};

enum LighType {
    LighTypeDirectional = 0,
    LighTypeSpot = 1,
    LighTypePoint = 2,
};

inline unsigned int getNumComponents(fastgltf_accessor_type type) {
    return (type >> 8) & 0xFF;
}

inline unsigned int getComponentBitSize(fastgltf_component_type type) {
    return (type & 0xFFFF0000) >> 16;
}

inline unsigned int getElementByteSize(fastgltf_accessor_type type, fastgltf_component_type componentType) {
    return getNumComponents(type) * (getComponentBitSize(componentType) / 8);
}

inline unsigned int getGLComponentType(fastgltf_component_type type) {
    return type & 0xFFFF;
}

#define FASTGLTF_EXPORT

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fastgltf_parser_s fastgltf_parser;
typedef struct fastgltf_gltf_data_buffer_s fastgltf_gltf_data_buffer;
typedef struct fastgltf_gltf_s fastgltf_gltf;
typedef struct fastgltf_asset_s fastgltf_asset;

typedef uint64_t fastgltf_custom_buffer;

typedef struct fastgltf_accessor_s fastgltf_accessor;
typedef struct fastgltf_animation_s fastgltf_animation;
typedef struct fastgltf_buffer_s fastgltf_buffer;
typedef struct fastgltf_buffer_view_s fastgltf_buffer_view;
typedef struct fastgltf_camera_s fastgltf_camera;
typedef struct fastgltf_image_s fastgltf_image;
typedef struct fastgltf_light_s fastgltf_light;
typedef struct fastgltf_material_s fastgltf_material;
typedef struct fastgltf_mesh_s fastgltf_mesh;
typedef struct fastgltf_node_s fastgltf_node;
typedef struct fastgltf_sampler_s fastgltf_sampler;
typedef struct fastgltf_scene_s fastgltf_scene;
typedef struct fastgltf_skin_s fastgltf_skin;
typedef struct fastgltf_texture_s fastgltf_texture;

struct fastgltf_array {
    void* data;
    size_t size;
};

FASTGLTF_EXPORT fastgltf_component_type fastgltf_get_component_type(unsigned int componentType);
FASTGLTF_EXPORT fastgltf_accessor_type fastgltf_get_accessor_type(const char* string);

FASTGLTF_EXPORT fastgltf_parser* fastgltf_create_parser(fastgltf_extensions extensions);
FASTGLTF_EXPORT void fastgltf_destroy_parser(fastgltf_parser* parser);

FASTGLTF_EXPORT fastgltf_gltf_data_buffer_s* fastgltf_create_gltf_data_buffer(unsigned char* bytes, size_t size);
FASTGLTF_EXPORT fastgltf_gltf_data_buffer_s* fastgltf_create_gltf_data_buffer_from_path(const char* filePath);
FASTGLTF_EXPORT fastgltf_gltf_data_buffer_s* fastgltf_create_gltf_data_buffer_from_wpath(const wchar_t* filePath);
FASTGLTF_EXPORT void fastgltf_destroy_gltf_data_buffer(fastgltf_gltf_data_buffer* data);

FASTGLTF_EXPORT fastgltf_error fastgltf_get_parser_error(fastgltf_parser* parser);
FASTGLTF_EXPORT fastgltf_gltf* fastgltf_load_gltf(fastgltf_parser* parser, fastgltf_gltf_data_buffer* json, const char* directory, fastgltf_options options);
FASTGLTF_EXPORT fastgltf_gltf* fastgltf_load_binary_gltf(fastgltf_parser* parser, fastgltf_gltf_data_buffer* data, const char* directory, fastgltf_options options);
FASTGLTF_EXPORT fastgltf_gltf* fastgltf_load_gltf_w(fastgltf_parser* parser, fastgltf_gltf_data_buffer* json, const wchar_t* directory, fastgltf_options options);
FASTGLTF_EXPORT fastgltf_gltf* fastgltf_load_binary_gltf_w(fastgltf_parser* parser, fastgltf_gltf_data_buffer* data, const wchar_t* directory, fastgltf_options options);
FASTGLTF_EXPORT void fastgltf_destroy_gltf(fastgltf_gltf* gltf);

FASTGLTF_EXPORT fastgltf_error fastgltf_parse_all(fastgltf_gltf* gltf);
FASTGLTF_EXPORT fastgltf_error fastgltf_parse(fastgltf_gltf* gltf, fastgltf_category categories);

FASTGLTF_EXPORT fastgltf_asset* fastgltf_get_parsed_asset(fastgltf_gltf* gltf);
FASTGLTF_EXPORT void fastgltf_destroy_asset(fastgltf_asset* asset);

FASTGLTF_EXPORT size_t fastgltf_get_buffer_count(fastgltf_asset* asset);
FASTGLTF_EXPORT fastgltf_buffer* fastgltf_get_buffer(fastgltf_asset* asset, size_t index);
FASTGLTF_EXPORT size_t fastgltf_get_buffer_length(fastgltf_buffer* buffer);
FASTGLTF_EXPORT fastgltf_data_source fastgltf_get_buffer_data_source_type(fastgltf_buffer* buffer);
FASTGLTF_EXPORT MimeType fastgltf_get_buffer_data_mime(fastgltf_buffer* buffer);
// Only call this when fastgltf_get_buffer_data_source_type returned DataSourceBufferView.
FASTGLTF_EXPORT void fastgltf_buffer_data_get_buffer_view(fastgltf_buffer* buffer, size_t* bufferView);
// Only call this when fastgltf_get_buffer_data_source_type returned DataSourceFilePath
FASTGLTF_EXPORT void fastgltf_buffer_data_get_file_path(fastgltf_buffer* buffer, size_t* offset, const wchar_t** path);
// Only call this when fastgltf_get_buffer_data_source_type returned DataSourceBufferVector.
FASTGLTF_EXPORT void fastgltf_buffer_data_get_vector(fastgltf_buffer* buffer, size_t* size, uint8_t** data);
// Only call this when fastgltf_get_buffer_data_source_type returned DataSourceCustomBuffer.
FASTGLTF_EXPORT fastgltf_custom_buffer fastgltf_buffer_data_get_custom_buffer(fastgltf_buffer* buffer);

#ifdef __cplusplus
}
#endif

#undef FASTGLTF_EXPORT

#endif
