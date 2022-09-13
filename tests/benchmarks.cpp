#include <fstream>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fastgltf_parser.hpp"
#include "fastgltf_types.hpp"

TEST_CASE("Benchmark loading of NewSponza", "[gltf-benchmark]") {
    auto path = std::filesystem::path { __FILE__ }.parent_path() / "gltf";
    auto intel = path / "intel_sponza" / "NewSponza_Main_glTF_002.gltf";

    fastgltf::Parser parser;

    BENCHMARK("Parse NewSponza with SIMD") {
        auto sponza = parser.loadGLTF(intel, fastgltf::Options::None);
        sponza->parseBuffers();
        sponza->parseBufferViews();
        sponza->parseAccessors();
        sponza->parseMeshes();
        sponza->parseNodes();
        return sponza->parseScenes();
    };

    BENCHMARK("Parse NewSponza without SIMD") {
        auto sponza = parser.loadGLTF(intel, fastgltf::Options::DontUseSIMD);
        sponza->parseBuffers();
        sponza->parseBufferViews();
        sponza->parseAccessors();
        sponza->parseMeshes();
        sponza->parseNodes();
        return sponza->parseScenes();
    };
}

TEST_CASE("Benchmark base64 decoding from glTF file", "[base64-benchmark]") {
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

    BENCHMARK("Parse 2CylinderEngine and decode base64 with SIMD") {
        auto engine = parser.loadGLTF(bytes.data(), bytes.size(), cylinderEngine.parent_path(), fastgltf::Options::None);
        return engine->parseBuffers();
    };

    BENCHMARK("Parse 2CylinderEngine and decode base64 without SIMD") {
        auto engine = parser.loadGLTF(bytes.data(), bytes.size(), cylinderEngine.parent_path(), fastgltf::Options::DontUseSIMD);
        return engine->parseBuffers();
    };
};

TEST_CASE("Benchmark raw JSON parsing", "[gltf-benchmark]") {
    auto path = std::filesystem::path { __FILE__ }.parent_path() / "gltf";

    auto cylinderEngine = path / "sample-models" / "2.0" / "Buggy" / "glTF" / "Buggy.gltf";

    std::ifstream file(cylinderEngine, std::ios::ate | std::ios::binary);
    auto fileSize = file.tellg();

    std::vector<uint8_t> bytes(static_cast<size_t>(fileSize));
    file.seekg(0, std::ifstream::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), fileSize);

    fastgltf::Parser parser;
    fastgltf::Image texture;
    std::string bufferData;

    BENCHMARK("Parse Buggy.gltf with SIMD") {
        auto buggy = parser.loadGLTF(bytes.data(), bytes.size(), cylinderEngine.parent_path(), fastgltf::Options::None);
        buggy->parseBuffers();
        buggy->parseBufferViews();
        buggy->parseAccessors();
        buggy->parseMeshes();
        buggy->parseNodes();
        return buggy->parseScenes();
    };

    BENCHMARK("Parse Buggy.gltf without SIMD") {
        auto buggy = parser.loadGLTF(bytes.data(), bytes.size(), cylinderEngine.parent_path(), fastgltf::Options::DontUseSIMD);
        buggy->parseBuffers();
        buggy->parseBufferViews();
        buggy->parseAccessors();
        buggy->parseMeshes();
        buggy->parseNodes();
        return buggy->parseScenes();
    };
}
