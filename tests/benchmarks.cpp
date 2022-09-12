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

    BENCHMARK("Large buffer decode with AVX") {
        return parser.loadGLTF(cylinderEngine, fastgltf::Options::None);
    };

    BENCHMARK("Large buffer decode without SIMD") {
        return parser.loadGLTF(cylinderEngine, fastgltf::Options::DontUseSIMD);
    };
};

