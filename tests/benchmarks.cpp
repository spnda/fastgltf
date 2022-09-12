#include <fstream>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fastgltf_parser.hpp"
#include "fastgltf_types.hpp"

TEST_CASE("Benchmark loading of NewSponza", "[gltf-benchmark]") {
    auto path = std::filesystem::path { __FILE__ }.parent_path() / "gltf";
    auto intel = path / "intel_sponza" / "NewSponza_Main_glTF_002.gltf";

    fastgltf::Parser parser;

    BENCHMARK("Load newsponza with SIMD") {
        return parser.loadGLTF(intel, fastgltf::Options::None);
    };

    BENCHMARK("Load newsponza without SIMD") {
        return parser.loadGLTF(intel, fastgltf::Options::DontUseSIMD);
    };
}

TEST_CASE("Benchmark AVX vs fallback base64 decoding", "[base64-benchmark]") {
    auto path = std::filesystem::path { __FILE__ }.parent_path() / "gltf";

    fastgltf::Parser parser;
    fastgltf::Image texture;
    std::string bufferData;

    auto cylinderEngine = path / "sample-models" / "2.0" / "2CylinderEngine" / "glTF-Embedded" / "2CylinderEngine.gltf";
    auto parent = cylinderEngine.parent_path().string();

    std::ifstream file(cylinderEngine, std::ios::ate | std::ios::binary);
    auto fileSize = file.tellg();

    std::vector<uint8_t> bytes(static_cast<size_t>(fileSize));
    file.seekg(0, std::ifstream::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), fileSize);

    BENCHMARK("2CylinderEngine decode with AVX") {
        return parser.loadGLTF(bytes.data(), bytes.size(), cylinderEngine.parent_path(), fastgltf::Options::None);
    };

    BENCHMARK("2CylinderEngine decode without SIMD") {
        return parser.loadGLTF(bytes.data(), bytes.size(), cylinderEngine.parent_path(), fastgltf::Options::DontUseSIMD);
    };
};

