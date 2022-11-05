#include <fstream>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fastgltf_parser.hpp"
#include "fastgltf_types.hpp"

extern std::filesystem::path path;

TEST_CASE("Benchmark loading of NewSponza", "[gltf-benchmark]") {
    auto intel = path / "intel_sponza" / "NewSponza_Main_glTF_002.gltf";

    fastgltf::Parser parser;

    auto data = std::make_unique<fastgltf::GltfDataBuffer>();
    data->loadFromFile(intel);

    BENCHMARK("Parse NewSponza with SIMD") {
        auto sponza = parser.loadGLTF(data.get(), intel.parent_path(), fastgltf::Options::None);
        return sponza->parse();
    };

    BENCHMARK("Parse NewSponza without SIMD") {
        auto sponza = parser.loadGLTF(data.get(), intel.parent_path(), fastgltf::Options::DontUseSIMD);
        return sponza->parse();
    };
}

TEST_CASE("Benchmark base64 decoding from glTF file", "[base64-benchmark]") {
    fastgltf::Parser parser;
    fastgltf::Image texture;
    std::string bufferData;

    auto cylinderEngine = path / "sample-models" / "2.0" / "2CylinderEngine" / "glTF-Embedded" / "2CylinderEngine.gltf";
    auto parent = cylinderEngine.parent_path().string();

    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    jsonData->loadFromFile(cylinderEngine);

    BENCHMARK("Parse 2CylinderEngine and decode base64 with SIMD") {
        auto engine = parser.loadGLTF(jsonData.get(), cylinderEngine.parent_path(), fastgltf::Options::None);
        return engine->parse();
    };

    BENCHMARK("Parse 2CylinderEngine and decode base64 without SIMD") {
        auto engine = parser.loadGLTF(jsonData.get(), cylinderEngine.parent_path(), fastgltf::Options::DontUseSIMD);
        return engine->parse();
    };
};

TEST_CASE("Benchmark raw JSON parsing", "[gltf-benchmark]") {
    auto cylinderEngine = path / "sample-models" / "2.0" / "Buggy" / "glTF" / "Buggy.gltf";

    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    jsonData->loadFromFile(cylinderEngine);

    fastgltf::Parser parser;
    fastgltf::Image texture;
    std::string bufferData;

    BENCHMARK("Parse Buggy.gltf with SIMD") {
        auto buggy = parser.loadGLTF(jsonData.get(), cylinderEngine.parent_path(), fastgltf::Options::None);
        return buggy->parse();
    };

    BENCHMARK("Parse Buggy.gltf without SIMD") {
        auto buggy = parser.loadGLTF(jsonData.get(), cylinderEngine.parent_path(), fastgltf::Options::DontUseSIMD);
        return buggy->parse();
    };
}
