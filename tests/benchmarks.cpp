#include <fstream>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "simdjson.h"

#include "fastgltf_parser.hpp"
#include "gltf_path.hpp"

constexpr auto benchmarkOptions = fastgltf::Options::DontRequireValidAssetMember;

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
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(intelSponza / "NewSponza_Main_glTF_002.gltf"));

    BENCHMARK("Parse NewSponza") {
        auto sponza = parser.loadGLTF(jsonData.get(), intelSponza, benchmarkOptions);
        return sponza->parse();
    };
}

TEST_CASE("Benchmark base64 decoding from glTF file", "[gltf-benchmark]") {
    fastgltf::Parser parser;
    auto cylinderEngine = sampleModels / "2.0" / "2CylinderEngine" / "glTF-Embedded";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(cylinderEngine / "2CylinderEngine.gltf"));

    BENCHMARK("Parse 2CylinderEngine and decode base64") {
        auto engine = parser.loadGLTF(jsonData.get(), cylinderEngine, benchmarkOptions);
        return engine->parse();
    };
}

TEST_CASE("Benchmark raw JSON parsing", "[gltf-benchmark]") {
    fastgltf::Parser parser;
    auto buggyPath = sampleModels / "2.0" / "Buggy" / "glTF";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(buggyPath / "Buggy.gltf"));

    BENCHMARK("Parse Buggy.gltf") {
        auto buggy = parser.loadGLTF(jsonData.get(), buggyPath, benchmarkOptions);
        return buggy->parse();
    };
}

TEST_CASE("Benchmark massive gltf file", "[gltf-benchmark]") {
    if (!std::filesystem::exists(bistroPath / "bistro.gltf")) {
        // Bistro is not part of gltf-Sample-Models, and therefore not always available.
        SKIP("Amazon's Bistro (GLTF) is required for this benchmark.");
    }

    fastgltf::Parser parser(fastgltf::Extensions::KHR_mesh_quantization);
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(bistroPath / "bistro.gltf"));

    BENCHMARK("Parse Bistro") {
        auto engine = parser.loadGLTF(jsonData.get(), bistroPath, benchmarkOptions | fastgltf::Options::MinimiseJsonBeforeParsing);
        return engine->parse();
    };
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
