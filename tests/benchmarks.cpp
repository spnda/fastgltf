#include <fstream>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fastgltf_parser.hpp"
#include "fastgltf_types.hpp"
#include "gltf_path.hpp"

constexpr auto benchmarkOptions = fastgltf::Options::DontRequireValidAssetMember;
constexpr auto noSimdBenchmarkOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::DontUseSIMD;

TEST_CASE("Benchmark loading of NewSponza", "[gltf-benchmark]") {
    fastgltf::Parser parser;
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    jsonData->loadFromFile(intelSponza / "NewSponza_Main_glTF_002.gltf");

    BENCHMARK("Parse NewSponza with SIMD") {
        auto sponza = parser.loadGLTF(jsonData.get(), intelSponza, benchmarkOptions);
        return sponza->parse();
    };

    BENCHMARK("Parse NewSponza without SIMD") {
        auto sponza = parser.loadGLTF(jsonData.get(), intelSponza, noSimdBenchmarkOptions);
        return sponza->parse();
    };
}

TEST_CASE("Benchmark base64 decoding from glTF file", "[base64-benchmark]") {
    fastgltf::Parser parser;
    auto cylinderEngine = sampleModels / "2.0" / "2CylinderEngine" / "glTF-Embedded";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    jsonData->loadFromFile(cylinderEngine / "2CylinderEngine.gltf");

    BENCHMARK("Parse 2CylinderEngine and decode base64 with SIMD") {
        auto engine = parser.loadGLTF(jsonData.get(), cylinderEngine, benchmarkOptions);
        return engine->parse();
    };

    BENCHMARK("Parse 2CylinderEngine and decode base64 without SIMD") {
        auto engine = parser.loadGLTF(jsonData.get(), cylinderEngine, noSimdBenchmarkOptions);
        return engine->parse();
    };
};

TEST_CASE("Benchmark raw JSON parsing", "[gltf-benchmark]") {
    fastgltf::Parser parser;
    auto buggyPath = sampleModels / "2.0" / "Buggy" / "glTF";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    jsonData->loadFromFile(buggyPath / "Buggy.gltf");

    BENCHMARK("Parse Buggy.gltf with SIMD") {
        auto buggy = parser.loadGLTF(jsonData.get(), buggyPath, benchmarkOptions);
        return buggy->parse();
    };

    BENCHMARK("Parse Buggy.gltf without SIMD") {
        auto buggy = parser.loadGLTF(jsonData.get(), buggyPath, noSimdBenchmarkOptions);
        return buggy->parse();
    };
}
