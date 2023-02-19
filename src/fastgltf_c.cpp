#include <cassert>

#include <fastgltf_c.h>

#include <fastgltf_parser.hpp>
#include <fastgltf_types.hpp>

// Some checks to ensure some conversions are correct.
static_assert(DataSourceCount == std::variant_size_v<fastgltf::DataSource>,
    "fastgltf_data_source does not properly reflect the fastgltf::DataSource variant.");
static_assert(std::is_same_v<std::monostate, std::variant_alternative_t<DataSourceNone, fastgltf::DataSource>>);
static_assert(std::is_same_v<fastgltf::sources::BufferView, std::variant_alternative_t<DataSourceBufferView, fastgltf::DataSource>>);
static_assert(std::is_same_v<fastgltf::sources::FilePath, std::variant_alternative_t<DataSourceFilePath, fastgltf::DataSource>>);
static_assert(std::is_same_v<fastgltf::sources::Vector, std::variant_alternative_t<DataSourceVector, fastgltf::DataSource>>);
static_assert(std::is_same_v<fastgltf::sources::CustomBuffer, std::variant_alternative_t<DataSourceCustomBuffer, fastgltf::DataSource>>);

fastgltf_component_type fastgltf_get_component_type(unsigned int componentType) {
    return static_cast<fastgltf_component_type>(fastgltf::getComponentType(
        static_cast<std::underlying_type_t<fastgltf::ComponentType>>(componentType)));
}

fastgltf_accessor_type fastgltf_get_accessor_type(const char* string) {
    return static_cast<fastgltf_accessor_type>(fastgltf::getAccessorType(std::string_view { string }));
}

fastgltf_parser* fastgltf_create_parser(fastgltf_extensions extensions) {
    return reinterpret_cast<fastgltf_parser*>(
        new (std::nothrow) fastgltf::Parser(static_cast<fastgltf::Extensions>(extensions)));
}

void fastgltf_destroy_parser(fastgltf_parser* parser) {
    delete reinterpret_cast<fastgltf::Parser*>(parser);
}

fastgltf_gltf_data_buffer* fastgltf_create_gltf_data_buffer(unsigned char* bytes, size_t size) {
    auto data = new (std::nothrow) fastgltf::GltfDataBuffer();
    data->copyBytes(bytes, size);
    return reinterpret_cast<fastgltf_gltf_data_buffer*>(data);
}

fastgltf_gltf_data_buffer* fastgltf_create_gltf_data_buffer_from_path(const char* file) {
    auto data = new (std::nothrow) fastgltf::GltfDataBuffer();
    data->loadFromFile(std::string_view { file });
    return reinterpret_cast<fastgltf_gltf_data_buffer*>(data);
}

fastgltf_gltf_data_buffer* fastgltf_create_gltf_data_buffer_from_wpath(const wchar_t* file) {
    auto data = new (std::nothrow) fastgltf::GltfDataBuffer();
    data->loadFromFile(std::wstring_view { file });
    return reinterpret_cast<fastgltf_gltf_data_buffer*>(data);
}

void fastgltf_destroy_gltf_data_buffer(fastgltf_gltf_data_buffer* data) {
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

fastgltf_gltf* fastgltf_load_gltf_w(fastgltf_parser* parser, fastgltf_gltf_data_buffer* json, const wchar_t* directory, fastgltf_options options) {
    auto gltf = reinterpret_cast<fastgltf::Parser*>(parser)->loadGLTF(
        reinterpret_cast<fastgltf::GltfDataBuffer*>(json), std::wstring_view { directory }, static_cast<fastgltf::Options>(options));
    return reinterpret_cast<fastgltf_gltf*>(gltf.release());
}

fastgltf_gltf* fastgltf_load_binary_gltf_w(fastgltf_parser* parser, fastgltf_gltf_data_buffer* data, const wchar_t* directory, fastgltf_options options) {
    auto gltf = reinterpret_cast<fastgltf::Parser*>(parser)->loadBinaryGLTF(
        reinterpret_cast<fastgltf::GltfDataBuffer*>(data), std::wstring_view { directory }, static_cast<fastgltf::Options>(options));
    return reinterpret_cast<fastgltf_gltf*>(gltf.release());
}

void fastgltf_destroy_gltf(fastgltf_gltf* gltf) {
    delete reinterpret_cast<fastgltf::glTF*>(gltf);
}

fastgltf_error fastgltf_parse_all(fastgltf_gltf* gltf) {
    return static_cast<fastgltf_error>(
        reinterpret_cast<fastgltf::glTF*>(gltf)->parse());
}

fastgltf_error fastgltf_parse(fastgltf_gltf* gltf, fastgltf_category categories) {
    return static_cast<fastgltf_error>(
        reinterpret_cast<fastgltf::glTF*>(gltf)->parse(static_cast<fastgltf::Category>(categories)));
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

size_t fastgltf_get_buffer_count(fastgltf_asset* casset) {
    auto* asset = reinterpret_cast<fastgltf::Asset*>(casset);
    return asset->buffers.size();
}

fastgltf_buffer* fastgltf_get_buffer(fastgltf_asset* casset, size_t index) {
    auto* asset = reinterpret_cast<fastgltf::Asset*>(casset);
    assert(index <= asset->buffers.size());
    return reinterpret_cast<fastgltf_buffer*>(&asset->buffers.at(index));
}

size_t fastgltf_get_buffer_length(fastgltf_buffer* buffer) {
    return reinterpret_cast<fastgltf::Buffer*>(buffer)->byteLength;
}

fastgltf_data_source fastgltf_get_buffer_data_source_type(fastgltf_buffer* buffer) {
    return static_cast<fastgltf_data_source>(
        reinterpret_cast<fastgltf::Buffer*>(buffer)->data.index());
}

MimeType fastgltf_get_buffer_data_mime(fastgltf_buffer* buffer) {
    return std::visit([](auto& arg) {
        using namespace fastgltf::sources;
        using T = std::decay_t<decltype(arg)>;
        if constexpr (fastgltf::is_any<T, BufferView, FilePath, Vector, CustomBuffer>()) {
            return static_cast<::MimeType>(arg.mimeType);
        } else {
            return MimeTypeNone;
        }
    }, reinterpret_cast<fastgltf::Buffer*>(buffer)->data);
}

void fastgltf_buffer_data_get_buffer_view(fastgltf_buffer* buffer, size_t* bufferView) {
    auto* v = std::get_if<fastgltf::sources::BufferView>(
        &reinterpret_cast<fastgltf::Buffer*>(buffer)->data);
    assert(v != nullptr);
    *bufferView = v->bufferViewIndex;
}

void fastgltf_buffer_data_get_file_path(fastgltf_buffer* buffer, size_t* offset, const wchar_t** path) {
    auto* v = std::get_if<fastgltf::sources::FilePath>(
        &reinterpret_cast<fastgltf::Buffer*>(buffer)->data);
    assert(v != nullptr);
    *path = v->path.c_str();
    *offset = v->fileByteOffset;
}

void fastgltf_buffer_data_get_vector(fastgltf_buffer* buffer, size_t* size, uint8_t** data) {
    auto* v = std::get_if<fastgltf::sources::Vector>(
        &reinterpret_cast<fastgltf::Buffer*>(buffer)->data);
    assert(v != nullptr);
    *data = v->bytes.data();
    *size = v->bytes.size();
}

fastgltf_custom_buffer fastgltf_buffer_data_get_custom_buffer(fastgltf_buffer* buffer) {
    auto* v = std::get_if<fastgltf::sources::CustomBuffer>(
        &reinterpret_cast<fastgltf::Buffer*>(buffer)->data);
    assert(v != nullptr);
    return static_cast<fastgltf_custom_buffer>(v->id);
}
