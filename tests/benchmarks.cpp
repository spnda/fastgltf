#include <fstream>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fastgltf_parser.hpp"
#include "fastgltf_types.hpp"
#include "gltf_path.hpp"

TEST_CASE("Benchmark loading of NewSponza", "[gltf-benchmark]") {
    fastgltf::Parser parser;
    auto data = std::make_unique<fastgltf::GltfDataBuffer>();
    data->loadFromFile(intelSponza / "NewSponza_Main_glTF_002.gltf");

    BENCHMARK("Parse NewSponza with SIMD") {
        auto sponza = parser.loadGLTF(data.get(), intelSponza, fastgltf::Options::None);
        return sponza->parse();
    };

    BENCHMARK("Parse NewSponza without SIMD") {
        auto sponza = parser.loadGLTF(data.get(), intelSponza, fastgltf::Options::DontUseSIMD);
        return sponza->parse();
    };
}

TEST_CASE("Benchmark base64 decoding from glTF file", "[base64-benchmark]") {
    fastgltf::Parser parser;
    auto cylinderEngine = sampleModels / "2.0" / "2CylinderEngine" / "glTF-Embedded";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    jsonData->loadFromFile(cylinderEngine / "2CylinderEngine.gltf");

    BENCHMARK("Parse 2CylinderEngine and decode base64 with SIMD") {
        auto engine = parser.loadGLTF(jsonData.get(), cylinderEngine, fastgltf::Options::None);
        return engine->parse();
    };

    BENCHMARK("Parse 2CylinderEngine and decode base64 without SIMD") {
        auto engine = parser.loadGLTF(jsonData.get(), cylinderEngine, fastgltf::Options::DontUseSIMD);
        return engine->parse();
    };
};

TEST_CASE("Benchmark raw JSON parsing", "[gltf-benchmark]") {
    fastgltf::Parser parser;
    auto buggyPath = sampleModels / "2.0" / "Buggy" / "glTF";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    jsonData->loadFromFile(buggyPath / "Buggy.gltf");

    BENCHMARK("Parse Buggy.gltf with SIMD") {
        auto buggy = parser.loadGLTF(jsonData.get(), buggyPath, fastgltf::Options::None);
        return buggy->parse();
    };

    BENCHMARK("Parse Buggy.gltf without SIMD") {
        auto buggy = parser.loadGLTF(jsonData.get(), buggyPath, fastgltf::Options::DontUseSIMD);
        return buggy->parse();
    };
}
