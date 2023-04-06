#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "base64_decode.hpp"
#include "fastgltf_parser.hpp"
#include "fastgltf_types.hpp"
#include "gltf_path.hpp"

TEST_CASE("Component type tests", "[gltf-loader]") {
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

    REQUIRE(fastgltf::getElementByteSize(AccessorType::Scalar, ComponentType::Byte)  == 1);
    REQUIRE(fastgltf::getElementByteSize(AccessorType::Vec4,   ComponentType::Byte)  == 4);
    REQUIRE(fastgltf::getElementByteSize(AccessorType::Vec4,   ComponentType::Short) == 8);

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

TEST_CASE("Test if glTF type detection works", "[gltf-loader]") {
    fastgltf::Parser parser;

    SECTION("glTF") {
        auto gltfPath = sampleModels / "2.0" / "ABeautifulGame" / "glTF";
        REQUIRE(std::filesystem::exists(gltfPath));
        fastgltf::GltfDataBuffer jsonData;
        REQUIRE(jsonData.loadFromFile(gltfPath / "ABeautifulGame.gltf"));
        REQUIRE(fastgltf::determineGltfFileType(&jsonData) == fastgltf::GltfType::glTF);

        auto model = parser.loadGLTF(&jsonData, gltfPath);
        REQUIRE(parser.getError() == fastgltf::Error::None);
        REQUIRE(model != nullptr);
    }

    SECTION("GLB") {
        auto glbPath = sampleModels / "2.0" / "BoomBox" / "glTF-Binary";
        REQUIRE(std::filesystem::exists(glbPath));
        fastgltf::GltfDataBuffer jsonData;
        REQUIRE(jsonData.loadFromFile(glbPath / "BoomBox.glb"));
        REQUIRE(fastgltf::determineGltfFileType(&jsonData) == fastgltf::GltfType::GLB);

        auto model = parser.loadBinaryGLTF(&jsonData, glbPath);
        REQUIRE(parser.getError() == fastgltf::Error::None);
        REQUIRE(model != nullptr);
    }

    SECTION("Invalid") {
        auto gltfPath = path / "base64.txt"; // Random file in the test directory that's not a glTF file.
        REQUIRE(std::filesystem::exists(gltfPath));
        fastgltf::GltfDataBuffer jsonData;
        REQUIRE(jsonData.loadFromFile(gltfPath));
        REQUIRE(fastgltf::determineGltfFileType(&jsonData) == fastgltf::GltfType::Invalid);
    }
}

TEST_CASE("Loading some basic glTF", "[gltf-loader]") {
    fastgltf::Parser parser;
    SECTION("Loading basic invalid glTF files") {
        auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
        REQUIRE(jsonData->loadFromFile(path / "empty_json.gltf"));
        auto emptyGltf = parser.loadGLTF(jsonData.get(), path);
        REQUIRE(emptyGltf->parse() == fastgltf::Error::InvalidOrMissingAssetField);
    }

    SECTION("Load basic glTF file") {
        auto basicJsonData = std::make_unique<fastgltf::GltfDataBuffer>();
        REQUIRE(basicJsonData->loadFromFile(path / "basic_gltf.gltf"));

        auto basicGltf = parser.loadGLTF(basicJsonData.get(), path);
        REQUIRE(basicGltf != nullptr);
        REQUIRE(parser.getError() == fastgltf::Error::None);
    }

    SECTION("Loading basic Cube.gltf") {
        auto cubePath = sampleModels / "2.0" / "Cube" / "glTF";
        auto cubeJsonData = std::make_unique<fastgltf::GltfDataBuffer>();
        REQUIRE(cubeJsonData->loadFromFile(cubePath / "Cube.gltf"));

        auto cubeGltf = parser.loadGLTF(cubeJsonData.get(), cubePath);
        REQUIRE(parser.getError() == fastgltf::Error::None);
        REQUIRE(cubeGltf != nullptr);

        REQUIRE(cubeGltf->parse(fastgltf::Category::OnlyRenderable) == fastgltf::Error::None);
        REQUIRE(cubeGltf->validate() == fastgltf::Error::None);

        auto cube = cubeGltf->getParsedAsset();
        REQUIRE(cube->scenes.size() == 1);
        REQUIRE(cube->scenes.front().nodeIndices.size() == 1);
        REQUIRE(cube->scenes.front().nodeIndices.front() == 0);

        REQUIRE(cube->nodes.size() == 1);
        REQUIRE(cube->nodes.front().name == "Cube");
        REQUIRE(std::holds_alternative<fastgltf::Node::TRS>(cube->nodes.front().transform));

        REQUIRE(cube->accessors.size() == 5);
        REQUIRE(cube->accessors[0].type == fastgltf::AccessorType::Scalar);
        REQUIRE(cube->accessors[0].componentType == fastgltf::ComponentType::UnsignedShort);
        REQUIRE(cube->accessors[1].type == fastgltf::AccessorType::Vec3);
        REQUIRE(cube->accessors[1].componentType == fastgltf::ComponentType::Float);

        REQUIRE(cube->bufferViews.size() == 5);
        REQUIRE(cube->buffers.size() == 1);

        REQUIRE(cube->materials.size() == 1);
        auto& material = cube->materials.front();
        REQUIRE(material.name == "Cube");
        REQUIRE(material.pbrData.has_value());
        REQUIRE(material.pbrData->baseColorTexture.has_value());
        REQUIRE(material.pbrData->baseColorTexture->textureIndex == 0);
        REQUIRE(material.pbrData->metallicRoughnessTexture.has_value());
        REQUIRE(material.pbrData->metallicRoughnessTexture->textureIndex == 1);
        REQUIRE(!material.normalTexture.has_value());
        REQUIRE(!material.emissiveTexture.has_value());
        REQUIRE(!material.occlusionTexture.has_value());
    }

    SECTION("Loading basic Box.gltf") {
        auto boxPath = sampleModels / "2.0" / "Box" / "glTF";
        auto boxJsonData = std::make_unique<fastgltf::GltfDataBuffer>();
        REQUIRE(boxJsonData->loadFromFile(boxPath / "Box.gltf"));

        auto boxGltf = parser.loadGLTF(boxJsonData.get(), boxPath);
        REQUIRE(parser.getError() == fastgltf::Error::None);
        REQUIRE(boxGltf != nullptr);

        REQUIRE(boxGltf->parse(fastgltf::Category::OnlyRenderable) == fastgltf::Error::None);
        REQUIRE(boxGltf->validate() == fastgltf::Error::None);

        auto box = boxGltf->getParsedAsset();
        REQUIRE(box->defaultScene.has_value());
        REQUIRE(box->defaultScene.value() == 0);

        REQUIRE(box->nodes.size() == 2);
        REQUIRE(box->nodes[0].children.size() == 1);
        REQUIRE(box->nodes[0].children[0] == 1);
        REQUIRE(box->nodes[1].children.empty());
        REQUIRE(box->nodes[1].meshIndex.has_value());
        REQUIRE(box->nodes[1].meshIndex.value() == 0);

        REQUIRE(box->materials.size() == 1);
        REQUIRE(box->materials[0].name == "Red");
        REQUIRE(box->materials[0].pbrData.has_value());
        REQUIRE(box->materials[0].pbrData->baseColorFactor[3] == 1.0f);
        REQUIRE(box->materials[0].pbrData->metallicFactor == 0.0f);
    }
}

TEST_CASE("Loading KHR_texture_basisu glTF files", "[gltf-loader]") {
    auto stainedLamp = sampleModels / "2.0" / "StainedGlassLamp" / "glTF-KTX-BasisU";

    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(stainedLamp / "StainedGlassLamp.gltf"));

    SECTION("Loading KHR_texture_basisu") {
        fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);
        auto stainedGlassLamp = parser.loadGLTF(jsonData.get(), path, fastgltf::Options::DontRequireValidAssetMember);
        REQUIRE(parser.getError() == fastgltf::Error::None);
        REQUIRE(stainedGlassLamp != nullptr);

        REQUIRE(stainedGlassLamp->parse(fastgltf::Category::Textures | fastgltf::Category::Images) == fastgltf::Error::None);
        REQUIRE(stainedGlassLamp->validate() == fastgltf::Error::None);

        auto asset = stainedGlassLamp->getParsedAsset();
        REQUIRE(asset->textures.size() == 19);
        REQUIRE(!asset->images.empty());

        auto& texture = asset->textures[1];
        REQUIRE(texture.imageIndex == 1);
        REQUIRE(texture.samplerIndex == 0);
        REQUIRE(!texture.fallbackImageIndex.has_value());

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
        auto stainedGlassLamp = parser.loadGLTF(jsonData.get(), path, fastgltf::Options::DontRequireValidAssetMember);
        REQUIRE(stainedGlassLamp->parse() == fastgltf::Error::MissingExtensions);
    }
}

TEST_CASE("Loading KHR_texture_transform glTF files", "[gltf-loader]") {
    auto transformTest = sampleModels / "2.0" / "TextureTransformMultiTest" / "glTF";

    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(transformTest / "TextureTransformMultiTest.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_transform);
    auto test = parser.loadGLTF(jsonData.get(), transformTest, fastgltf::Options::DontRequireValidAssetMember);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(test != nullptr);

    REQUIRE(test->parse(fastgltf::Category::Materials) == fastgltf::Error::None);
    REQUIRE(test->validate() == fastgltf::Error::None);

    auto asset = test->getParsedAsset();
    REQUIRE(!asset->materials.empty());

    auto& material = asset->materials.front();
    REQUIRE(material.pbrData.has_value());
    REQUIRE(material.pbrData->baseColorTexture.has_value());
    REQUIRE(material.pbrData->baseColorTexture->uvOffset[0] == 0.705f);
    REQUIRE(material.pbrData->baseColorTexture->rotation == Catch::Approx(1.5707963705062866f));
}

TEST_CASE("Loading glTF animation", "[gltf-loader]") {
    auto animatedCube = sampleModels / "2.0" / "AnimatedCube" / "glTF";

    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(animatedCube / "AnimatedCube.gltf"));

    fastgltf::Parser parser;
    auto cube = parser.loadGLTF(jsonData.get(), animatedCube);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(cube != nullptr);

    REQUIRE(cube->parse(fastgltf::Category::OnlyAnimations) == fastgltf::Error::None);
    REQUIRE(cube->validate() == fastgltf::Error::None);

    auto asset = cube->getParsedAsset();
    REQUIRE(!asset->animations.empty());

    auto& animation = asset->animations.front();
    REQUIRE(animation.name == "animation_AnimatedCube");

    REQUIRE(!animation.channels.empty());
    REQUIRE(animation.channels.front().nodeIndex == 0);
    REQUIRE(animation.channels.front().samplerIndex == 0);
    REQUIRE(animation.channels.front().path == fastgltf::AnimationPath::Rotation);

    REQUIRE(!animation.samplers.empty());
    REQUIRE(animation.samplers.front().interpolation == fastgltf::AnimationInterpolation::Linear);
    REQUIRE(animation.samplers.front().inputAccessor == 0);
    REQUIRE(animation.samplers.front().outputAccessor == 1);
}

TEST_CASE("Loading glTF skins", "[gltf-loader]") {
    auto simpleSkin = sampleModels / "2.0" / "SimpleSkin" / "glTF";

    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(simpleSkin / "SimpleSkin.gltf"));

    fastgltf::Parser parser;
    auto model = parser.loadGLTF(jsonData.get(), simpleSkin);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(model != nullptr);

    REQUIRE(model->parse(fastgltf::Category::Skins | fastgltf::Category::Nodes) == fastgltf::Error::None);
    REQUIRE(model->validate() == fastgltf::Error::None);

    auto asset = model->getParsedAsset();
    REQUIRE(!asset->skins.empty());

    auto& skin = asset->skins.front();
    REQUIRE(skin.joints.size() == 2);
    REQUIRE(skin.joints[0] == 1);
    REQUIRE(skin.joints[1] == 2);
    REQUIRE(skin.inverseBindMatrices.has_value());
    REQUIRE(skin.inverseBindMatrices.value() == 4);

    REQUIRE(!asset->nodes.empty());

    auto& node = asset->nodes.front();
    REQUIRE(node.skinIndex.has_value());
    REQUIRE(node.skinIndex == 0);
}

TEST_CASE("Loading glTF cameras", "[gltf-loader]") {
    auto cameras = sampleModels / "2.0" / "Cameras" / "glTF";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(cameras / "Cameras.gltf"));

    fastgltf::Parser parser;
    auto model = parser.loadGLTF(jsonData.get(), cameras);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(model != nullptr);

    REQUIRE(model->parse(fastgltf::Category::Cameras) == fastgltf::Error::None);
    REQUIRE(model->validate() == fastgltf::Error::None);

    auto asset = model->getParsedAsset();
    REQUIRE(asset->cameras.size() == 2);

    REQUIRE(std::holds_alternative<fastgltf::Camera::Perspective>(asset->cameras[0].camera));
    REQUIRE(std::holds_alternative<fastgltf::Camera::Orthographic>(asset->cameras[1].camera));

    const auto* pPerspective = std::get_if<fastgltf::Camera::Perspective>(&asset->cameras[0].camera);
    REQUIRE(pPerspective != nullptr);
    REQUIRE(pPerspective->aspectRatio == 1.0f);
    REQUIRE(pPerspective->yfov == 0.7f);
    REQUIRE(pPerspective->zfar == 100);
    REQUIRE(pPerspective->znear == 0.01f);

    const auto* pOrthographic = std::get_if<fastgltf::Camera::Orthographic>(&asset->cameras[1].camera);
    REQUIRE(pOrthographic != nullptr);
    REQUIRE(pOrthographic->xmag == 1.0f);
    REQUIRE(pOrthographic->ymag == 1.0f);
    REQUIRE(pOrthographic->zfar == 100);
    REQUIRE(pOrthographic->znear == 0.01f);
}

TEST_CASE("Validate whole glTF", "[gltf-loader]") {
    auto sponza = sampleModels / "2.0" / "Sponza" / "glTF";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(sponza / "Sponza.gltf"));

    fastgltf::Parser parser;
    auto model = parser.loadGLTF(jsonData.get(), sponza);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(model != nullptr);

    REQUIRE(model->parse() == fastgltf::Error::None);
    REQUIRE(model->validate() == fastgltf::Error::None);

    auto brainStem = sampleModels / "2.0" / "BrainStem" / "glTF";
    jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(brainStem / "BrainStem.gltf"));

    model = parser.loadGLTF(jsonData.get(), brainStem);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(model != nullptr);

    REQUIRE(model->parse() == fastgltf::Error::None);
    REQUIRE(model->validate() == fastgltf::Error::None);
}

TEST_CASE("Test allocation callbacks for embedded buffers", "[gltf-loader]") {
    auto boxPath = sampleModels / "2.0" / "Box" / "glTF-Embedded";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(boxPath / "Box.gltf"));

    std::vector<void*> allocations;

    auto mapCallback = [](uint64_t bufferSize, void* userPointer) -> fastgltf::BufferInfo {
        REQUIRE(userPointer != nullptr);
        auto* allocations = static_cast<std::vector<void*>*>(userPointer);
        allocations->emplace_back(std::malloc(bufferSize));
        return fastgltf::BufferInfo {
            allocations->back(),
            allocations->size() - 1,
        };
    };

    fastgltf::Parser parser;
    parser.setUserPointer(&allocations);
    parser.setBufferAllocationCallback(mapCallback, nullptr);
    auto model = parser.loadGLTF(jsonData.get(), boxPath);
    REQUIRE(model != nullptr);
    REQUIRE(model->parse(fastgltf::Category::Buffers) == fastgltf::Error::None);
    REQUIRE(allocations.size() == 1);

    auto asset = model->getParsedAsset();
    REQUIRE(asset->buffers.size() == 1);

    auto& buffer = asset->buffers.front();
    const auto* customBuffer = std::get_if<fastgltf::sources::CustomBuffer>(&buffer.data);
    REQUIRE(customBuffer != nullptr);
    REQUIRE(customBuffer->id == 0);

    for (auto& allocation : allocations) {
        REQUIRE(allocation != nullptr);
        std::free(allocation);
    }
}

TEST_CASE("Test base64 decoding callbacks", "[gltf-loader]") {
    auto boxPath = sampleModels / "2.0" / "Box" / "glTF-Embedded";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(boxPath / "Box.gltf"));

    size_t decodeCounter = 0;
    auto decodeCallback = [](std::string_view encodedData, uint8_t* outputData, size_t padding, size_t outputSize, void* userPointer) {
        (*static_cast<size_t*>(userPointer))++;
        fastgltf::base64::decode_inplace(encodedData, outputData, padding);
    };

    fastgltf::Parser parser;
    parser.setUserPointer(&decodeCounter);
    parser.setBase64DecodeCallback(decodeCallback);
    auto model = parser.loadGLTF(jsonData.get(), boxPath);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(model != nullptr);
    REQUIRE(model->parse(fastgltf::Category::Buffers) == fastgltf::Error::None);
    REQUIRE(model->validate() == fastgltf::Error::None);
    REQUIRE(decodeCounter != 0);
}

TEST_CASE("Test TRS parsing and optional decomposition", "[gltf-loader]") {
    SECTION("Test decomposition on glTF asset") {
        auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
        REQUIRE(jsonData->loadFromFile(path / "transform_matrices.gltf"));

        // Parse once without decomposing, once with decomposing the matrix.
        fastgltf::Parser parser;
        auto modelWithMatrix = parser.loadGLTF(jsonData.get(), path);
        REQUIRE(modelWithMatrix != nullptr);
        REQUIRE(parser.getError() == fastgltf::Error::None);
        REQUIRE(modelWithMatrix != nullptr);

        REQUIRE(modelWithMatrix->parse(fastgltf::Category::Nodes | fastgltf::Category::Cameras) == fastgltf::Error::None);
        REQUIRE(modelWithMatrix->validate() == fastgltf::Error::None);
        auto assetWithMatrix = modelWithMatrix->getParsedAsset();

        auto modelDecomposed = parser.loadGLTF(jsonData.get(), path, fastgltf::Options::DecomposeNodeMatrices);
        REQUIRE(parser.getError() == fastgltf::Error::None);
        REQUIRE(modelWithMatrix != nullptr);

        REQUIRE(modelDecomposed->parse(fastgltf::Category::Nodes | fastgltf::Category::Cameras) == fastgltf::Error::None);
        REQUIRE(modelDecomposed->validate() == fastgltf::Error::None);
        auto assetDecomposed = modelDecomposed->getParsedAsset();

        REQUIRE(assetWithMatrix->cameras.size() == 1);
        REQUIRE(assetDecomposed->cameras.size() == 1);
        REQUIRE(assetWithMatrix->nodes.size() == 2);
        REQUIRE(assetDecomposed->nodes.size() == 2);
        REQUIRE(std::holds_alternative<fastgltf::Node::TransformMatrix>(assetWithMatrix->nodes.back().transform));
        REQUIRE(std::holds_alternative<fastgltf::Node::TRS>(assetDecomposed->nodes.back().transform));

        // Get the TRS components from the first node and use them as the test data for decomposing.
        const auto* pDefaultTRS = std::get_if<fastgltf::Node::TRS>(&assetWithMatrix->nodes.front().transform);
        REQUIRE(pDefaultTRS != nullptr);
        auto translation = glm::make_vec3(pDefaultTRS->translation.data());
        auto rotation = glm::make_quat(pDefaultTRS->rotation.data());
        auto scale = glm::make_vec3(pDefaultTRS->scale.data());
        auto rotationMatrix = glm::toMat4(rotation);
        auto transform = glm::translate(glm::mat4(1.0f), translation) * rotationMatrix * glm::scale(glm::mat4(1.0f), scale);

        // Check if the parsed matrix is correct.
        const auto* pMatrix = std::get_if<fastgltf::Node::TransformMatrix>(&assetWithMatrix->nodes.back().transform);
        REQUIRE(pMatrix != nullptr);
        REQUIRE(glm::make_mat4x4(pMatrix->data()) == transform);

        // Check if the decomposed components equal the original components.
        const auto* pDecomposedTRS = std::get_if<fastgltf::Node::TRS>(&assetDecomposed->nodes.back().transform);
        REQUIRE(glm::make_vec3(pDecomposedTRS->translation.data()) == translation);
        REQUIRE(glm::make_quat(pDecomposedTRS->rotation.data()) == rotation);
        REQUIRE(glm::make_vec3(pDecomposedTRS->scale.data()) == scale);
    }

    SECTION("Test decomposition against glm decomposition") {
        // Some random complex transform matrix from one of the glTF sample models.
        std::array<float, 16> matrix = {
            -0.4234085381031037F,
            -0.9059388637542724F,
            -7.575183536001616e-11F,
            0.0F,
            -0.9059388637542724F,
            0.4234085381031037F,
            -4.821281221478735e-11F,
            0.0F,
            7.575183536001616e-11F,
            4.821281221478735e-11F,
            -1.0F,
            0.0F,
            -90.59386444091796F,
            -24.379817962646489F,
            -40.05522918701172F,
            1.0F
        };

        std::array<float, 3> translation = {}, scale = {};
        std::array<float, 4> rotation = {};
        fastgltf::decomposeTransformMatrix(matrix, scale, rotation, translation);

        auto glmMatrix = glm::make_mat4x4(matrix.data());
        glm::vec3 glmScale, glmTranslation, glmSkew;
        glm::quat glmRotation;
        glm::vec4 glmPerspective;
        glm::decompose(glmMatrix, glmScale, glmRotation, glmTranslation, glmSkew, glmPerspective);

        // I use glm::epsilon<float>() * 10 here because some matrices I tested this with resulted
        // in an error margin greater than the normal epsilon value. I will investigate this in the
        // future, but I suspect using double in the decompose functions should help mitigate most
        // of it.
        REQUIRE(glm::make_vec3(translation.data()) == glmTranslation);
        REQUIRE(glm::all(glm::epsilonEqual(glm::make_quat(rotation.data()), glmRotation, glm::epsilon<float>() * 10)));
        REQUIRE(glm::all(glm::epsilonEqual(glm::make_vec3(scale.data()), glmScale, glm::epsilon<float>())));
    }
}

TEST_CASE("Validate sparse accessor parsing", "[gltf-loader]") {
    auto simpleSparseAccessor = sampleModels / "2.0" / "SimpleSparseAccessor" / "glTF";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(simpleSparseAccessor / "SimpleSparseAccessor.gltf"));

    fastgltf::Parser parser;
    auto model = parser.loadGLTF(jsonData.get(), simpleSparseAccessor);
    REQUIRE(model != nullptr);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(model->parse(fastgltf::Category::Accessors) == fastgltf::Error::None);
    REQUIRE(model->validate() == fastgltf::Error::None);

    auto asset = model->getParsedAsset();
    REQUIRE(asset->accessors.size() == 2);
    REQUIRE(!asset->accessors[0].sparse.has_value());
    REQUIRE(asset->accessors[1].sparse.has_value());
    auto& sparse = asset->accessors[1].sparse.value();
    REQUIRE(sparse.count == 3);
    REQUIRE(sparse.bufferViewIndices == 2);
    REQUIRE(sparse.byteOffsetIndices == 0);
    REQUIRE(sparse.bufferViewValues == 3);
    REQUIRE(sparse.byteOffsetValues == 0);
    REQUIRE(sparse.indexComponentType == fastgltf::ComponentType::UnsignedShort);
}

TEST_CASE("Validate morph target parsing", "[gltf-loader]") {
    auto simpleMorph = sampleModels / "2.0" / "SimpleMorph" / "glTF";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(simpleMorph / "SimpleMorph.gltf"));

    fastgltf::Parser parser;
    auto model = parser.loadGLTF(jsonData.get(), simpleMorph);
    REQUIRE(model != nullptr);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(model->parse(fastgltf::Category::Meshes) == fastgltf::Error::None);
    REQUIRE(model->validate() == fastgltf::Error::None);

    auto asset = model->getParsedAsset();
    REQUIRE(asset->meshes.size() == 1);
    REQUIRE(asset->meshes.front().weights.size() == 2);
    REQUIRE(asset->meshes.front().primitives.size() == 1);

    auto& primitive = asset->meshes.front().primitives.front();
    REQUIRE(primitive.attributes.find("POSITION") != primitive.attributes.end());
    REQUIRE(primitive.attributes["POSITION"] == 1);

    REQUIRE(primitive.targets.size() == 2);
    REQUIRE(primitive.targets[0].find("POSITION") != primitive.targets[0].end());
    REQUIRE(primitive.targets[0]["POSITION"] == 2);
    REQUIRE(primitive.targets[1].find("POSITION") != primitive.targets[1].end());
    REQUIRE(primitive.targets[1]["POSITION"] == 3);
}

TEST_CASE("Test KHR_lights_punctual", "[gltf-loader]") {
    auto lightsLamp = sampleModels / "2.0" / "LightsPunctualLamp" / "glTF";
    fastgltf::GltfDataBuffer jsonData;
    REQUIRE(jsonData.loadFromFile(lightsLamp / "LightsPunctualLamp.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual);
    auto model = parser.loadGLTF(&jsonData, lightsLamp);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(model->parse(fastgltf::Category::Nodes) == fastgltf::Error::None);
    REQUIRE(model->validate() == fastgltf::Error::None);

    auto asset = model->getParsedAsset();
    REQUIRE(asset->lights.size() == 5);
    REQUIRE(asset->nodes.size() > 4);

    auto& nodes = asset->nodes;
    REQUIRE(nodes[3].lightsIndex.has_value());
    REQUIRE(nodes[3].lightsIndex.value() == 0);

    auto& lights = asset->lights;
    REQUIRE(lights[0].name == "Point");
    REQUIRE(lights[0].type == fastgltf::LightType::Point);
    REQUIRE(lights[0].intensity == 15.0f);
    REQUIRE(glm::epsilonEqual(lights[0].color[0], 1.0f, glm::epsilon<float>()));
    REQUIRE(glm::epsilonEqual(lights[0].color[1], 0.63187497854232788f, glm::epsilon<float>()));
    REQUIRE(glm::epsilonEqual(lights[0].color[2], 0.23909975588321689f, glm::epsilon<float>()));
}

TEST_CASE("Test accessors min/max", "[gltf-loader]") {
    auto lightsLamp = sampleModels / "2.0" / "LightsPunctualLamp" / "glTF";
    fastgltf::GltfDataBuffer jsonData;
    REQUIRE(jsonData.loadFromFile(lightsLamp / "LightsPunctualLamp.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual);
    auto model = parser.loadGLTF(&jsonData, lightsLamp);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(model->parse(fastgltf::Category::Accessors) == fastgltf::Error::None);
    REQUIRE(model->validate() == fastgltf::Error::None);

    auto asset = model->getParsedAsset();
    REQUIRE(asset->accessors.size() == 15);
    auto& accessors = asset->accessors;

    {
        auto& firstAccessor = accessors[0];
        const auto* max = std::get_if<std::vector<std::int64_t>>(&firstAccessor.max);
        const auto* min = std::get_if<std::vector<std::int64_t>>(&firstAccessor.min);
        REQUIRE(max != nullptr);
        REQUIRE(min != nullptr);
        REQUIRE(max->size() == fastgltf::getNumComponents(firstAccessor.type));
        REQUIRE(max->size() == 1);
        REQUIRE(min->size() == 1);
        REQUIRE(max->front() == 3211);
        REQUIRE(min->front() == 0);
    }

    {
        auto& secondAccessor = accessors[1];
        const auto* max = std::get_if<std::vector<double>>(&secondAccessor.max);
        const auto* min = std::get_if<std::vector<double>>(&secondAccessor.min);
        REQUIRE(max != nullptr);
        REQUIRE(min != nullptr);
        REQUIRE(max->size() == fastgltf::getNumComponents(secondAccessor.type));
        REQUIRE(max->size() == 3);
        REQUIRE(min->size() == 3);

        REQUIRE(glm::epsilonEqual(max->at(0), 0.81497824192047119, glm::epsilon<double>()));
        REQUIRE(glm::epsilonEqual(max->at(1), 1.8746249675750732, glm::epsilon<double>()));
        REQUIRE(glm::epsilonEqual(max->at(2), 0.32295516133308411, glm::epsilon<double>()));

        REQUIRE(glm::epsilonEqual(min->at(0), -0.12269512563943863, glm::epsilon<double>()));
        REQUIRE(glm::epsilonEqual(min->at(1), 0.013025385327637196, glm::epsilon<double>()));
        REQUIRE(glm::epsilonEqual(min->at(2), -0.32393229007720947, glm::epsilon<double>()));
    }

    {
        auto& fifthAccessor = accessors[4];
        const auto* max = std::get_if<std::vector<double>>(&fifthAccessor.max);
        const auto* min = std::get_if<std::vector<double>>(&fifthAccessor.min);
        REQUIRE(max != nullptr);
        REQUIRE(min != nullptr);
        REQUIRE(max->size() == fastgltf::getNumComponents(fifthAccessor.type));
        REQUIRE(max->size() == 4);
        REQUIRE(min->size() == 4);

        REQUIRE(max->back() == 1.0);
    }
}
