#include <catch2/catch_test_macros.hpp>

#include <fastgltf_c.h>
#include "gltf_path.hpp"

TEST_CASE("Test basic C API", "[gltf-loader]") {
    auto cubeFolder = sampleModels / "2.0" / "Cube" / "glTF";
    auto cubePath = cubeFolder / "Cube.gltf";

    fastgltf_parser* parser = fastgltf_create_parser((fastgltf_extensions)0);
    fastgltf_gltf_data_buffer* data = fastgltf_create_gltf_data_buffer_from_wpath(cubePath.c_str());

    fastgltf_gltf* gltf = fastgltf_load_gltf_w(parser, data, cubeFolder.c_str(), OptionsDontRequireValidAssetMember);
    REQUIRE(fastgltf_get_parser_error(parser) == ErrorNone);
    REQUIRE(fastgltf_parse_all(gltf) == ErrorNone);

    fastgltf_destroy_parser(parser);
    fastgltf_destroy_gltf_data_buffer(data);

    fastgltf_asset* asset = fastgltf_get_parsed_asset(gltf);
    REQUIRE(asset != nullptr);
    fastgltf_destroy_gltf(gltf);

    auto bufferCount = fastgltf_get_buffer_count(asset);
    REQUIRE(bufferCount == 1);
    auto* buffer = fastgltf_get_buffer(asset, 0);
    REQUIRE(fastgltf_get_buffer_length(buffer) == 1800);
    auto sourceType = fastgltf_get_buffer_data_source_type(buffer);
    REQUIRE(sourceType == DataSourceFilePath);
    const wchar_t* bufferPath;
    size_t offset;
    fastgltf_buffer_data_get_file_path(buffer, &offset, &bufferPath);
    REQUIRE(offset == 0);
    REQUIRE(std::filesystem::exists(bufferPath));

    fastgltf_destroy_asset(asset);
}
