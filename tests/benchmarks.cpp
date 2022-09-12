#include <fstream>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "simdjson.h"

#include "fastgltf_parser.hpp"
#include "gltf_path.hpp"

constexpr auto benchmarkOptions = fastgltf::Options::DontRequireValidAssetMember;

#ifdef HAS_RAPIDJSON
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#endif

#ifdef HAS_TINYGLTF
// We don't want tinygltf to load/write images.
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_FS
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

bool tinygltf_FileExistsFunction([[maybe_unused]] const std::string& filename, [[maybe_unused]] void* user) {
    return true;
}

std::string tinygltf_ExpandFilePathFunction(const std::string& path, [[maybe_unused]] void* user) {
    return path;
}

bool tinygltf_ReadWholeFileFunction(std::vector<unsigned char>* data, std::string*, const std::string&, void*) {
    // tinygltf checks if size == 1. It also checks if the size is correct for glb files, but
    // well ignore that for now.
    data->resize(1);
    return true;
}

bool tinygltf_LoadImageData(tinygltf::Image *image, const int image_idx, std::string *err,
                   std::string *warn, int req_width, int req_height,
                   const unsigned char *bytes, int size, void *user_data) {
    return true;
}

void setTinyGLTFCallbacks(tinygltf::TinyGLTF& gltf) {
    gltf.SetFsCallbacks({
        tinygltf_FileExistsFunction,
        tinygltf_ExpandFilePathFunction,
        tinygltf_ReadWholeFileFunction,
        nullptr, nullptr,
    });
    gltf.SetImageLoader(tinygltf_LoadImageData, nullptr);
}
#endif

#ifdef HAS_CGLTF
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#endif

std::vector<uint8_t> readFileAsBytes(std::filesystem::path path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error(std::string { "Failed to open file: " } + path.string());

    auto fileSize = file.tellg();
    std::vector<uint8_t> bytes(static_cast<size_t>(fileSize) + fastgltf::getGltfBufferPadding());
    file.seekg(0, std::ifstream::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), fileSize);
    file.close();
    return bytes;
}

TEST_CASE("Benchmark loading of NewSponza", "[gltf-benchmark]") {
    if (!std::filesystem::exists(intelSponza / "NewSponza_Main_glTF_002.gltf")) {
        // NewSponza is not part of gltf-Sample-Models, and therefore not always available.
        SKIP("Intel's NewSponza (GLTF) is required for this benchmark.");
    }

    fastgltf::Parser parser;
#ifdef HAS_TINYGLTF
    tinygltf::TinyGLTF tinygltf;
    tinygltf::Model model;
    std::string warn, err;
#endif

    auto bytes = readFileAsBytes(intelSponza / "NewSponza_Main_glTF_002.gltf");
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->fromByteView(bytes.data(), bytes.size() - fastgltf::getGltfBufferPadding(), bytes.size()));

    BENCHMARK("Parse NewSponza") {
        auto sponza = parser.loadGLTF(jsonData.get(), intelSponza, benchmarkOptions);
        return sponza->parse();
    };

#ifdef HAS_TINYGLTF
    setTinyGLTFCallbacks(tinygltf);
    BENCHMARK("Parse NewSponza with tinygltf") {
        return tinygltf.LoadASCIIFromString(&model, &err, &warn, reinterpret_cast<char*>(bytes.data()), bytes.size(), intelSponza.string());
    };
#endif

#ifdef HAS_CGLTF
    auto filePath = (path / "NewSponza_Main_glTF_002.gltf").string();
    BENCHMARK("Parse NewSponza with cgltf") {
        cgltf_options options = {};
        cgltf_data* data = nullptr;
        cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &data);
        if (result != cgltf_result_success) {
            cgltf_free(data);
            return result;
        }

        return result;
    };
#endif
}

TEST_CASE("Benchmark base64 decoding from glTF file", "[gltf-benchmark]") {
    fastgltf::Parser parser;
#ifdef HAS_TINYGLTF
    tinygltf::TinyGLTF tinygltf;
    tinygltf::Model model;
    std::string warn, err;
#endif

    auto cylinderEngine = sampleModels / "2.0" / "2CylinderEngine" / "glTF-Embedded";
    auto bytes = readFileAsBytes(cylinderEngine / "2CylinderEngine.gltf");
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->fromByteView(bytes.data(), bytes.size() - fastgltf::getGltfBufferPadding(), bytes.size()));

    BENCHMARK("Parse 2CylinderEngine and decode base64") {
        auto engine = parser.loadGLTF(jsonData.get(), cylinderEngine, benchmarkOptions);
        return engine->parse();
    };

#ifdef HAS_TINYGLTF
    setTinyGLTFCallbacks(tinygltf);
    BENCHMARK("2CylinderEngine decode with tinygltf") {
        return tinygltf.LoadASCIIFromString(&model, &err, &warn, reinterpret_cast<char*>(bytes.data()), bytes.size(), cylinderEngine.string());
    };
#endif

#ifdef HAS_CGLTF
    BENCHMARK("2CylinderEngine decode with cgltf") {
        cgltf_options options = {};
        cgltf_data* data = nullptr;
        auto filePath = cylinderEngine.string();
        cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &data);
        if (result == cgltf_result_success) {
            result = cgltf_load_buffers(&options, data, filePath.c_str());
        }
        cgltf_free(data);
        return result;
    };
#endif
}

TEST_CASE("Benchmark raw JSON parsing", "[gltf-benchmark]") {
    fastgltf::Parser parser;
#ifdef HAS_TINYGLTF
    tinygltf::TinyGLTF tinygltf;
    tinygltf::Model model;
    std::string warn, err;
#endif

    auto buggyPath = sampleModels / "2.0" / "Buggy" / "glTF";
    auto bytes = readFileAsBytes(buggyPath / "Buggy.gltf");
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->fromByteView(bytes.data(), bytes.size() - fastgltf::getGltfBufferPadding(), bytes.size()));

    BENCHMARK("Parse Buggy.gltf") {
        auto buggy = parser.loadGLTF(jsonData.get(), buggyPath, benchmarkOptions);
        return buggy->parse();
    };

#ifdef HAS_TINYGLTF
    setTinyGLTFCallbacks(tinygltf);
    BENCHMARK("Parse Buggy.gltf with tinygltf") {
        return tinygltf.LoadASCIIFromString(&model, &err, &warn, reinterpret_cast<char*>(bytes.data()), bytes.size(), buggyPath.string());
    };
#endif

#ifdef HAS_CGLTF
    BENCHMARK("Parse Buggy.gltf with cgltf") {
        cgltf_options options = {};
        cgltf_data* data = nullptr;
        auto filePath = buggyPath.string();
        cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &data);
        if (result != cgltf_result_success) {
            cgltf_free(data);
            return result;
        }

        return result;
    };
#endif
}

TEST_CASE("Benchmark massive gltf file", "[gltf-benchmark]") {
    if (!std::filesystem::exists(bistroPath / "bistro.gltf")) {
        // Bistro is not part of gltf-Sample-Models, and therefore not always available.
        SKIP("Amazon's Bistro (GLTF) is required for this benchmark.");
    }

    fastgltf::Parser parser(fastgltf::Extensions::KHR_mesh_quantization);
#ifdef HAS_TINYGLTF
    tinygltf::TinyGLTF tinygltf;
    tinygltf::Model model;
    std::string warn, err;
#endif

    auto bytes = readFileAsBytes(bistroPath / "bistro.gltf");
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->fromByteView(bytes.data(), bytes.size() - fastgltf::getGltfBufferPadding(), bytes.size()));

    BENCHMARK("Parse Bistro") {
        auto engine = parser.loadGLTF(jsonData.get(), bistroPath, benchmarkOptions | fastgltf::Options::MinimiseJsonBeforeParsing);
        return engine->parse();
    };

#ifdef HAS_TINYGLTF
    setTinyGLTFCallbacks(tinygltf);
    BENCHMARK("Parse Bistro with tinygltf") {
        return tinygltf.LoadASCIIFromString(&model, &err, &warn, reinterpret_cast<char*>(bytes.data()), bytes.size(), bistroPath.string());
    };
#endif

#ifdef HAS_CGLTF
    BENCHMARK("Parse Bistro with cgltf") {
        cgltf_options options = {};
        cgltf_data* data = nullptr;
        auto filePath = bistroPath.string();
        cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &data);
        if (result != cgltf_result_success) {
            cgltf_free(data);
            return result;
        }

        return result;
    };
#endif
}

TEST_CASE("Compare parsing performance with minified documents", "[gltf-benchmark]") {
    auto buggyPath = sampleModels / "2.0" / "Buggy" / "glTF";
    auto bytes = readFileAsBytes(buggyPath / "Buggy.gltf");
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->fromByteView(bytes.data(), bytes.size() - fastgltf::getGltfBufferPadding(), bytes.size()));

    // Create a minified JSON string
    std::vector<uint8_t> minified(bytes.size());
    size_t dstLen = 0;
    auto result = simdjson::minify(reinterpret_cast<const char*>(bytes.data()), bytes.size(),
                                   reinterpret_cast<char*>(minified.data()), dstLen);
    REQUIRE(result == simdjson::SUCCESS);
    minified.resize(dstLen);

    // For completeness, benchmark minifying the JSON
    BENCHMARK("Minify Buggy.gltf") {
        auto result = simdjson::minify(reinterpret_cast<const char*>(bytes.data()), bytes.size(),
                                       reinterpret_cast<char*>(minified.data()), dstLen);
        REQUIRE(result == simdjson::SUCCESS);
        return result;
    };

    auto minifiedJsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(minifiedJsonData->fromByteView(minified.data(), minified.size() - fastgltf::getGltfBufferPadding(), minified.size()));

    fastgltf::Parser parser;
    BENCHMARK("Parse Buggy.gltf with normal JSON") {
        auto buggy = parser.loadGLTF(jsonData.get(), buggyPath, benchmarkOptions);
        return buggy->parse();
    };

    BENCHMARK("Parse Buggy.gltf with minified JSON") {
        auto buggy = parser.loadGLTF(minifiedJsonData.get(), buggyPath, benchmarkOptions);
        return buggy->parse();
    };
}
