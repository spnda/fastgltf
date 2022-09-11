#include <iostream>

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "fastgltf_parser.hpp"
#include "fastgltf_types.hpp"

TEST_CASE("Component type tests") {
    using namespace fastgltf;

    // clang-format off
    REQUIRE(fastgltf::getNumComponents(AccessorType::Scalar) ==  1);
    REQUIRE(fastgltf::getNumComponents(AccessorType::Vec2)   ==  2);
    REQUIRE(fastgltf::getNumComponents(AccessorType::Vec3)   ==  3);
    REQUIRE(fastgltf::getNumComponents(AccessorType::Vec4)   ==  4);
    REQUIRE(fastgltf::getNumComponents(AccessorType::Mat2)   ==  4);
    REQUIRE(fastgltf::getNumComponents(AccessorType::Mat3)   ==  9);
    REQUIRE(fastgltf::getNumComponents(AccessorType::Mat4)   == 16);

    REQUIRE(fastgltf::getComponentBitSize(ComponentType::Byte)          ==  8);
    REQUIRE(fastgltf::getComponentBitSize(ComponentType::UnsignedByte)  ==  8);
    REQUIRE(fastgltf::getComponentBitSize(ComponentType::Short)         == 16);
    REQUIRE(fastgltf::getComponentBitSize(ComponentType::UnsignedShort) == 16);
    REQUIRE(fastgltf::getComponentBitSize(ComponentType::UnsignedInt)   == 32);
    REQUIRE(fastgltf::getComponentBitSize(ComponentType::Float)         == 32);
    REQUIRE(fastgltf::getComponentBitSize(ComponentType::Double)        == 64);
    REQUIRE(fastgltf::getComponentBitSize(ComponentType::Invalid)       ==  0);

    REQUIRE(fastgltf::getComponentType(5120) == ComponentType::Byte);
    REQUIRE(fastgltf::getComponentType(5121) == ComponentType::UnsignedByte);
    REQUIRE(fastgltf::getComponentType(5122) == ComponentType::Short);
    REQUIRE(fastgltf::getComponentType(5123) == ComponentType::UnsignedShort);
    REQUIRE(fastgltf::getComponentType(5125) == ComponentType::UnsignedInt);
    REQUIRE(fastgltf::getComponentType(5126) == ComponentType::Float);
    REQUIRE(fastgltf::getComponentType(5130) == ComponentType::Double);
    REQUIRE(fastgltf::getComponentType(5131) == ComponentType::Invalid);
    // clang-format on
}

TEST_CASE("Loading some basic glTF") {
    // We need to use the __FILE__ macro so that we have access to test glTF files in this
    // directory. As Clang does not yet fully support std::source_location, we cannot use that.
    auto path = std::filesystem::path { __FILE__ }.parent_path() / "gltf";

    fastgltf::Parser parser;
    SECTION("Loading basic invalid glTF files") {
        REQUIRE(!parser.loadGlTF(path / "empty_json.gltf"));
        REQUIRE(parser.getError() == fastgltf::Error::InvalidOrMissingAssetField);
    }

    SECTION("Loading basic glTF files") {
        // basic_gltf is a glTF file including only the absolute minimum a glTF file needs.
        REQUIRE(parser.loadGlTF(path / "basic_gltf.gltf"));
        REQUIRE(parser.getError() == fastgltf::Error::None);

        REQUIRE(parser.loadGlTF(path / "cube" / "Cube.gltf"));
        REQUIRE(parser.getError() == fastgltf::Error::None);

        auto cube = parser.getParsedAsset();
        REQUIRE(cube->scenes.size() == 1);
        REQUIRE(cube->scenes.front().nodeIndices.size() == 1);
        REQUIRE(cube->scenes.front().nodeIndices.front() == 0);

        REQUIRE(cube->nodes.size() == 1);
        REQUIRE(cube->nodes.front().name == "Cube");
        REQUIRE(!cube->nodes.front().hasMatrix);

        REQUIRE(cube->accessors.size() == 5);
        REQUIRE(cube->accessors[0].type == fastgltf::AccessorType::Scalar);
        REQUIRE(cube->accessors[0].componentType == fastgltf::ComponentType::UnsignedShort);
        REQUIRE(cube->accessors[1].type == fastgltf::AccessorType::Vec3);
        REQUIRE(cube->accessors[1].componentType == fastgltf::ComponentType::Float);

        REQUIRE(cube->bufferViews.size() == 5);
        REQUIRE(cube->buffers.size() == 1);
    }

    auto intel = path / "intel_sponza" / "NewSponza_Main_glTF_002.gltf";

    BENCHMARK("Load newsponza with SIMD") {
        return parser.loadGlTF(intel, fastgltf::Options::None);
    };

    BENCHMARK("Load newsponza without SIMD") {
        return parser.loadGlTF(intel, fastgltf::Options::DontUseSIMD);
    };
}
