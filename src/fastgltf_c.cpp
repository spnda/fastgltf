#include <fastgltf_c.h>

#include <fastgltf_parser.hpp>
#include <fastgltf_types.hpp>

fastgltf_parser* fastgltf_create_parser(fastgltf_extensions extensions) {
    return reinterpret_cast<fastgltf_parser*>(
        new fastgltf::Parser(static_cast<fastgltf::Extensions>(extensions)));
}

void fastgltf_destroy_parser(fastgltf_parser* parser) {
    delete reinterpret_cast<fastgltf::Parser*>(parser);
}

fastgltf_gltf_data_buffer* fastgltf_create_gltf_data_buffer(unsigned char* bytes, size_t size) {
    auto data = new (std::nothrow) fastgltf::GltfDataBuffer();
    data->copyBytes(bytes, size);
    return reinterpret_cast<fastgltf_gltf_data_buffer*>(data);
}

fastgltf_gltf_data_buffer* fastgltf_create_gltf_data_buffer_from_file(const char* file) {
    auto data = new (std::nothrow) fastgltf::GltfDataBuffer();
    data->loadFromFile(std::string_view { file });
    return reinterpret_cast<fastgltf_gltf_data_buffer*>(data);
}

void fastgltf_destroy_json_data(fastgltf_gltf_data_buffer* data) {
    delete reinterpret_cast<fastgltf::GltfDataBuffer*>(data);
}

fastgltf_error fastgltf_get_parser_error(fastgltf_parser* parser) {
    return static_cast<fastgltf_error>(
        reinterpret_cast<fastgltf::Parser*>(parser)->getError());
}

fastgltf_gltf* fastgltf_load_gltf(fastgltf_parser* parser, fastgltf_gltf_data_buffer* json, const char* directory, fastgltf_options options) {
    auto gltf = reinterpret_cast<fastgltf::Parser*>(parser)->loadGLTF(
        reinterpret_cast<fastgltf::GltfDataBuffer*>(json), std::string_view { directory }, static_cast<fastgltf::Options>(options));
    return reinterpret_cast<fastgltf_gltf*>(gltf.release());
}

fastgltf_gltf* fastgltf_load_binary_gltf(fastgltf_parser* parser, fastgltf_gltf_data_buffer* data, const char* directory, fastgltf_options options) {
    auto gltf = reinterpret_cast<fastgltf::Parser*>(parser)->loadBinaryGLTF(
        reinterpret_cast<fastgltf::GltfDataBuffer*>(data), std::string_view { directory }, static_cast<fastgltf::Options>(options));
    return reinterpret_cast<fastgltf_gltf*>(gltf.release());
}

void fastgltf_destroy_gltf(fastgltf_gltf* gltf) {
    delete reinterpret_cast<fastgltf::glTF*>(gltf);
}

fastgltf_error fastgltf_parse(fastgltf_gltf* gltf, fastgltf_category categories) {
    return static_cast<fastgltf_error>(reinterpret_cast<fastgltf::glTF*>(gltf)->parse(static_cast<fastgltf::Category>(categories)));
}

fastgltf_asset* fastgltf_get_parsed_asset(fastgltf_gltf* gltf) {
    // Obtain the unique_ptr from the glTF and release it. Otherwise, it gets destroyed with the
    // destructor of fastgltf::glTF.
    auto asset = reinterpret_cast<fastgltf::glTF*>(gltf)->getParsedAsset();
    return reinterpret_cast<fastgltf_asset*>(asset.release());
}

void fastgltf_destroy_asset(fastgltf_asset* asset) {
    delete reinterpret_cast<fastgltf::Asset*>(asset);
}
