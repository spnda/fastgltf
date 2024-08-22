#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <fastgltf/core.hpp>
#include "gltf_path.hpp"

// Tests for extension functionality, declared in the same order as the fastgltf::Extensions enum.
// Some tests might need expanding, by being tested on multiple assets.

TEST_CASE("Extension KHR_texture_transform", "[gltf-loader]") {
	auto transformTest = sampleModels / "2.0" / "TextureTransformMultiTest" / "glTF";
	fastgltf::GltfFileStream jsonData(transformTest / "TextureTransformMultiTest.gltf");
	REQUIRE(jsonData.isOpen());

	fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_transform);
	auto asset = parser.loadGltfJson(jsonData, transformTest, fastgltf::Options::DontRequireValidAssetMember, fastgltf::Category::Materials);
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	REQUIRE(!asset->materials.empty());

	auto& material = asset->materials.front();
	REQUIRE(material.pbrData.baseColorTexture.has_value());
	REQUIRE(material.pbrData.baseColorTexture->transform != nullptr);
	REQUIRE(material.pbrData.baseColorTexture->transform->uvOffset[0] == 0.705f);
	REQUIRE(material.pbrData.baseColorTexture->transform->rotation == Catch::Approx(1.5707963705062866f));
}

TEST_CASE("Extension KHR_texture_basisu", "[gltf-loader]") {
    auto stainedLamp = sampleModels / "2.0" / "StainedGlassLamp" / "glTF-KTX-BasisU";
	fastgltf::GltfFileStream jsonData(stainedLamp / "StainedGlassLamp.gltf");
	REQUIRE(jsonData.isOpen());

    SECTION("Loading KHR_texture_basisu") {
        fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);
        auto asset = parser.loadGltfJson(jsonData, path, fastgltf::Options::DontRequireValidAssetMember,
									 fastgltf::Category::Textures | fastgltf::Category::Images);
        REQUIRE(asset.error() == fastgltf::Error::None);
		REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

        REQUIRE(asset->textures.size() == 19);
        REQUIRE(!asset->images.empty());

        auto& texture = asset->textures[1];
        REQUIRE(!texture.imageIndex.has_value());
        REQUIRE(texture.samplerIndex == 0U);
		REQUIRE(texture.basisuImageIndex.has_value());
		REQUIRE(texture.basisuImageIndex.value() == 1);

        auto& image = asset->images.front();
        auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data);
        REQUIRE(filePath != nullptr);
        REQUIRE(filePath->uri.valid());
        REQUIRE(filePath->uri.isLocalPath());
        REQUIRE(filePath->mimeType == fastgltf::MimeType::KTX2);
    }

    SECTION("Testing requiredExtensions") {
        // We specify no extensions, yet the StainedGlassLamp requires KHR_texture_basisu.
        fastgltf::Parser parser(fastgltf::Extensions::None);
        auto stainedGlassLamp = parser.loadGltfJson(jsonData, path, fastgltf::Options::DontRequireValidAssetMember);
        REQUIRE(stainedGlassLamp.error() == fastgltf::Error::MissingExtensions);
    }
}

// TODO: Add tests for MSFT_texture_dds, KHR_mesh_quantization extension

TEST_CASE("Extension EXT_meshopt_compression", "[gltf-loader]") {
	auto brainStem = sampleModels / "2.0" / "BrainStem" / "glTF-Meshopt";
	fastgltf::GltfFileStream jsonData(brainStem / "BrainStem.gltf");
	REQUIRE(jsonData.isOpen());

	fastgltf::Parser parser(fastgltf::Extensions::EXT_meshopt_compression | fastgltf::Extensions::KHR_mesh_quantization);
	auto asset = parser.loadGltfJson(jsonData, brainStem, fastgltf::Options::None);
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	for (auto i = 0; i < 8; ++i) {
		REQUIRE(bool(asset->bufferViews[i].meshoptCompression));
	}

	{
		auto& mc = *asset->bufferViews[0].meshoptCompression;
		REQUIRE(mc.bufferIndex == 0);
		REQUIRE(mc.byteOffset == 0);
		REQUIRE(mc.byteLength == 2646);
		REQUIRE(mc.byteStride == 4);
		REQUIRE(mc.mode == fastgltf::MeshoptCompressionMode::Attributes);
		REQUIRE(mc.count == 34084);
	}
	{
		auto& mc = *asset->bufferViews[1].meshoptCompression;
		REQUIRE(mc.bufferIndex == 0);
		REQUIRE(mc.byteOffset == 2648);
		REQUIRE(mc.byteLength == 68972);
		REQUIRE(mc.byteStride == 4);
		REQUIRE(mc.mode == fastgltf::MeshoptCompressionMode::Attributes);
		REQUIRE(mc.filter == fastgltf::MeshoptCompressionFilter::Octahedral);
		REQUIRE(mc.count == 34084);
	}
}

TEST_CASE("Extension KHR_draco_mesh_compression", "[gltf-loader]") {
	auto brainStem = sampleModels / "2.0" / "BrainStem" / "glTF-Draco";
	fastgltf::GltfFileStream jsonData(brainStem / "BrainStem.gltf");
	REQUIRE(jsonData.isOpen());

	fastgltf::Parser parser(fastgltf::Extensions::KHR_draco_mesh_compression);
	auto asset = parser.loadGltfJson(jsonData, brainStem, fastgltf::Options::None);
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	REQUIRE(!asset->meshes.empty());
	REQUIRE(asset->meshes[0].primitives.size() >= 2);

	{
		auto& draco = asset->meshes[0].primitives[0].dracoCompression;
		REQUIRE(draco);
		REQUIRE(draco->bufferView == 4);

		REQUIRE(draco->attributes[0].name == "JOINTS_0");
		REQUIRE(draco->attributes[0].accessorIndex == 0);

		REQUIRE(draco->attributes[1].name == "NORMAL");
		REQUIRE(draco->attributes[1].accessorIndex == 1);

		REQUIRE(draco->attributes[2].name == "POSITION");
		REQUIRE(draco->attributes[2].accessorIndex == 2);

		REQUIRE(draco->attributes[3].name == "WEIGHTS_0");
		REQUIRE(draco->attributes[3].accessorIndex == 3);
	}
	{
		auto& draco = asset->meshes[0].primitives[1].dracoCompression;
		REQUIRE(draco);
		REQUIRE(draco->bufferView == 5);

		REQUIRE(draco->attributes[0].name == "JOINTS_0");
		REQUIRE(draco->attributes[0].accessorIndex == 0);

		REQUIRE(draco->attributes[1].name == "NORMAL");
		REQUIRE(draco->attributes[1].accessorIndex == 1);

		REQUIRE(draco->attributes[2].name == "POSITION");
		REQUIRE(draco->attributes[2].accessorIndex == 2);

		REQUIRE(draco->attributes[3].name == "WEIGHTS_0");
		REQUIRE(draco->attributes[3].accessorIndex == 3);
	}
}

TEST_CASE("Extension KHR_lights_punctual", "[gltf-loader]") {
	SECTION("Point light") {
		auto lightsLamp = sampleModels / "2.0" / "LightsPunctualLamp" / "glTF";
		fastgltf::GltfFileStream jsonData(lightsLamp / "LightsPunctualLamp.gltf");
		REQUIRE(jsonData.isOpen());

		fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual);
		auto asset = parser.loadGltfJson(jsonData, lightsLamp, fastgltf::Options::None, fastgltf::Category::Nodes);
		REQUIRE(asset.error() == fastgltf::Error::None);
		REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

		REQUIRE(asset->lights.size() == 5);
		REQUIRE(asset->nodes.size() > 4);

		auto& nodes = asset->nodes;
		REQUIRE(nodes[3].lightIndex.has_value());
		REQUIRE(nodes[3].lightIndex.value() == 0);

		auto& lights = asset->lights;
		REQUIRE(lights[0].name == "Point");
		REQUIRE(lights[0].type == fastgltf::LightType::Point);
		REQUIRE(lights[0].intensity == 15.0f);
		REQUIRE(lights[0].color[0] == Catch::Approx(1.0f));
		REQUIRE(lights[0].color[1] == Catch::Approx(0.63187497854232788f));
		REQUIRE(lights[0].color[2] == Catch::Approx(0.23909975588321689f));
	}

	SECTION("Directional light") {
		auto directionalLight = sampleModels / "2.0" / "DirectionalLight" / "glTF";
		fastgltf::GltfFileStream jsonData(directionalLight / "DirectionalLight.gltf");
		REQUIRE(jsonData.isOpen());

		fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual);
		auto asset = parser.loadGltfJson(jsonData, directionalLight, fastgltf::Options::None, fastgltf::Category::Nodes);
		REQUIRE(asset.error() == fastgltf::Error::None);
		REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

		REQUIRE(asset->lights.size() == 1);

		auto& light = asset->lights.front();
		REQUIRE(light.color == fastgltf::math::nvec3(0.9, 0.8, 0.1));
		REQUIRE(light.intensity == 1.0f);
		REQUIRE(light.type == fastgltf::LightType::Directional);
		REQUIRE(light.name == "Sun");
	}
}

// TODO: Add tests for EXT_texture_webp extension

TEST_CASE("Extension KHR_materials_specular", "[gltf-loader]") {
    auto specularTest = sampleModels / "2.0" / "SpecularTest" / "glTF";
	fastgltf::GltfFileStream jsonData(specularTest / "SpecularTest.gltf");
	REQUIRE(jsonData.isOpen());

    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_specular);
    auto asset = parser.loadGltfJson(jsonData, specularTest, fastgltf::Options::None, fastgltf::Category::Materials);
    REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

    REQUIRE(asset->materials.size() >= 12);

    auto& materials = asset->materials;
    REQUIRE(materials[1].specular != nullptr);
    REQUIRE(materials[1].specular->specularFactor == 0.0f);

    REQUIRE(materials[2].specular != nullptr);
	REQUIRE(materials[2].specular->specularFactor == Catch::Approx(0.051269f));

    REQUIRE(materials[8].specular != nullptr);
	REQUIRE(materials[8].specular->specularColorFactor[0] == Catch::Approx(0.051269f));
	REQUIRE(materials[8].specular->specularColorFactor[0] == Catch::Approx(0.051269f));
	REQUIRE(materials[8].specular->specularColorFactor[0] == Catch::Approx(0.051269f));

    REQUIRE(materials[12].specular != nullptr);
    REQUIRE(materials[12].specular->specularColorTexture.has_value());
    REQUIRE(materials[12].specular->specularColorTexture.value().textureIndex == 2);
}

TEST_CASE("Extension KHR_materials_ior and KHR_materials_iridescence", "[gltf-loader]") {
    auto specularTest = sampleModels / "2.0" / "IridescenceDielectricSpheres" / "glTF";
	fastgltf::GltfFileStream jsonData(specularTest / "IridescenceDielectricSpheres.gltf");
	REQUIRE(jsonData.isOpen());

    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_iridescence | fastgltf::Extensions::KHR_materials_ior);
    auto asset = parser.loadGltfJson(jsonData, specularTest, fastgltf::Options::None, fastgltf::Category::Materials);
    REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

    REQUIRE(asset->materials.size() >= 50);

    auto& materials = asset->materials;
    REQUIRE(materials[0].iridescence != nullptr);
    REQUIRE(materials[0].iridescence->iridescenceFactor == 1.0f);
    REQUIRE(materials[0].iridescence->iridescenceIor == 1.0f);
    REQUIRE(materials[0].iridescence->iridescenceThicknessMaximum == 100.0f);

    REQUIRE(materials[0].ior == 1.0f);

    REQUIRE(materials[7].ior == 1.17f);

    REQUIRE(materials[50].iridescence != nullptr);
    REQUIRE(materials[50].iridescence->iridescenceFactor == 1.0f);
    REQUIRE(materials[50].iridescence->iridescenceIor == 1.17f);
    REQUIRE(materials[50].iridescence->iridescenceThicknessMaximum == 200.0f);
}

TEST_CASE("Extension KHR_materials_volume and KHR_materials_transmission", "[gltf-loader]") {
    auto beautifulGame = sampleModels / "2.0" / "ABeautifulGame" / "glTF";

	fastgltf::GltfFileStream jsonData(beautifulGame / "ABeautifulGame.gltf");
	REQUIRE(jsonData.isOpen());

    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_volume | fastgltf::Extensions::KHR_materials_transmission);
    auto asset = parser.loadGltfJson(jsonData, beautifulGame, fastgltf::Options::None, fastgltf::Category::Materials);
    REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

    REQUIRE(asset->materials.size() >= 5);

    auto& materials = asset->materials;
    REQUIRE(materials[5].volume != nullptr);
	REQUIRE(materials[5].volume->thicknessFactor == Catch::Approx(0.2199999988079071f));
	REQUIRE(materials[5].volume->attenuationColor[0] == Catch::Approx(0.800000011920929f));
	REQUIRE(materials[5].volume->attenuationColor[1] == Catch::Approx(0.800000011920929f));
	REQUIRE(materials[5].volume->attenuationColor[2] == Catch::Approx(0.800000011920929f));

    REQUIRE(materials[5].transmission != nullptr);
    REQUIRE(materials[5].transmission->transmissionFactor == 1.0f);
}

TEST_CASE("Extension KHR_materials_clearcoat", "[gltf-loader]") {
    auto clearcoatTest = sampleModels / "2.0" / "ClearCoatTest" / "glTF";
	fastgltf::GltfFileStream jsonData(clearcoatTest / "ClearCoatTest.gltf");
	REQUIRE(jsonData.isOpen());

    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_clearcoat);
    auto asset = parser.loadGltfJson(jsonData, clearcoatTest, fastgltf::Options::None, fastgltf::Category::Materials);
    REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

    REQUIRE(asset->materials.size() >= 7);

    auto& materials = asset->materials;
    REQUIRE(materials[1].clearcoat != nullptr);
    REQUIRE(materials[1].clearcoat->clearcoatFactor == 1.0f);
    REQUIRE(materials[1].clearcoat->clearcoatRoughnessFactor == 0.03f);

    REQUIRE(materials[7].clearcoat != nullptr);
    REQUIRE(materials[7].clearcoat->clearcoatFactor == 1.0f);
    REQUIRE(materials[7].clearcoat->clearcoatRoughnessFactor == 1.0f);
    REQUIRE(materials[7].clearcoat->clearcoatRoughnessTexture.has_value());
    REQUIRE(materials[7].clearcoat->clearcoatRoughnessTexture->textureIndex == 2);
    REQUIRE(materials[7].clearcoat->clearcoatRoughnessTexture->texCoordIndex == 0);
}

TEST_CASE("Extension KHR_materials_emissive_strength", "[gltf-loader]") {
	auto emissiveStrengthTest = sampleModels / "2.0" / "EmissiveStrengthTest" / "glTF";
	fastgltf::GltfFileStream jsonData(emissiveStrengthTest / "EmissiveStrengthTest.gltf");
	REQUIRE(jsonData.isOpen());

	fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_emissive_strength);
	auto asset = parser.loadGltfJson(jsonData, emissiveStrengthTest);
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	REQUIRE(asset->materials.size() >= 3);

	REQUIRE(asset->materials[0].emissiveStrength == 4.0f);
	REQUIRE(asset->materials[2].emissiveStrength == 2.0f);
}

TEST_CASE("Extension KHR_materials_sheen", "[gltf-loader]") {
	auto sheenChair = sampleModels / "2.0" / "SheenChair" / "glTF";
	fastgltf::GltfFileStream jsonData(sheenChair / "SheenChair.gltf");
	REQUIRE(jsonData.isOpen());

	fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_sheen | fastgltf::Extensions::KHR_texture_transform);
	auto asset = parser.loadGltfJson(jsonData, sheenChair);
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	REQUIRE(asset->materials.size() >= 5);

	auto& mat0 = asset->materials[0];
	REQUIRE(mat0.sheen);
	REQUIRE(mat0.sheen->sheenColorFactor == fastgltf::math::nvec3(1.0, 0.329, 0.1));
	REQUIRE(mat0.sheen->sheenRoughnessFactor == 0.8f);
}

TEST_CASE("Extension KHR_materials_unlit", "[gltf-loader]") {
	auto unlitTest = sampleModels / "2.0" / "UnlitTest" / "glTF";
	fastgltf::GltfFileStream jsonData(unlitTest / "UnlitTest.gltf");
	REQUIRE(jsonData.isOpen());

	fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_unlit);
	auto asset = parser.loadGltfJson(jsonData, unlitTest);
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	REQUIRE(asset->materials.size() == 2);
	REQUIRE(asset->materials[0].unlit);
	REQUIRE(asset->materials[1].unlit);
}

TEST_CASE("Extension KHR_materials_anisotropy", "[gltf-loader]") {
	auto carbonFibre = sampleModels / "2.0" / "CarbonFibre" / "glTF";
	fastgltf::GltfFileStream jsonData(carbonFibre / "CarbonFibre.gltf");
	REQUIRE(jsonData.isOpen());

	fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_anisotropy);
	auto asset = parser.loadGltfJson(jsonData, carbonFibre);
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	REQUIRE(asset->materials.size() == 1);
	auto& anisotropy = asset->materials[0].anisotropy;
	REQUIRE(anisotropy);
	REQUIRE(anisotropy->anisotropyStrength == 0.5f);
	REQUIRE(anisotropy->anisotropyRotation == 0.0f);
	REQUIRE(anisotropy->anisotropyTexture.has_value());
	REQUIRE(anisotropy->anisotropyTexture->textureIndex == 2);
}

TEST_CASE("Extension EXT_mesh_gpu_instancing", "[gltf-loader]") {
    auto simpleInstancingTest = sampleModels / "2.0" / "SimpleInstancing" / "glTF";

	fastgltf::GltfFileStream jsonData(simpleInstancingTest / "SimpleInstancing.gltf");
	REQUIRE(jsonData.isOpen());

    fastgltf::Parser parser(fastgltf::Extensions::EXT_mesh_gpu_instancing);
    auto asset = parser.loadGltfJson(jsonData, simpleInstancingTest, fastgltf::Options::None, fastgltf::Category::Accessors | fastgltf::Category::Nodes);
    REQUIRE(asset.error() == fastgltf::Error::None);
    REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

    REQUIRE(asset->accessors.size() >= 6);
    REQUIRE(asset->nodes.size() >= 1);

    auto& nodes = asset->nodes;
    REQUIRE(nodes[0].instancingAttributes.size() == 3u);
    REQUIRE(nodes[0].findInstancingAttribute("TRANSLATION") != nodes[0].instancingAttributes.cend());
    REQUIRE(nodes[0].findInstancingAttribute("SCALE") != nodes[0].instancingAttributes.cend());
    REQUIRE(nodes[0].findInstancingAttribute("ROTATION") != nodes[0].instancingAttributes.cend());
}

#if FASTGLTF_ENABLE_DEPRECATED_EXT
TEST_CASE("Extension KHR_materials_pbrSpecularGlossiness", "[gltf-loader]") {
    auto specularGlossinessTest = sampleModels / "2.0" / "SpecGlossVsMetalRough" / "glTF";
    fastgltf::GltfFileStream jsonData(specularGlossinessTest / "SpecGlossVsMetalRough.gltf");
    REQUIRE(jsonData.isOpen());

    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_pbrSpecularGlossiness | fastgltf::Extensions::KHR_materials_specular);
    auto asset = parser.loadGltfJson(jsonData, specularGlossinessTest);
    REQUIRE(asset.error() == fastgltf::Error::None);
    REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

    REQUIRE(asset->materials.size() == 4);

    auto& materials = asset->materials;
    REQUIRE(materials[0].specularGlossiness != nullptr);
	REQUIRE(materials[0].specularGlossiness->diffuseFactor == fastgltf::math::nvec4(1));
	REQUIRE(materials[0].specularGlossiness->specularFactor == fastgltf::math::nvec3(1));
    REQUIRE(materials[0].specularGlossiness->glossinessFactor == 1.0f);
    REQUIRE(materials[0].specularGlossiness->diffuseTexture.has_value());
    REQUIRE(materials[0].specularGlossiness->diffuseTexture.value().textureIndex == 5);
    REQUIRE(materials[0].specularGlossiness->specularGlossinessTexture.has_value());
    REQUIRE(materials[0].specularGlossiness->specularGlossinessTexture.value().textureIndex == 6);

    REQUIRE(materials[3].specularGlossiness != nullptr);
	REQUIRE(materials[3].specularGlossiness->diffuseFactor == fastgltf::math::nvec4(1));
	REQUIRE(materials[3].specularGlossiness->specularFactor == fastgltf::math::nvec3(0));
    REQUIRE(materials[3].specularGlossiness->glossinessFactor == 0.0f);
    REQUIRE(materials[3].specularGlossiness->diffuseTexture.has_value());
    REQUIRE(materials[3].specularGlossiness->diffuseTexture.value().textureIndex == 7);
}
#endif

// TODO: Add tests for MSFT_packing_* extensions

TEST_CASE("Extension KHR_materials_dispersion", "[gltf-loader]") {
	constexpr std::string_view json = R"({"materials": [
        {
            "extensions": {
                "KHR_materials_dispersion": {
                    "dispersion": 0.1
                }
            }
        }
    ]})";
	auto jsonData = fastgltf::GltfDataBuffer::FromBytes(
			reinterpret_cast<const std::byte*>(json.data()), json.size());
	REQUIRE(jsonData.error() == fastgltf::Error::None);

	fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_dispersion);
	auto asset = parser.loadGltfJson(jsonData.get(), {}, fastgltf::Options::DontRequireValidAssetMember);
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	REQUIRE(asset->materials.size() == 1);
	REQUIRE(asset->materials.front().dispersion == 0.1f);
}

TEST_CASE("Extension KHR_materials_variant", "[gltf-loader]") {
	auto velvetSofa = sampleModels / "2.0" / "GlamVelvetSofa" / "glTF";

	fastgltf::GltfFileStream jsonData(velvetSofa / "GlamVelvetSofa.gltf");
	REQUIRE(jsonData.isOpen());

	fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_variants | fastgltf::Extensions::KHR_texture_transform);
	auto asset = parser.loadGltfJson(jsonData, velvetSofa, fastgltf::Options::None);
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	REQUIRE(asset->materialVariants.size() == 5);
	REQUIRE(asset->materialVariants[0] == "Champagne");
	REQUIRE(asset->materialVariants[1] == "Navy");
	REQUIRE(asset->materialVariants[2] == "Gray");
	REQUIRE(asset->materialVariants[3] == "Black");
	REQUIRE(asset->materialVariants[4] == "Pale Pink");

	REQUIRE(asset->meshes.size() >= 2);
	REQUIRE(asset->meshes[1].primitives.size() == 1);

	auto& primitive = asset->meshes[1].primitives[0];
	REQUIRE(primitive.mappings.size() == 5);
	REQUIRE(primitive.mappings[0] == 2U);
	REQUIRE(primitive.mappings[1] == 3U);
	REQUIRE(primitive.mappings[2] == 4U);
	REQUIRE(primitive.mappings[3] == 5U);
	REQUIRE(primitive.mappings[4] == 6U);
}
