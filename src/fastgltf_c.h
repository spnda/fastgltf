enum fastgltf_extensions {
    KHR_texture_transform = 1 << 1,
    KHR_texture_basisu = 1 << 2,
    MSFT_texture_dds = 1 << 3,
    KHR_mesh_quantization = 1 << 4,
    EXT_meshopt_compression = 1 << 5,
};

enum fastgltf_options {
    OptionsAllowDouble = 1 << 0,
    OptionsDontRequireValidAssetMember = 1 << 1,
    OptionsDontUseSIMD = 1 << 2,
    OptionsLoadGLBBuffers = 1 << 3,
    OptionsLoadExternalBuffers = 1 << 4,
    OptionsDecomposeNodeMatrices = 1 << 5,
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

    CategoryBuffers = 1 << 0,
    CategoryBufferViews = 1 << 1 | CategoryBuffers,
    CategoryAccessors = 1 << 2 | CategoryBufferViews,
    CategoryImages = 1 << 3 | CategoryBufferViews,
    CategorySamplers = 1 << 4,
    CategoryTextures = 1 << 5 | CategoryImages | CategorySamplers,
    CategoryAnimations = 1 << 6 | CategoryAccessors,
    CategoryCameras = 1 << 7,
    CategoryMaterials = 1 << 8 | CategoryTextures,
    CategoryMeshes = 1 << 9 | CategoryAccessors | CategoryMaterials,
    CategorySkins = 1 << 10 | CategoryAccessors | (1 << 11),
    CategoryNodes = 1 << 11 | CategoryCameras | CategoryMeshes | CategorySkins,
    CategoryScenes = 1 << 12 | CategoryNodes,
    CategoryAsset = 1 << 13,

    CategoryAll = CategoryAsset | CategoryScenes | CategoryAnimations,
};

#define FASTGLTF_EXPORT

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fastgltf_parser_s fastgltf_parser;
typedef struct fastgltf_gltf_data_buffer_s fastgltf_gltf_data_buffer;
typedef struct fastgltf_gltf_s fastgltf_gltf;
typedef struct fastgltf_asset_s fastgltf_asset;

FASTGLTF_EXPORT fastgltf_parser* fastgltf_create_parser(fastgltf_extensions extensions);
FASTGLTF_EXPORT void fastgltf_destroy_parser(fastgltf_parser* parser);

FASTGLTF_EXPORT fastgltf_gltf_data_buffer_s* fastgltf_create_gltf_data_buffer(unsigned char* bytes, size_t size);
FASTGLTF_EXPORT fastgltf_gltf_data_buffer_s* fastgltf_create_gltf_data_buffer_from_file(const char* filePath);
FASTGLTF_EXPORT void fastgltf_destroy_json_data(fastgltf_gltf_data_buffer* data);

FASTGLTF_EXPORT fastgltf_error fastgltf_get_parser_error(fastgltf_parser* parser);
FASTGLTF_EXPORT fastgltf_gltf* fastgltf_load_gltf(fastgltf_parser* parser, fastgltf_gltf_data_buffer* json, const char* directory, fastgltf_options options);
FASTGLTF_EXPORT fastgltf_gltf* fastgltf_load_binary_gltf(fastgltf_parser* parser, fastgltf_gltf_data_buffer* data, const char* directory, fastgltf_options options);
FASTGLTF_EXPORT void fastgltf_destroy_gltf(fastgltf_gltf* gltf);

FASTGLTF_EXPORT fastgltf_error fastgltf_parse(fastgltf_gltf* gltf, fastgltf_category categories);

FASTGLTF_EXPORT fastgltf_asset* fastgltf_get_parsed_asset(fastgltf_gltf* gltf);
FASTGLTF_EXPORT void fastgltf_destroy_asset(fastgltf_asset* asset);

#ifdef __cplusplus
}
#endif

#undef FASTGLTF_EXPORT
