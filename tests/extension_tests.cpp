#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fastgltf/core.hpp>
#include "gltf_path.hpp"

TEST_CASE("Loading KHR_texture_basisu glTF files", "[gltf-loader]") {
    auto stainedLamp = sampleModels / "2.0" / "StainedGlassLamp" / "glTF-KTX-BasisU";

    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(stainedLamp / "StainedGlassLamp.gltf"));

    SECTION("Loading KHR_texture_basisu") {
        fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);
        auto asset = parser.loadGltfJson(jsonData.get(), path, fastgltf::Options::DontRequireValidAssetMember,
									 fastgltf::Category::Textures | fastgltf::Category::Images);
        REQUIRE(asset.error() == fastgltf::Error::None);
		REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

        REQUIRE(asset->textures.size() == 19);
        REQUIRE(!asset->images.empty());

        auto& texture = asset->textures[1];
        REQUIRE(!texture.imageIndex.has_value());
        REQUIRE(texture.samplerIndex == 0);
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
        auto stainedGlassLamp = parser.loadGltfJson(jsonData.get(), path, fastgltf::Options::DontRequireValidAssetMember);
        REQUIRE(stainedGlassLamp.error() == fastgltf::Error::MissingExtensions);
    }
}

TEST_CASE("Loading KHR_texture_transform glTF files", "[gltf-loader]") {
    auto transformTest = sampleModels / "2.0" / "TextureTransformMultiTest" / "glTF";

    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(transformTest / "TextureTransformMultiTest.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_transform);
    auto asset = parser.loadGltfJson(jsonData.get(), transformTest, fastgltf::Options::DontRequireValidAssetMember, fastgltf::Category::Materials);
    REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

    REQUIRE(!asset->materials.empty());

    auto& material = asset->materials.front();
    REQUIRE(material.pbrData.baseColorTexture.has_value());
    REQUIRE(material.pbrData.baseColorTexture->transform != nullptr);
    REQUIRE(material.pbrData.baseColorTexture->transform->uvOffset[0] == 0.705f);
    REQUIRE(material.pbrData.baseColorTexture->transform->rotation == Catch::Approx(1.5707963705062866f));
}

TEST_CASE("Test KHR_lights_punctual", "[gltf-loader]") {
    auto lightsLamp = sampleModels / "2.0" / "LightsPunctualLamp" / "glTF";
    fastgltf::GltfDataBuffer jsonData;
    REQUIRE(jsonData.loadFromFile(lightsLamp / "LightsPunctualLamp.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual);
    auto asset = parser.loadGltfJson(&jsonData, lightsLamp, fastgltf::Options::None, fastgltf::Category::Nodes);
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
    REQUIRE(glm::epsilonEqual(lights[0].color[0], 1.0f, glm::epsilon<float>()));
    REQUIRE(glm::epsilonEqual(lights[0].color[1], 0.63187497854232788f, glm::epsilon<float>()));
    REQUIRE(glm::epsilonEqual(lights[0].color[2], 0.23909975588321689f, glm::epsilon<float>()));
}

TEST_CASE("Test KHR_materials_specular", "[gltf-loader]") {
    auto specularTest = sampleModels / "2.0" / "SpecularTest" / "glTF";
    fastgltf::GltfDataBuffer jsonData;
    REQUIRE(jsonData.loadFromFile(specularTest / "SpecularTest.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_specular);
    auto asset = parser.loadGltfJson(&jsonData, specularTest, fastgltf::Options::None, fastgltf::Category::Materials);
    REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

    REQUIRE(asset->materials.size() >= 12);

    auto& materials = asset->materials;
    REQUIRE(materials[1].specular != nullptr);
    REQUIRE(materials[1].specular->specularFactor == 0.0f);

    REQUIRE(materials[2].specular != nullptr);
    REQUIRE(glm::epsilonEqual(materials[2].specular->specularFactor, 0.051269f, glm::epsilon<float>()));

    REQUIRE(materials[8].specular != nullptr);
    REQUIRE(glm::epsilonEqual(materials[8].specular->specularColorFactor[0], 0.051269f, glm::epsilon<float>()));
    REQUIRE(glm::epsilonEqual(materials[8].specular->specularColorFactor[1], 0.051269f, glm::epsilon<float>()));
    REQUIRE(glm::epsilonEqual(materials[8].specular->specularColorFactor[2], 0.051269f, glm::epsilon<float>()));

    REQUIRE(materials[12].specular != nullptr);
    REQUIRE(materials[12].specular->specularColorTexture.has_value());
    REQUIRE(materials[12].specular->specularColorTexture.value().textureIndex == 2);
}

TEST_CASE("Test KHR_materials_ior and KHR_materials_iridescence", "[gltf-loader]") {
    auto specularTest = sampleModels / "2.0" / "IridescenceDielectricSpheres" / "glTF";
    fastgltf::GltfDataBuffer jsonData;
    REQUIRE(jsonData.loadFromFile(specularTest / "IridescenceDielectricSpheres.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_iridescence | fastgltf::Extensions::KHR_materials_ior);
    auto asset = parser.loadGltfJson(&jsonData, specularTest, fastgltf::Options::None, fastgltf::Category::Materials);
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

TEST_CASE("Test KHR_materials_volume and KHR_materials_transmission", "[gltf-loader]") {
    auto beautifulGame = sampleModels / "2.0" / "ABeautifulGame" / "glTF";
    fastgltf::GltfDataBuffer jsonData;
    REQUIRE(jsonData.loadFromFile(beautifulGame / "ABeautifulGame.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_volume | fastgltf::Extensions::KHR_materials_transmission);
    auto asset = parser.loadGltfJson(&jsonData, beautifulGame, fastgltf::Options::None, fastgltf::Category::Materials);
    REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

    REQUIRE(asset->materials.size() >= 5);

    auto& materials = asset->materials;
    REQUIRE(materials[5].volume != nullptr);
    REQUIRE(glm::epsilonEqual(materials[5].volume->thicknessFactor, 0.2199999988079071f, glm::epsilon<float>()));
    REQUIRE(glm::epsilonEqual(materials[5].volume->attenuationColor[0], 0.800000011920929f, glm::epsilon<float>()));
    REQUIRE(glm::epsilonEqual(materials[5].volume->attenuationColor[1], 0.800000011920929f, glm::epsilon<float>()));
    REQUIRE(glm::epsilonEqual(materials[5].volume->attenuationColor[2], 0.800000011920929f, glm::epsilon<float>()));

    REQUIRE(materials[5].transmission != nullptr);
    REQUIRE(materials[5].transmission->transmissionFactor == 1.0f);
}

TEST_CASE("Test KHR_materials_clearcoat", "[gltf-loader]") {
    auto clearcoatTest = sampleModels / "2.0" / "ClearCoatTest" / "glTF";
    fastgltf::GltfDataBuffer jsonData;
    REQUIRE(jsonData.loadFromFile(clearcoatTest / "ClearCoatTest.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_clearcoat);
    auto asset = parser.loadGltfJson(&jsonData, clearcoatTest, fastgltf::Options::None, fastgltf::Category::Materials);
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

TEST_CASE("Test EXT_mesh_gpu_instancing", "[gltf-loader]") {
    auto simpleInstancingTest = sampleModels / "2.0" / "SimpleInstancing" / "glTF";
    fastgltf::GltfDataBuffer jsonData;
    REQUIRE(jsonData.loadFromFile(simpleInstancingTest / "SimpleInstancing.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::EXT_mesh_gpu_instancing);
    auto asset = parser.loadGltfJson(&jsonData, simpleInstancingTest, fastgltf::Options::None, fastgltf::Category::Accessors | fastgltf::Category::Nodes);
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
TEST_CASE("Test KHR_materials_pbrSpecularGlossiness", "[gltf-loader]") {
    auto specularGlossinessTest = sampleModels / "2.0" / "SpecGlossVsMetalRough" / "glTF";
    fastgltf::GltfDataBuffer jsonData;
    REQUIRE(jsonData.loadFromFile(specularGlossinessTest / "SpecGlossVsMetalRough.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_pbrSpecularGlossiness | fastgltf::Extensions::KHR_materials_specular);
    auto asset = parser.loadGltfJson(&jsonData, specularGlossinessTest);
    REQUIRE(asset.error() == fastgltf::Error::None);
    REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

    REQUIRE(asset->materials.size() == 4);

    auto& materials = asset->materials;
    REQUIRE(materials[0].specularGlossiness != nullptr);
    REQUIRE(materials[0].specularGlossiness->diffuseFactor[0] == 1.0f);
    REQUIRE(materials[0].specularGlossiness->diffuseFactor[1] == 1.0f);
    REQUIRE(materials[0].specularGlossiness->diffuseFactor[2] == 1.0f);
    REQUIRE(materials[0].specularGlossiness->diffuseFactor[3] == 1.0f);
    REQUIRE(materials[0].specularGlossiness->specularFactor[0] == 1.0f);
    REQUIRE(materials[0].specularGlossiness->specularFactor[1] == 1.0f);
    REQUIRE(materials[0].specularGlossiness->specularFactor[2] == 1.0f);
    REQUIRE(materials[0].specularGlossiness->glossinessFactor == 1.0f);
    REQUIRE(materials[0].specularGlossiness->diffuseTexture.has_value());
    REQUIRE(materials[0].specularGlossiness->diffuseTexture.value().textureIndex == 5);
    REQUIRE(materials[0].specularGlossiness->specularGlossinessTexture.has_value());
    REQUIRE(materials[0].specularGlossiness->specularGlossinessTexture.value().textureIndex == 6);

    REQUIRE(materials[3].specularGlossiness != nullptr);
    REQUIRE(materials[3].specularGlossiness->diffuseFactor[0] == 1.0f);
    REQUIRE(materials[3].specularGlossiness->diffuseFactor[1] == 1.0f);
    REQUIRE(materials[3].specularGlossiness->diffuseFactor[2] == 1.0f);
    REQUIRE(materials[3].specularGlossiness->diffuseFactor[3] == 1.0f);
    REQUIRE(materials[3].specularGlossiness->specularFactor[0] == 0.0f);
    REQUIRE(materials[3].specularGlossiness->specularFactor[1] == 0.0f);
    REQUIRE(materials[3].specularGlossiness->specularFactor[2] == 0.0f);
    REQUIRE(materials[3].specularGlossiness->glossinessFactor == 0.0f);
    REQUIRE(materials[3].specularGlossiness->diffuseTexture.has_value());
    REQUIRE(materials[3].specularGlossiness->diffuseTexture.value().textureIndex == 7);
}
#endif

TEST_CASE("Test KHR_materials_dispersion", "[gltf-loader]") {
	constexpr std::string_view json = R"({"materials": [
        {
            "extensions": {
                "KHR_materials_dispersion": {
                    "dispersion": 0.1
                }
            }
        }
    ]})";
	fastgltf::GltfDataBuffer jsonData;
	jsonData.copyBytes(reinterpret_cast<const uint8_t*>(json.data()), json.size());

	fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_dispersion);
	auto asset = parser.loadGltfJson(&jsonData, {}, fastgltf::Options::DontRequireValidAssetMember);
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	REQUIRE(asset->materials.size() == 1);
	REQUIRE(asset->materials.front().dispersion == 0.1f);
}

TEST_CASE("Test KHR_materials_variant", "[gltf-loader]") {
	auto velvetSofa = sampleModels / "2.0" / "GlamVelvetSofa" / "glTF";
	fastgltf::GltfDataBuffer jsonData;
	REQUIRE(jsonData.loadFromFile(velvetSofa / "GlamVelvetSofa.gltf"));

	fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_variants | fastgltf::Extensions::KHR_texture_transform);
	auto asset = parser.loadGltfJson(&jsonData, velvetSofa, fastgltf::Options::None);
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
	REQUIRE(primitive.mappings[0] == 2);
	REQUIRE(primitive.mappings[1] == 3);
	REQUIRE(primitive.mappings[2] == 4);
	REQUIRE(primitive.mappings[3] == 5);
	REQUIRE(primitive.mappings[4] == 6);
}

TEST_CASE("Test EXT_meshopt_compression", "[gltf-loader]") {
	auto brainStem = sampleModels / "2.0" / "BrainStem" / "glTF-Meshopt";
	fastgltf::GltfDataBuffer jsonData;
	REQUIRE(jsonData.loadFromFile(brainStem / "BrainStem.gltf"));

	fastgltf::Parser parser(fastgltf::Extensions::EXT_meshopt_compression | fastgltf::Extensions::KHR_mesh_quantization);
	auto asset = parser.loadGltfJson(&jsonData, brainStem, fastgltf::Options::None);
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	for (auto i = 0; i < 8; ++i) {
		REQUIRE(bool(asset->bufferViews[i].meshoptCompression));
	}

	{
		auto& mc = *asset->bufferViews[0].meshoptCompression.get();
		REQUIRE(mc.bufferIndex == 0);
		REQUIRE(mc.byteOffset == 0);
		REQUIRE(mc.byteLength == 2646);
		REQUIRE(mc.byteStride == 4);
		REQUIRE(mc.mode == fastgltf::MeshoptCompressionMode::Attributes);
		REQUIRE(mc.count == 34084);
	}
	{
		auto& mc = *asset->bufferViews[1].meshoptCompression.get();
		REQUIRE(mc.bufferIndex == 0);
		REQUIRE(mc.byteOffset == 2648);
		REQUIRE(mc.byteLength == 68972);
		REQUIRE(mc.byteStride == 4);
		REQUIRE(mc.mode == fastgltf::MeshoptCompressionMode::Attributes);
		REQUIRE(mc.filter == fastgltf::MeshoptCompressionFilter::Octahedral);
		REQUIRE(mc.count == 34084);
	}
}
