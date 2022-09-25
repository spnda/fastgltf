#include <array>
#include <fstream>
#include <functional>
#include <utility>

#include "simdjson.h"

#include "fastgltf_parser.hpp"
#include "fastgltf_types.hpp"
#include "base64_decode.hpp"

namespace fg = fastgltf;
namespace fs = std::filesystem;

namespace fastgltf {
    constexpr std::string_view mimeTypeJpeg = "image/jpeg";
    constexpr std::string_view mimeTypePng = "image/png";
    constexpr std::string_view mimeTypeKtx = "image/ktx2";
    constexpr std::string_view mimeTypeDds = "image/vnd-ms.dds";
    constexpr std::string_view mimeTypeGltfBuffer = "application/gltf-buffer";
    constexpr std::string_view mimeTypeOctetStream = "application/octet-stream";

    struct ParserData {
        // Can simdjson not store this data itself?
        std::vector<uint8_t> bytes;
        simdjson::dom::document doc;
        simdjson::dom::object root;
    };

    // ASCII for "glTF".
    constexpr uint32_t binaryGltfHeaderMagic = 0x46546C67;

    struct BinaryGltfHeader {
        uint32_t magic;
        uint32_t version;
        uint32_t length;
    };
    static_assert(sizeof(BinaryGltfHeader) == 12, "Binary gltf header must be 12 bytes");

    struct BinaryGltfChunk {
        uint32_t chunkLength;
        uint32_t chunkType;
    };

    [[nodiscard, gnu::always_inline]] std::tuple<bool, bool, size_t> getImageIndexForExtension(simdjson::dom::object& object, std::string_view extension);
    [[nodiscard, gnu::always_inline]] bool parseTextureExtensions(Texture& texture, simdjson::dom::object& extensions, Options options);

    [[nodiscard, gnu::always_inline]] std::pair<bool, Error> iterateOverArray(simdjson::dom::object& parent, std::string_view arrayName, const std::function<bool(simdjson::dom::element&)>& callback);
}

std::tuple<bool, bool, size_t> fg::getImageIndexForExtension(simdjson::dom::object& object, std::string_view extension) {
    using namespace simdjson;

    // Both KHR_texture_basisu and MSFT_texture_dds allow specifying an alternative
    // image source index.
    dom::object sourceExtensionObject;
    if (object[extension].get_object().get(sourceExtensionObject) != SUCCESS) {
        return std::make_tuple(false, true, 0);
    }

    // Check if the extension object provides a source index.
    size_t imageIndex;
    if (sourceExtensionObject["source"].get_uint64().get(imageIndex) != SUCCESS) {
        return std::make_tuple(true, false, 0);
    }

    return std::make_tuple(false, false, imageIndex);
};

std::pair<bool, fg::Error> fg::iterateOverArray(simdjson::dom::object& parent, std::string_view arrayName,
                                                 const std::function<bool(simdjson::dom::element&)>& callback) {
    using namespace simdjson;

    dom::array array;
    if (parent[arrayName].get_array().get(array) != SUCCESS) {
        return std::make_pair(false, Error::None);
    }

    /*size_t count = 0;
    if (array.count_elements().get(count) != SUCCESS) {
        return std::make_tuple(false, 0, Error::InvalidJson);
    }*/

    for (auto field : array) {
        if (!callback(field)) {
            return std::make_pair(false, Error::InvalidGltf);
        }
    }

    return std::make_pair(true, Error::None);
}

bool fg::parseTextureExtensions(Texture& texture, simdjson::dom::object& extensions, Options options) {
    if (hasBit(options, Options::LoadKTXExtension)) {
        auto [invalidGltf, extensionNotPresent, imageIndex] = getImageIndexForExtension(extensions, "KHR_texture_basisu");
        if (invalidGltf) {
            return false;
        }

        if (!extensionNotPresent) {
            texture.imageIndex = imageIndex;
            return true;
        }
    }

    if (hasBit(options, Options::LoadDDSExtension)) {
        auto [invalidGltf, extensionNotPresent, imageIndex] = getImageIndexForExtension(extensions, "MSFT_texture_dds");
        if (invalidGltf) {
            return false;
        }

        if (!extensionNotPresent) {
            texture.imageIndex = imageIndex;
            return true;
        }
    }

    return false;
}

#pragma region glTF
fg::glTF::glTF(std::unique_ptr<ParserData> data, fs::path directory, Options options) : data(std::move(data)), directory(std::move(directory)), options(options) {
    parsedAsset = std::make_unique<Asset>();
    glb = nullptr;
}

fg::glTF::glTF(std::unique_ptr<ParserData> data, fs::path file, std::vector<uint8_t>&& glbData, Options options) : data(std::move(data)), directory(file.parent_path()), options(options) {
    parsedAsset = std::make_unique<Asset>();
    glb = std::make_unique<GLBBuffer>();
    glb->buffer = std::move(glbData);
    glb->file = std::move(file);
}

fg::glTF::glTF(std::unique_ptr<ParserData> data, fs::path file, size_t fileOffset, size_t fileSize, Options options) : data(std::move(data)), directory(file.parent_path()), options(options) {
    parsedAsset = std::make_unique<Asset>();
    glb = std::make_unique<GLBBuffer>();
    glb->file = std::move(file);
    glb->fileOffset = fileOffset;
    glb->fileSize = fileSize;
}

// We define the destructor here as otherwise the definition would be generated in other cpp files
// in which the definition for ParserData is not available.
fg::glTF::~glTF() = default;

bool fg::glTF::checkAssetField() {
    using namespace simdjson;

    dom::object asset;
    if (data->root["asset"].get_object().get(asset) != SUCCESS) {
        errorCode = Error::InvalidOrMissingAssetField;
        return false;
    }

    std::string_view version;
    if (asset["version"].get_string().get(version) != SUCCESS) {
        errorCode = Error::InvalidOrMissingAssetField;
        return false;
    }

    return true;
}

std::tuple<fg::Error, fg::DataSource, fg::DataLocation> fg::glTF::decodeUri(std::string_view uri) const {
    if (uri.substr(0, 4) == "data") {
        // This is a data URI.
        auto index =  uri.find(';');
        auto encodingEnd = uri.find(',', index + 1);
        if (index == std::string::npos || encodingEnd == std::string::npos) {
            return std::make_tuple(Error::InvalidGltf, DataSource {}, DataLocation::None);
        }

        auto encoding = uri.substr(index + 1, encodingEnd - index - 1);
        if (encoding != "base64") {
            return std::make_tuple(Error::InvalidGltf, DataSource {}, DataLocation::None);
        }

        // Decode the base64 data.
        auto encodedData = uri.substr(encodingEnd + 1);
        std::vector<uint8_t> uriData;
        if (hasBit(options, Options::DontUseSIMD)) {
            uriData = base64::fallback_decode(encodedData);
        } else {
            uriData = base64::decode(encodedData);
        }

        DataSource source = {};
        source.mimeType = getMimeTypeFromString(uri.substr(5, index - 5));
        source.bytes = std::move(uriData);
        return std::make_tuple(Error::None, source, DataLocation::VectorWithMime);
    } else {
        DataSource source = {};
        source.path = directory / uri;
        return std::make_tuple(Error::None, source, DataLocation::FilePathWithByteRange);
    }
}

fg::MimeType fg::glTF::getMimeTypeFromString(std::string_view mime) {
    if (mime == mimeTypeJpeg) {
        return MimeType::JPEG;
    } else if (mime == mimeTypePng) {
        return MimeType::PNG;
    } else if (mime == mimeTypeKtx) {
        return MimeType::KTX2;
    } else if (mime == mimeTypeDds) {
        return MimeType::DDS;
    } else if (mime == mimeTypeGltfBuffer) {
        return MimeType::GltfBuffer;
    } else if (mime == mimeTypeOctetStream) {
        return MimeType::OctetStream;
    } else {
        return MimeType::None;
    }
}

std::unique_ptr<fg::Asset> fg::glTF::getParsedAsset() {
    // If there has been any errors we don't want the caller to get the partially parsed asset.
    if (errorCode != Error::None) {
        return nullptr;
    }
    return std::move(parsedAsset);
}

fg::Asset* fg::glTF::getParsedAssetPointer() {
    if (errorCode != Error::None) {
        return nullptr;
    }
    return parsedAsset.get();
}

fg::Error fg::glTF::parseAccessors() {
    using namespace simdjson;
    auto [foundAccessors, accessorError] = iterateOverArray(data->root, "accessors", [this](auto& value) mutable -> bool {
        // Required fields: "componentType", "count"
        Accessor accessor = {};
        dom::object accessorObject;
        if (value.get_object().get(accessorObject) != SUCCESS) {
            return false;
        }

        size_t componentType;
        if (accessorObject["componentType"].get_uint64().get(componentType) != SUCCESS) {
            return false;
        } else {
            accessor.componentType = getComponentType(static_cast<std::underlying_type_t<ComponentType>>(componentType));
            if (accessor.componentType == ComponentType::Double && !hasBit(options, Options::AllowDouble)) {
                return false;
            }
        }

        std::string_view accessorType;
        if (accessorObject["type"].get_string().get(accessorType) != SUCCESS) {
            return false;
        } else {
            accessor.type = getAccessorType(accessorType);
        }

        if (accessorObject["count"].get_uint64().get(accessor.count) != SUCCESS) {
            return false;
        }

        size_t bufferView;
        if (accessorObject["bufferView"].get_uint64().get(bufferView) == SUCCESS) {
            accessor.bufferViewIndex = bufferView;
        }

        // byteOffset is optional, but defaults to 0
        if (accessorObject["byteOffset"].get_uint64().get(accessor.byteOffset) != SUCCESS) {
            accessor.byteOffset = 0;
        }

        if (accessorObject["normalized"].get_bool().get(accessor.normalized) != SUCCESS) {
            accessor.normalized = false;
        }

        // name is optional.
        std::string_view name;
        if (accessorObject["name"].get_string().get(name) == SUCCESS) {
            accessor.name = std::string { name };
        }

        parsedAsset->accessors.emplace_back(std::move(accessor));
        return true;
    });

    if (!foundAccessors && accessorError != Error::None) {
        errorCode = accessorError;
    }
    return errorCode;
}

fg::Error fg::glTF::parseBuffers() {
    using namespace simdjson;
    size_t bufferIndex = 0;
    auto [foundBuffers, bufferError] = iterateOverArray(data->root, "buffers", [this, &bufferIndex](auto& value) mutable -> bool {
        // Required fields: "byteLength"
        Buffer buffer = {};
        dom::object bufferObject;
        if (value.get_object().get(bufferObject) != SUCCESS) {
            return false;
        }

        if (bufferObject["byteLength"].get_uint64().get(buffer.byteLength) != SUCCESS) {
            return false;
        }

        // When parsing GLB, there's a buffer object that will point to the BUF chunk in the
        // file. Otherwise, data must be specified in the "uri" field.
        std::string_view uri;
        if (bufferObject["uri"].get_string().get(uri) == SUCCESS) {
            auto [error, source, location] = decodeUri(uri);
            if (error != Error::None)
                return false;

            buffer.data = source;
            buffer.location = location;
        } else if (bufferIndex == 0 && glb != nullptr) {
            if (hasBit(options, Options::LoadGLBBuffers)) {
                // We've loaded the GLB chunk already.
                buffer.data.bytes = std::move(glb->buffer);
                buffer.location = DataLocation::VectorWithMime;
            } else if (!hasBit(options, Options::LoadGLBBuffers)) {
                // The GLB chunk has not been loaded.
                buffer.location = DataLocation::FilePathWithByteRange;
                buffer.data.path = glb->file;
                buffer.data.mimeType = MimeType::GltfBuffer;
                buffer.data.fileByteOffset = glb->fileOffset;
            }
        } else {
            // All other buffers have to contain a uri field.
            return false;
        }

        if (buffer.location == DataLocation::None) {
            return false;
        }

        // name is optional.
        std::string_view name;
        if (bufferObject["name"].get_string().get(name) == SUCCESS) {
            buffer.name = std::string { name };
        }

        ++bufferIndex;
        parsedAsset->buffers.emplace_back(std::move(buffer));
        return true;
    });

    if (!foundBuffers && bufferError != Error::None) {
        errorCode = bufferError;
    }

    return errorCode;
}

fg::Error fg::glTF::parseBufferViews() {
    using namespace simdjson;
    auto [foundBufferViews, bufferViewError] = iterateOverArray(data->root, "bufferViews", [this](auto& value) mutable -> bool {
        // Required fields: "bufferIndex", "byteLength"
        BufferView view = {};
        dom::object bufferViewObject;
        if (value.get_object().get(bufferViewObject) != SUCCESS) {
            return false;
        }

        // Required with normal glTF, not necessary with GLB files.
        if (bufferViewObject["buffer"].get_uint64().get(view.bufferIndex) != SUCCESS) {
            return false;
        }

        if (bufferViewObject["byteLength"].get_uint64().get(view.byteLength) != SUCCESS) {
            return false;
        }

        // byteOffset is optional, but defaults to 0
        if (bufferViewObject["byteOffset"].get_uint64().get(view.byteOffset) != SUCCESS) {
            view.byteOffset = 0;
        }

        size_t byteStride;
        if (bufferViewObject["byteStride"].get_uint64().get(byteStride) == SUCCESS) {
            view.byteStride = byteStride;
        }

        // target is optional
        size_t target;
        if (bufferViewObject["target"].get_uint64().get(target) == SUCCESS) {
            view.target = static_cast<BufferTarget>(target);
        }

        // name is optional.
        std::string_view name;
        if (bufferViewObject["name"].get_string().get(name) == SUCCESS) {
            view.name = std::string { name };
        }

        parsedAsset->bufferViews.emplace_back(std::move(view));
        return true;
    });

    if (!foundBufferViews && bufferViewError != Error::None) {
        errorCode = bufferViewError;
    }
    return errorCode;
}

fg::Error fg::glTF::parseImages() {
    using namespace simdjson;
    auto [foundImages, imageError] = iterateOverArray(data->root, "images", [this](auto& value) mutable -> bool {
        Image image;
        dom::object imageObject;
        if (value.get_object().get(imageObject) != SUCCESS) {
            return false;
        }

        std::string_view uri;
        if (imageObject["uri"].get_string().get(uri) == SUCCESS) {
            if (imageObject["bufferView"].error() == SUCCESS) {
                // If uri is declared, bufferView cannot be declared.
                return false;
            }
            auto [error, source, location] = decodeUri(uri);
            if (error != Error::None)
                return false;

            image.data = source;
            image.location = location;

            std::string_view mimeType;
            if (imageObject["mimeType"].get_string().get(mimeType) == SUCCESS) {
                image.data.mimeType = getMimeTypeFromString(mimeType);
            }
        }

        size_t bufferViewIndex;
        if (imageObject["bufferView"].get_uint64().get(bufferViewIndex) == SUCCESS) {
            std::string_view mimeType;
            if (imageObject["mimeType"].get_string().get(mimeType) != SUCCESS) {
                // If bufferView is defined, mimeType needs to also be defined.
                return false;
            }

            image.data.bufferViewIndex = bufferViewIndex;
            image.data.mimeType = getMimeTypeFromString(mimeType);
        }

        if (image.location == DataLocation::None) {
            return false;
        }

        // name is optional.
        std::string_view name;
        if (imageObject["name"].get_string().get(name) == SUCCESS) {
            image.name = std::string{name};
        }

        parsedAsset->images.emplace_back(std::move(image));
        return true;
    });

    if (!foundImages && imageError != Error::None) {
        errorCode = imageError;
    }
    return errorCode;
}

fg::Error fg::glTF::parseMaterials() {
    using namespace simdjson;
    auto [foundMaterials, materialsError] = iterateOverArray(data->root, "materials", [this](auto& value) mutable -> bool {
        dom::object materialObject;
        if (value.get_object().get(materialObject) != SUCCESS) {
            return false;
        }
        Material material = {};

        dom::array emissiveFactor;
        if (materialObject["emissiveFactor"].get_array().get(emissiveFactor) == SUCCESS) {
            if (emissiveFactor.size() != 3)
                return false;
            for (auto i = 0U; i < 3; ++i) {
                double val;
                if (emissiveFactor.at(i).get_double().get(val) != SUCCESS)
                    return false;
                material.emissiveFactor[i] = static_cast<float>(val);
            }
        } else {
            material.emissiveFactor = { 0, 0, 0 };
        }

        {
            TextureInfo normalTexture = {};
            auto error = parseTextureObject(&materialObject, "normalTexture", &normalTexture);
            if (error != Error::None) {
                return false;
            }
            material.normalTexture = normalTexture;
        }
        {
            TextureInfo occlusionTexture = {};
            auto error = parseTextureObject(&materialObject, "occlusionTexture", &occlusionTexture);
            if (error != Error::None) {
                return false;
            }
            material.occlusionTexture = occlusionTexture;
        }
        {
            TextureInfo emissiveTexture = {};
            auto error = parseTextureObject(&materialObject, "emissiveTexture", &emissiveTexture);
            if (error != Error::None) {
                return false;
            }
            material.emissiveTexture = emissiveTexture;
        }

        dom::object pbrMetallicRoughness;
        if (materialObject["pbrMetallicRoughness"].get_object().get(pbrMetallicRoughness) == SUCCESS) {
            PBRData pbr = {};

            dom::array baseColorFactor;
            if (pbrMetallicRoughness["baseColorFactor"].get_array().get(baseColorFactor) == SUCCESS) {
                for (auto i = 0U; i < 4; ++i) {
                    double val;
                    if (baseColorFactor.at(i).get_double().get(val) != SUCCESS) {
                        return false;
                    }
                    pbr.baseColorFactor[i] = static_cast<float>(val);
                }
            } else {
                pbr.baseColorFactor = { 1, 1, 1, 1 };
            }

            double factor;
            if (pbrMetallicRoughness["metallicFactor"].get_double().get(factor) == SUCCESS) {
                pbr.metallicFactor = static_cast<float>(factor);
            } else {
                pbr.metallicFactor = 1.0f;
            }
            if (pbrMetallicRoughness["roughnessFactor"].get_double().get(factor) == SUCCESS) {
                pbr.roughnessFactor = static_cast<float>(factor);
            } else {
                pbr.roughnessFactor = 1.0f;
            }

            {
                TextureInfo baseColorTexture = {};
                auto error = parseTextureObject(&pbrMetallicRoughness, "baseColorTexture", &baseColorTexture);
                if (error != Error::None) {
                    return false;
                }
                pbr.baseColorTexture = baseColorTexture;
            }
            {
                TextureInfo metallicRoughnessTexture = {};
                auto error = parseTextureObject(&pbrMetallicRoughness, "metallicRoughnessTexture", &metallicRoughnessTexture);
                if (error != Error::None) {
                    return false;
                }
                pbr.metallicRoughnessTexture = metallicRoughnessTexture;
            }

            material.pbrData = pbr;
        }

        // name is optional.
        std::string_view name;
        if (materialObject["name"].get_string().get(name) == SUCCESS) {
            material.name = std::string { name };
        }

        parsedAsset->materials.emplace_back(std::move(material));
        return true;
    });

    if (!foundMaterials && materialsError != Error::None) {
        errorCode = materialsError;
    }
    return errorCode;
}

fg::Error fg::glTF::parseMeshes() {
    using namespace simdjson;
    auto [foundMeshes, meshError] = iterateOverArray(data->root, "meshes", [this](auto& value) mutable -> bool {
        // Required fields: "primitives"
        dom::object meshObject;
        if (value.get_object().get(meshObject) != SUCCESS) {
            return false;
        }
        Mesh mesh = {};

        auto [foundPrimitives, primitiveError] = iterateOverArray(meshObject, "primitives", [&primitives = mesh.primitives](auto& value) mutable -> bool {
            using namespace simdjson; // Why MSVC?
            // Required fields: "attributes"
            Primitive primitive = {};
            dom::object primitiveObject;
            if (value.get_object().get(primitiveObject) != SUCCESS) {
                return false;
            }

            {
                dom::object attributesObject;
                if (primitiveObject["attributes"].get_object().get(attributesObject) != SUCCESS) {
                    return false;
                }

                // We iterate through the JSON object and write each key/pair value into the
                // attributes map. This is not filtered for actual values. TODO?
                for (auto field : attributesObject) {
                    std::string_view key = field.key;

                    auto& attribute = primitive.attributes[std::string { key }];
                    if (field.value.get_uint64().get(attribute) != SUCCESS) {
                        return false;
                    }
                }
            }

            {
                // Mode shall default to 4.
                uint64_t mode;
                if (primitiveObject["mode"].get_uint64().get(mode) != SUCCESS) {
                    mode = 4;
                }
                primitive.type = static_cast<PrimitiveType>(mode);
            }

            size_t indicesAccessor;
            if (primitiveObject["indices"].get_uint64().get(indicesAccessor) == SUCCESS) {
                primitive.indicesAccessor = indicesAccessor;
            }

            size_t materialIndex;
            if (primitiveObject["material"].get_uint64().get(materialIndex) == SUCCESS) {
                primitive.materialIndex = materialIndex;
            }

            primitives.emplace_back(std::move(primitive));
            return true;
        });

        if (!foundPrimitives && primitiveError != Error::None) {
            errorCode = Error::InvalidGltf;
            return false;
        }

        // name is optional.
        std::string_view name;
        if (meshObject["name"].get_string().get(name) == SUCCESS) {
            mesh.name = std::string { name };
        }

        parsedAsset->meshes.emplace_back(std::move(mesh));
        return true;
    });

    if (!foundMeshes && meshError != Error::None) {
        errorCode = meshError;
    }
    return errorCode;
}

fg::Error fg::glTF::parseNodes() {
    using namespace simdjson;
    auto [foundNodes, nodeError] = iterateOverArray(data->root, "nodes", [this](auto& value) -> bool {
        Node node = {};
        dom::object nodeObject;
        if (value.get_object().get(nodeObject) != SUCCESS) {
            return false;
        }

        size_t meshIndex;
        if (nodeObject["mesh"].get_uint64().get(meshIndex) == SUCCESS) {
            node.meshIndex = meshIndex;
        }

        auto [foundChildren, childrenError] = iterateOverArray(nodeObject, "children", [&node](auto& value) -> bool {
            size_t index;
            if (value.get_uint64().get(index) != SUCCESS) {
                return false;
            }

            node.children.emplace_back(index);
            return true;
        });

        if (foundChildren && childrenError != Error::None) {
            return false;
        }

        dom::array matrix;
        if (nodeObject["matrix"].get_array().get(matrix) == SUCCESS) {
            node.hasMatrix = true;
            auto i = 0U;
            for (auto num : matrix) {
                double val;
                if (num.get_double().get(val) != SUCCESS) {
                    node.hasMatrix = false;
                    break;
                }
                node.matrix[i] = static_cast<float>(val);
                ++i;
            }
        } else {
            // clang-format off
            node.matrix = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
            };
            // clang-format on
        }

        dom::array scale;
        if (nodeObject["scale"].get_array().get(scale) == SUCCESS) {
            auto i = 0U;
            for (auto num : scale) {
                double val;
                if (num.get_double().get(val) != SUCCESS) {
                    return false;
                }
                node.scale[i] = static_cast<float>(val);
                ++i;
            }
        } else {
            node.scale = {1.0f, 1.0f, 1.0f};
        }

        dom::array translation;
        if (nodeObject["translation"].get_array().get(translation) == SUCCESS) {
            auto i = 0U;
            for (auto num : translation) {
                double val;
                if (num.get_double().get(val) != SUCCESS) {
                    return false;
                }
                node.translation[i] = static_cast<float>(val);
                ++i;
            }
        } else {
            node.translation = {0.0f, 0.0f, 0.0f};
        }

        dom::array rotation;
        if (nodeObject["rotation"].get_array().get(rotation) == SUCCESS) {
            auto i = 0U;
            for (auto num : rotation) {
                double val;
                if (num.get_double().get(val) != SUCCESS) {
                    return false;
                }
                node.rotation[i] = static_cast<float>(val);
                ++i;
            }
        } else {
            node.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        }

        // name is optional.
        {
            std::string_view name;
            if (nodeObject["name"].get_string().get(name) == SUCCESS) {
                node.name = std::string { name };
            }
        }

        parsedAsset->nodes.emplace_back(std::move(node));
        return true;
    });

    if (!foundNodes && nodeError != Error::None) {
        errorCode = nodeError;
    }
    return errorCode;
}

fg::Error fg::glTF::parseScenes() {
    using namespace simdjson;

    size_t defaultScene;
    if (data->root["scene"].get_uint64().get(defaultScene) == SUCCESS) {
        parsedAsset->defaultScene = defaultScene;
    }

    auto [foundScenes, sceneError] = iterateOverArray(data->root, "scenes", [this](auto& value) -> bool {
        // The scene object can be completely empty
        Scene scene = {};
        dom::object sceneObject;
        if (value.get_object().get(sceneObject) != SUCCESS) {
            return false;
        }

        // name is optional.
        std::string_view name;
        if (sceneObject["name"].get_string().get(name) == SUCCESS) {
            scene.name = std::string { name };
        }

        // Parse the array of nodes.
        auto [foundNodes, nodeError] = iterateOverArray(sceneObject, "nodes", [&indices = scene.nodeIndices](auto& value) mutable -> bool {
            size_t index;
            if (value.get_uint64().get(index) != SUCCESS) {
                return false;
            }

            indices.push_back(index);
            return true;
        });

        if (!foundNodes && nodeError != Error::None) {
            errorCode = nodeError;
            return false;
        }

        parsedAsset->scenes.emplace_back(std::move(scene));
        return true;
    });

    // No error means the array has just not been found. However, it's optional, but the spec
    // still requires we parse everything else.
    if (!foundScenes && sceneError != Error::None) {
        errorCode = sceneError;
    }
    return errorCode;
}

fg::Error fg::glTF::parseTextureObject(void* object, std::string_view key, TextureInfo* info) noexcept {
    using namespace simdjson;
    auto& obj = *static_cast<dom::object*>(object);

    dom::object child;
    if (obj[key].get_object().get(child) != SUCCESS) {
        return Error::None;
    }

    size_t index;
    if (child["index"].get_uint64().get(index) == SUCCESS) {
        info->textureIndex = index;
    } else {
        return Error::InvalidGltf;
    }

    if (child["texCoord"].get_uint64().get(index) == SUCCESS) {
        info->texCoordIndex = index;
    } else {
        info->texCoordIndex = 0;
    }

    // scale only applies to normal textures.
    double scale = 1.0f;
    child["scale"].get_double().get(scale);
    info->scale = static_cast<float>(scale);

    dom::object extensions;
    if (child["extensions"].get_object().get(extensions) == SUCCESS) {
        dom::object textureTransform;
        if (extensions["KHR_texture_transform"].get_object().get(textureTransform) == SUCCESS && hasBit(options, Options::LoadTextureTransformExtension)) {
            if (textureTransform["texCoord"].get_uint64().get(index) == SUCCESS) {
                info->texCoordIndex = index;
            }

            double rotation = 0.0f;
            textureTransform["rotation"].get_double().get(rotation);
            info->rotation = static_cast<float>(rotation);

            dom::array array;
            if (textureTransform["offset"].get_array().get(array) == SUCCESS) {
                for (auto i = 0U; i < 2; ++i) {
                    double val;
                    if (array.at(i).get_double().get(val) != SUCCESS) {
                        return Error::InvalidGltf;
                    }
                    info->uvOffset[i] = static_cast<float>(val);
                }
            }

            if (textureTransform["scale"].get_array().get(array) == SUCCESS) {
                for (auto i = 0U; i < 2; ++i) {
                    double val;
                    if (array.at(i).get_double().get(val) != SUCCESS) {
                        return Error::InvalidGltf;
                    }
                    info->uvScale[i] = static_cast<float>(val);
                }
            }
        }
    }

    return Error::None;
}

fg::Error fg::glTF::parseTextures() {
    using namespace simdjson;
    auto [foundTextures, textureError] = iterateOverArray(data->root, "textures", [this](auto& value) mutable -> bool {
        Texture texture;
        dom::object textureObject;
        if (value.get_object().get(textureObject) != SUCCESS) {
            return false;
        }

        bool hasExtensions = false;
        dom::object extensionsObject;
        if (textureObject["extensions"].get_object().get(extensionsObject) == SUCCESS) {
            hasExtensions = true;
        }

        texture.imageIndex = std::numeric_limits<size_t>::max();
        if (textureObject["source"].get_uint64().get(texture.imageIndex) != SUCCESS && !hasExtensions) {
            // "The index of the image used by this texture. When undefined, an extension or other
            // mechanism SHOULD supply an alternate texture source, otherwise behavior is undefined."
            // => We'll have it be invalid.
            return false;
        }

        // If we have extensions, we'll use the normal "source" as the fallback and then parse
        // the extensions for any "source" field.
        if (hasExtensions) {
            if (texture.imageIndex != std::numeric_limits<size_t>::max()) {
                // If the source was specified we'll use that as a fallback.
                texture.fallbackImageIndex = texture.imageIndex;
            }
            if (!parseTextureExtensions(texture, extensionsObject, options)) {
                return false;
            }
        }

        // The index of the sampler used by this texture. When undefined, a sampler with
        // repeat wrapping and auto filtering SHOULD be used.
        size_t samplerIndex;
        if (textureObject["sampler"].get_uint64().get(samplerIndex) == SUCCESS) {
            texture.samplerIndex = samplerIndex;
        }

        // name is optional.
        std::string_view name;
        if (textureObject["name"].get_string().get(name) == SUCCESS) {
            texture.name = std::string { name };
        }

        parsedAsset->textures.emplace_back(std::move(texture));
        return true;
    });

    if (!foundTextures && textureError != Error::None) {
        errorCode = textureError;
    }
    return errorCode;
}

#pragma endregion

#pragma region JsonData
fg::JsonData::JsonData(uint8_t* bytes, size_t byteCount) noexcept {
    using namespace simdjson;
    data = std::make_unique<padded_string>(reinterpret_cast<char*>(bytes), byteCount);
}

fg::JsonData::JsonData(const fs::path& path) noexcept {
    using namespace simdjson;
    data = std::make_unique<padded_string>();
    if (padded_string::load(path.string()).get(*data) != SUCCESS) {
        // Not sure?
    }
}

fg::JsonData::~JsonData() = default;

const uint8_t* fg::JsonData::getData() const {
    return data->u8data();
}
#pragma endregion

#pragma region Parser
fg::Parser::Parser() noexcept {
    jsonParser = new simdjson::dom::parser();
}

fg::Parser::~Parser() {
    delete static_cast<simdjson::dom::parser*>(jsonParser);
}

fg::Error fg::Parser::getError() const {
    return errorCode;
}

std::unique_ptr<fg::glTF> fg::Parser::loadGLTF(JsonData* jsonData, fs::path directory, Options options) {
    using namespace simdjson;

    if (!fs::is_directory(directory)) {
        errorCode = Error::InvalidPath;
        return nullptr;
    }

    errorCode = Error::None;

    auto data = std::make_unique<ParserData>();
    auto* parser = static_cast<dom::parser*>(jsonParser);

    if (hasBit(options, Options::DontUseSIMD)) {
        simdjson::get_active_implementation() = simdjson::get_available_implementations()["fallback"];
    }

    if (parser->parse(*jsonData->data).get(data->root) != SUCCESS) {
        errorCode = Error::InvalidJson;
        return nullptr;
    }

    auto gltf = std::unique_ptr<glTF>(new glTF(std::move(data), std::move(directory), options));
    if (!hasBit(options, Options::DontRequireValidAssetMember) && !gltf->checkAssetField()) {
        errorCode = Error::InvalidOrMissingAssetField;
        return nullptr;
    }
    return gltf;
}

std::unique_ptr<fg::glTF> fg::Parser::loadGLTF(JsonData* jsonData, std::string_view directory, Options options) {
    fs::path parsed = { directory };
    if (parsed.empty() || !fs::is_directory(parsed)) {
        errorCode = Error::InvalidPath;
        return nullptr;
    }
    return loadGLTF(jsonData, parsed, options);
}

std::unique_ptr<fg::glTF> fg::Parser::loadBinaryGLTF(const fs::path& file, Options options) {
    using namespace simdjson;

    if (!fs::is_regular_file(file)) {
        errorCode = Error::InvalidPath;
        return nullptr;
    }

    errorCode = Error::None;

    std::ifstream gltfFile(file, std::ios::ate, std::ios::binary);
#if defined(DEBUG) || defined(_DEBUG)
    auto length = gltfFile.tellg();
    gltfFile.seekg(0, std::ifstream::beg);
#endif

    auto read = [&gltfFile](void* dst, size_t size) mutable {
        gltfFile.read(static_cast<char*>(dst), static_cast<int64_t>(size));
    };

    BinaryGltfHeader header = {};
    read(&header, sizeof header);
    if (header.magic != binaryGltfHeaderMagic || header.version != 2) {
        errorCode = Error::InvalidGLB;
        return nullptr;
    }
#if defined(DEBUG) || defined(_DEBUG)
    if (header.length != length) {
        errorCode = Error::InvalidGLB;
        return nullptr;
    }
#endif

    // The glTF 2 spec specifies that in GLB files the order of chunks is predefined. Specifically,
    //  1. JSON chunk
    //  2. BIN chunk (optional)
    BinaryGltfChunk jsonChunk = {};
    read(&jsonChunk, sizeof jsonChunk);
    if (jsonChunk.chunkType != 0x4E4F534A) {
        errorCode = Error::InvalidGLB;
        return nullptr;
    }

    std::vector<uint8_t> jsonData(jsonChunk.chunkLength + simdjson::SIMDJSON_PADDING);
    read(jsonData.data(), jsonChunk.chunkLength);
    // We set the padded region to 0 to avoid simdjson reading garbage
    std::memset(jsonData.data() + jsonChunk.chunkLength, 0, jsonData.size() - jsonChunk.chunkLength);

    auto data = std::make_unique<ParserData>();
    auto* parser = static_cast<dom::parser*>(jsonParser);

    if (hasBit(options, Options::DontUseSIMD)) {
        simdjson::get_active_implementation() = simdjson::get_available_implementations()["fallback"];
    }

    // The 'false' indicates that simdjson doesn't have to copy the data internally.
    if (parser->parse(jsonData.data(), jsonChunk.chunkLength, jsonData.size()).get(data->root) != SUCCESS) {
        errorCode = Error::InvalidJson;
        return nullptr;
    }

    // Is there enough room for another chunk header?
    if (header.length > (static_cast<uint32_t>(gltfFile.tellg()) + sizeof(BinaryGltfChunk))) {
        BinaryGltfChunk binaryChunk = {};
        read(&binaryChunk, sizeof binaryChunk);

        if (binaryChunk.chunkType != 0x004E4942) {
            errorCode = Error::InvalidGLB;
            return nullptr;
        }

        std::unique_ptr<glTF> gltf;
        if (hasBit(options, Options::LoadGLBBuffers)) {
            std::vector<uint8_t> binary(binaryChunk.chunkLength);
            read(binary.data(), binaryChunk.chunkLength);
            gltf = std::unique_ptr<glTF>(new glTF(std::move(data), file, std::move(binary), options));
        } else {
            gltf = std::unique_ptr<glTF>(new glTF(std::move(data), file, static_cast<size_t>(gltfFile.tellg()), binaryChunk.chunkLength, options));
        }

        if (!hasBit(options, Options::DontRequireValidAssetMember) && !gltf->checkAssetField()) {
            errorCode = Error::InvalidOrMissingAssetField;
            return nullptr;
        }
        return gltf;
    }

    // We're not loading the GLB buffer or there's none.
    auto gltf = std::unique_ptr<glTF>(new glTF(std::move(data), file, options));
    if (!hasBit(options, Options::DontRequireValidAssetMember) && !gltf->checkAssetField()) {
        errorCode = Error::InvalidOrMissingAssetField;
        return nullptr;
    }
    return gltf;
}

std::unique_ptr<fg::glTF> fg::Parser::loadBinaryGLTF(std::string_view file, Options options) {
    fs::path parsed = { file };
    if (parsed.empty() || !fs::is_regular_file(parsed)) {
        errorCode = Error::InvalidPath;
        return nullptr;
    }
    return loadBinaryGLTF(parsed, options);
}
#pragma endregion
