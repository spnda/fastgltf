#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <fastgltf/core.hpp>
#include "gltf_path.hpp"

// Tests for extension functionality, declared in the same order as the fastgltf::Extensions enum.
// Some tests might need expanding, by being tested on multiple assets.

TEST_CASE("Extension KHR_texture_transform", "[gltf-loader]") {
	auto transformTest = sampleAssets / "Models" / "TextureTransformMultiTest" / "glTF";
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
    auto stainedLamp = sampleAssets / "Models" / "StainedGlassLamp" / "glTF-KTX-BasisU";
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
	auto brainStem = sampleAssets / "Models" / "BrainStem" / "glTF-Meshopt";
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
	auto brainStem = sampleAssets / "Models" / "BrainStem" / "glTF-Draco";
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
		auto lightsLamp = sampleAssets / "Models" / "LightsPunctualLamp" / "glTF";
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
		auto directionalLight = sampleAssets / "Models" / "DirectionalLight" / "glTF";
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
    auto specularTest = sampleAssets / "Models" / "SpecularTest" / "glTF";
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
    auto specularTest = sampleAssets / "Models" / "IridescenceDielectricSpheres" / "glTF";
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
    auto beautifulGame = sampleAssets / "Models" / "ABeautifulGame" / "glTF";

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
    auto clearcoatTest = sampleAssets / "Models" / "ClearCoatTest" / "glTF";
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
    REQUIRE(materials[7].clearcoat->clearcoatRoughnessTexture->textureIndex == 1);
    REQUIRE(materials[7].clearcoat->clearcoatRoughnessTexture->texCoordIndex == 0);
}

TEST_CASE("Extension KHR_materials_emissive_strength", "[gltf-loader]") {
	auto emissiveStrengthTest = sampleAssets / "Models" / "EmissiveStrengthTest" / "glTF";
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
	auto sheenChair = sampleAssets / "Models" / "SheenChair" / "glTF";
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
	auto unlitTest = sampleAssets / "Models" / "UnlitTest" / "glTF";
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
	auto carbonFibre = sampleAssets / "Models" / "CarbonFibre" / "glTF";
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
    auto simpleInstancingTest = sampleAssets / "Models" / "SimpleInstancing" / "glTF";

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
    auto specularGlossinessTest = sampleAssets / "Models" / "SpecGlossVsMetalRough" / "glTF";
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
	auto velvetSofa = sampleAssets / "Models" / "GlamVelvetSofa" / "glTF";

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

#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
TEST_CASE("Extension KHR_implicit_shapes", "[gltf-loader]") {
	auto shapeTypes = physicsSampleAssets / "samples" / "ShapeTypes" / "ShapeTypes.gltf";

	fastgltf::GltfFileStream jsonData(shapeTypes);
	REQUIRE(jsonData.isOpen());

	fastgltf::Parser parser(fastgltf::Extensions::KHR_implicit_shapes | fastgltf::Extensions::KHR_lights_punctual);
	auto asset = parser.loadGltfJson(jsonData, shapeTypes.parent_path());
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	REQUIRE(asset->shapes.size() == 7);

	const auto& box0 = asset->shapes.at(0);
	REQUIRE(box0.type == fastgltf::ShapeType::Box);
	fastgltf::visit_exhaustive(fastgltf::visitor{
	    [](const fastgltf::BoxShape& box) {
			REQUIRE(box.size == fastgltf::math::fvec3(0.5285500288009644, 1, 0.5285500288009644));
	    },
		[](const fastgltf::SphereShape& sphere) {
			REQUIRE(false);
		},
		[](const fastgltf::CapsuleShape& capsule) {
			REQUIRE(false);
		},
		[](const fastgltf::CylinderShape& cylinder) {
			REQUIRE(false);
		}
		},
		box0.shape);

	const auto& sphere4 = asset->shapes.at(4);
	REQUIRE(sphere4.type == fastgltf::ShapeType::Sphere);
	fastgltf::visit_exhaustive(fastgltf::visitor{
		[](const fastgltf::BoxShape& box) {
			REQUIRE(false);
		},
		[](const fastgltf::SphereShape& sphere) {
			REQUIRE(abs(sphere.radius - 0.3417187615099374) < FLT_EPSILON);
		},
		[](const fastgltf::CapsuleShape& capsule) {
			REQUIRE(false);
		},
		[](const fastgltf::CylinderShape& cylinder) {
			REQUIRE(false);
		}
		},
		sphere4.shape);

	const auto& capsule6 = asset->shapes.at(6);
	REQUIRE(capsule6.type == fastgltf::ShapeType::Capsule);
	fastgltf::visit_exhaustive(fastgltf::visitor{
		[](const fastgltf::BoxShape& box) {
			REQUIRE(false);
		},
		[](const fastgltf::SphereShape& sphere) {
			REQUIRE(false);
		},
		[](const fastgltf::CapsuleShape& capsule) {
			REQUIRE(abs(capsule.height - 0.6000000238418579) < FLT_EPSILON);
			REQUIRE(abs(capsule.radiusBottom - 0.25) < FLT_EPSILON);
			REQUIRE(abs(capsule.radiusTop - 0.4000000059604645) < FLT_EPSILON);
		},
		[](const fastgltf::CylinderShape& cylinder) {
			REQUIRE(false);
		}
		},
		capsule6.shape);

	const auto& cylinder2 = asset->shapes.at(2);
	REQUIRE(cylinder2.type == fastgltf::ShapeType::Cylinder);
	fastgltf::visit_exhaustive(fastgltf::visitor{
		[](const fastgltf::BoxShape& box) {
			REQUIRE(false);
		},
		[](const fastgltf::SphereShape& sphere) {
			REQUIRE(false);
		},
		[](const fastgltf::CapsuleShape& capsule) {
			REQUIRE(false);
		},
		[](const fastgltf::CylinderShape& cylinder) {
			REQUIRE(abs(cylinder.height - 0.06662015616893768) < FLT_EPSILON);
			REQUIRE(abs(cylinder.radiusBottom - 0.15483441948890686) < FLT_EPSILON);
			REQUIRE(abs(cylinder.radiusTop - 0.15483441948890686) < FLT_EPSILON);
		}
		},
		cylinder2.shape);
}
#endif

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
TEST_CASE("Extension KHR_physics_rigid_bodies", "[gltf-loader]") {
	auto shapeTypes = physicsSampleAssets / "samples" / "ShapeTypes" / "ShapeTypes.gltf";

	fastgltf::GltfFileStream jsonData(shapeTypes);
	REQUIRE(jsonData.isOpen());

	fastgltf::Parser parser(fastgltf::Extensions::KHR_physics_rigid_bodies | fastgltf::Extensions::KHR_lights_punctual);
	auto asset = parser.loadGltfJson(jsonData, shapeTypes.parent_path());
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	REQUIRE(asset->physicsMaterials.size() == 1);
	const auto& material = asset->physicsMaterials.at(0);
	REQUIRE(material.staticFriction == 0.5);
	REQUIRE(material.dynamicFriction == 0.5);
	REQUIRE(material.restitution == 0);

	REQUIRE(asset->collisionFilters.size() == 1);
	const auto& filter = asset->collisionFilters.at(0);
	REQUIRE(filter.collisionSystems.size() == 1);
	REQUIRE(filter.collisionSystems.at(0) == "System_0");
	REQUIRE(filter.collideWithSystems.size() == 1);
	REQUIRE(filter.collideWithSystems.at(0) == "System_0");
	REQUIRE(filter.notCollideWithSystems.size() == 0);

	REQUIRE(asset->physicsJoints.size() == 0);

	const auto& node = asset->nodes.at(0);

	REQUIRE(node.physicsRigidBody.has_value());
	if (node.physicsRigidBody) {
		REQUIRE(node.physicsRigidBody->motion.has_value());
		if (node.physicsRigidBody->motion) {
			REQUIRE(node.physicsRigidBody->motion->mass == 1);
			REQUIRE(!node.physicsRigidBody->motion->inertialDiagonal.has_value());
			REQUIRE(!node.physicsRigidBody->motion->inertialOrientation.has_value());
		}

		REQUIRE(node.physicsRigidBody->collider.has_value());
		if (node.physicsRigidBody->collider) {
			REQUIRE(node.physicsRigidBody->collider->geometry.shape.has_value());
			REQUIRE(node.physicsRigidBody->collider->geometry.shape == 0);
			REQUIRE(!node.physicsRigidBody->collider->geometry.node.has_value());
			REQUIRE(node.physicsRigidBody->collider->physicsMaterial == 0);
			REQUIRE(node.physicsRigidBody->collider->collisionFilter == 0);
		}

		REQUIRE(!node.physicsRigidBody->trigger.has_value());
		REQUIRE(!node.physicsRigidBody->joint.has_value());
	}

	const auto& node10 = asset->nodes.at(11);

	REQUIRE(node10.physicsRigidBody.has_value());
	if (node10.physicsRigidBody) {
		REQUIRE(!node10.physicsRigidBody->motion.has_value());
		REQUIRE(!node10.physicsRigidBody->collider.has_value());

	    REQUIRE(node10.physicsRigidBody->trigger.has_value());
		if(node10.physicsRigidBody->trigger) {
			fastgltf::visit_exhaustive(fastgltf::visitor{
				[](const fastgltf::GeometryTrigger& geo) {
					REQUIRE(geo.geometry.convexHull);
					REQUIRE(geo.geometry.node == 10);
					REQUIRE(geo.collisionFilter.has_value());
					REQUIRE(geo.collisionFilter == 0);
				},
				[](const fastgltf::NodeTrigger& node) {
					REQUIRE(false);
				}
			},
			*node10.physicsRigidBody->trigger);
		}

	    REQUIRE(!node10.physicsRigidBody->joint.has_value());
	}
}
#endif
