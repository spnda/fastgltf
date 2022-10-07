#include <array>
#include <fstream>
#include <functional>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5030) // attribute 'x' is not recognized
#pragma warning(disable : 4514) // unreferenced inline function has been removed
#endif

#include "simdjson.h"

#define STR(x) #x
#define STRX(x) STR(x)
// Make sure that SIMDJSON_TARGET_VERSION is equal to SIMDJSON_VERSION.
static_assert(std::string_view { SIMDJSON_TARGET_VERSION } == STRX(SIMDJSON_VERSION), "Outdated version of simdjson. Reconfigure project to update.");
#undef STRX
#undef STR

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
    [[nodiscard, gnu::always_inline]] bool parseTextureExtensions(Texture& texture, simdjson::dom::object& extensions, Extensions extensionFlags);

    [[nodiscard, gnu::always_inline]] Error getJsonArray(simdjson::dom::object& parent, std::string_view arrayName, simdjson::dom::array* array) noexcept;
}

std::tuple<bool, bool, size_t> fg::getImageIndexForExtension(simdjson::dom::object& object, std::string_view extension) {
    using namespace simdjson;

    // Both KHR_texture_basisu and MSFT_texture_dds allow specifying an alternative
    // image source index.
    dom::object sourceExtensionObject;
    if (object[extension].get_object().get(sourceExtensionObject) != SUCCESS) {
        return std::make_tuple(false, true, 0U);
    }

    // Check if the extension object provides a source index.
    uint64_t imageIndex;
    if (sourceExtensionObject["source"].get_uint64().get(imageIndex) != SUCCESS) {
        return std::make_tuple(true, false, 0U);
    }

    return std::make_tuple(false, false, imageIndex);
}

fg::Error fg::getJsonArray(simdjson::dom::object& parent, std::string_view arrayName, simdjson::dom::array* array) noexcept {
    using namespace simdjson;

    auto error = parent[arrayName].get_array().get(*array);
    if (error == NO_SUCH_FIELD) {
        return Error::MissingField;
    } else if (error == SUCCESS) {
        return Error::None;
    } else {
        return Error::InvalidGltf;
    }
}

bool fg::parseTextureExtensions(Texture& texture, simdjson::dom::object& extensions, Extensions extensionFlags) {
    if (hasBit(extensionFlags, Extensions::KHR_texture_basisu)) {
        auto [invalidGltf, extensionNotPresent, imageIndex] = getImageIndexForExtension(extensions, "KHR_texture_basisu");
        if (invalidGltf) {
            return false;
        }

        if (!extensionNotPresent) {
            texture.imageIndex = imageIndex;
            return true;
        }
    }

    if (hasBit(extensionFlags, Extensions::MSFT_texture_dds)) {
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
fg::glTF::glTF(std::unique_ptr<ParserData> data, fs::path directory, Options options, Extensions extensions) : data(std::move(data)), directory(std::move(directory)), options(options), extensions(extensions) {
    parsedAsset = std::make_unique<Asset>();
    glb = nullptr;
}

fg::glTF::glTF(std::unique_ptr<ParserData> data, fs::path file, std::vector<uint8_t>&& glbData, Options options, Extensions extensions) : data(std::move(data)), directory(file.parent_path()), options(options), extensions(extensions) {
    parsedAsset = std::make_unique<Asset>();
    glb = std::make_unique<GLBBuffer>();
    glb->buffer = std::move(glbData);
    glb->file = std::move(file);
}

fg::glTF::glTF(std::unique_ptr<ParserData> data, fs::path file, size_t fileOffset, size_t fileSize, Options options, Extensions extensions) : data(std::move(data)), directory(file.parent_path()), options(options), extensions(extensions) {
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

// clang-format off
constexpr std::array<std::pair<std::string_view, fastgltf::Extensions>, 3> extensionStrings = {{
    { "KHR_texture_basisu",     fastgltf::Extensions::KHR_texture_basisu },
    { "KHR_texture_transform",  fastgltf::Extensions::KHR_texture_transform },
    { "MSFT_texture_dds",       fastgltf::Extensions::MSFT_texture_dds },
}};
// clang-format on

bool fg::glTF::checkExtensions() {
    using namespace simdjson;

    dom::array extensionsRequired;
    if (data->root["extensionsRequired"].get_array().get(extensionsRequired) == SUCCESS) {
        for (auto extension : extensionsRequired) {
            std::string_view string;
            if (extension.get_string().get(string) != SUCCESS) {
                errorCode = Error::InvalidGltf;
                return false;
            }

            // Check if the extension is known and listed in the parser.
            bool known = false;
            bool listed = false;
            for (auto& [extensionString, extensionEnum] : extensionStrings) {
                if (!known) {
                    known = extensionString == string;
                }
                if (!listed) {
                    listed = hasBit(extensions, extensionEnum);
                }
                if (known && listed)
                    break;
            }
            if (!known) {
                errorCode = Error::UnsupportedExtensions;
                return false;
            }
            if (!listed) {
                errorCode = Error::MissingExtensions;
                return false;
            }
        }
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

fg::Error fg::glTF::returnError(Error error) noexcept {
    errorCode = error;
    return error;
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

fg::Error fg::glTF::parseAll() {
    parseAccessors();
    parseBuffers();
    parseBufferViews();
    parseImages();
    parseMaterials();
    parseMeshes();
    parseNodes();
    parseSamplers();
    parseScenes();
    parseSkins();
    parseTextures();
    return errorCode;
}

fg::Error fg::glTF::parseAccessors() {
    using namespace simdjson;
    dom::array accessors;
    auto accessorError = getJsonArray(data->root, "accessors", &accessors);
    if (accessorError == Error::MissingField) {
        return Error::None;
    } else if (accessorError != Error::None) {
        return returnError(accessorError);
    }

    parsedAsset->accessors.reserve(accessors.size());
    for (auto accessorValue : accessors) {
        // Required fields: "componentType", "count"
        Accessor accessor = {};
        dom::object accessorObject;
        if (accessorValue.get_object().get(accessorObject) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        }

        uint64_t componentType;
        if (accessorObject["componentType"].get_uint64().get(componentType) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        } else {
            accessor.componentType = getComponentType(static_cast<std::underlying_type_t<ComponentType>>(componentType));
            if (accessor.componentType == ComponentType::Double && !hasBit(options, Options::AllowDouble)) {
                return returnError(Error::InvalidGltf);
            }
        }

        std::string_view accessorType;
        if (accessorObject["type"].get_string().get(accessorType) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        } else {
            accessor.type = getAccessorType(accessorType);
        }

        uint64_t accessorCount;
        if (accessorObject["count"].get_uint64().get(accessorCount) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        } else {
            accessor.count = static_cast<size_t>(accessorCount);
        }

        uint64_t bufferView;
        if (accessorObject["bufferView"].get_uint64().get(bufferView) == SUCCESS) {
            accessor.bufferViewIndex = static_cast<size_t>(bufferView);
        }

        // byteOffset is optional, but defaults to 0
        uint64_t byteOffset;
        if (accessorObject["byteOffset"].get_uint64().get(byteOffset) != SUCCESS) {
            accessor.byteOffset = 0U;
        } else {
            accessor.byteOffset = static_cast<size_t>(byteOffset);
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
    }

    return errorCode;
}

fg::Error fg::glTF::parseAnimations() {
    using namespace simdjson;
    dom::array animations;
    auto animationError = getJsonArray(data->root, "animations", &animations);
    if (animationError == Error::MissingField) {
        return Error::None;
    } else if (animationError != Error::None) {
        return returnError(animationError);
    }

    parsedAsset->animations.reserve(animations.size());
    for (auto animationValue : animations) {
        dom::object animationObject;
        Animation animation = {};
        if (animationValue.get_object().get(animationObject) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        }

        dom::array channels;
        auto channelError = getJsonArray(animationObject, "channels", &channels);
        if (channelError != Error::None) {
            return returnError(Error::InvalidGltf);
        }

        animation.channels.reserve(channels.size());
        for (auto channelValue : channels) {
            dom::object channelObject;
            AnimationChannel channel = {};
            if (channelValue.get_object().get(channelObject) != SUCCESS) {
                return returnError(Error::InvalidGltf);
            }

            uint64_t sampler;
            if (channelObject["sampler"].get_uint64().get(sampler) != SUCCESS) {
                return returnError(Error::InvalidGltf);
            }
            channel.samplerIndex = static_cast<size_t>(sampler);

            dom::object targetObject;
            if (channelObject["target"].get_object().get(targetObject) != SUCCESS) {
                return returnError(Error::InvalidGltf);
            } else {
                uint64_t node;
                if (targetObject["node"].get_uint64().get(node) != SUCCESS) {
                    // We don't support any extensions for animations, so it is required.
                    return returnError(Error::InvalidGltf);
                }
                channel.nodeIndex = static_cast<size_t>(node);

                std::string_view path;
                if (targetObject["path"].get_string().get(path) != SUCCESS) {
                    return returnError(Error::InvalidGltf);
                }

                if (path == "translation") {
                    channel.path = AnimationPath::Translation;
                } else if (path == "rotation") {
                    channel.path = AnimationPath::Rotation;
                } else if (path == "scale") {
                    channel.path = AnimationPath::Scale;
                } else if (path == "weights") {
                    channel.path = AnimationPath::Weights;
                }
            }

            animation.channels.emplace_back(channel);
        }

        dom::array samplers;
        auto samplerError = getJsonArray(animationObject, "samplers", &samplers);
        if (samplerError != Error::None) {
            return returnError(Error::InvalidGltf);
        }

        animation.samplers.reserve(samplers.size());
        for (auto samplerValue : samplers) {
            dom::object samplerObject;
            AnimationSampler sampler = {};
            if (samplerValue.get_object().get(samplerObject) != SUCCESS) {
                return returnError(Error::InvalidGltf);
            }

            uint64_t input;
            if (samplerObject["input"].get_uint64().get(input) != SUCCESS) {
                return returnError(Error::InvalidGltf);
            }
            sampler.inputAccessor = static_cast<size_t>(input);

            uint64_t output;
            if (samplerObject["output"].get_uint64().get(output) != SUCCESS) {
                return returnError(Error::InvalidGltf);
            }
            sampler.outputAccessor = static_cast<size_t>(output);

            std::string_view interpolation;
            if (samplerObject["interpolation"].get_string().get(interpolation) != SUCCESS) {
                sampler.interpolation = AnimationInterpolation::Linear;
            }

            if (interpolation == "LINEAR") {
                sampler.interpolation = AnimationInterpolation::Linear;
            } else if (interpolation == "STEP") {
                sampler.interpolation = AnimationInterpolation::Step;
            } else if (interpolation == "CUBICSPLINE") {
                sampler.interpolation = AnimationInterpolation::CubicSpline;
            } else {
                return returnError(Error::InvalidGltf);
            }

            animation.samplers.emplace_back(sampler);
        }

        parsedAsset->animations.emplace_back(std::move(animation));
    }

    return errorCode;
}

fg::Error fg::glTF::parseBuffers() {
    using namespace simdjson;
    dom::array buffers;
    auto bufferError = getJsonArray(data->root, "buffers", &buffers);
    if (bufferError == Error::MissingField) {
        return Error::None;
    } else if (bufferError != Error::None) {
        return returnError(bufferError);
    }

    parsedAsset->buffers.reserve(buffers.size());
    size_t bufferIndex = 0;
    for (auto bufferValue : buffers) {
        // Required fields: "byteLength"
        Buffer buffer = {};
        dom::object bufferObject;
        if (bufferValue.get_object().get(bufferObject) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        }

        uint64_t byteLength;
        if (bufferObject["byteLength"].get_uint64().get(byteLength) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        } else {
            buffer.byteLength = static_cast<size_t>(byteLength);
        }

        // When parsing GLB, there's a buffer object that will point to the BUF chunk in the
        // file. Otherwise, data must be specified in the "uri" field.
        std::string_view uri;
        if (bufferObject["uri"].get_string().get(uri) == SUCCESS) {
            auto [error, source, location] = decodeUri(uri);
            if (error != Error::None)
                return returnError(error);

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
            // All other buffers have to contain an uri field.
            return returnError(Error::InvalidGltf);
        }

        if (buffer.location == DataLocation::None) {
            return returnError(Error::InvalidGltf);
        }

        // name is optional.
        std::string_view name;
        if (bufferObject["name"].get_string().get(name) == SUCCESS) {
            buffer.name = std::string { name };
        }

        ++bufferIndex;
        parsedAsset->buffers.emplace_back(std::move(buffer));
    }

    return errorCode;
}

fg::Error fg::glTF::parseBufferViews() {
    using namespace simdjson;
    dom::array bufferViews;
    auto bufferViewError = getJsonArray(data->root, "bufferViews", &bufferViews);
    if (bufferViewError == Error::MissingField) {
        return Error::None;
    } else if (bufferViewError != Error::None) {
        return returnError(bufferViewError);
    }

    parsedAsset->bufferViews.reserve(bufferViews.size());
    for (auto bufferViewValue : bufferViews) {
        // Required fields: "bufferIndex", "byteLength"
        BufferView view = {};
        dom::object bufferViewObject;
        if (bufferViewValue.get_object().get(bufferViewObject) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        }

        // Required with normal glTF, not necessary with GLB files.
        uint64_t bufferIndex;
        if (bufferViewObject["buffer"].get_uint64().get(bufferIndex) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        } else {
            view.bufferIndex = static_cast<size_t>(bufferIndex);
        }

        uint64_t byteLength;
        if (bufferViewObject["byteLength"].get_uint64().get(byteLength) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        } else {
            view.byteLength = static_cast<size_t>(byteLength);
        }

        // byteOffset is optional, but defaults to 0
        uint64_t byteOffset;
        if (bufferViewObject["byteOffset"].get_uint64().get(byteOffset) != SUCCESS) {
            view.byteOffset = 0;
        } else {
            view.byteOffset = static_cast<size_t>(byteOffset);
        }

        uint64_t byteStride;
        if (bufferViewObject["byteStride"].get_uint64().get(byteStride) == SUCCESS) {
            view.byteStride = static_cast<size_t>(byteStride);
        }

        // target is optional
        uint64_t target;
        if (bufferViewObject["target"].get_uint64().get(target) == SUCCESS) {
            view.target = static_cast<BufferTarget>(target);
        }

        // name is optional.
        std::string_view name;
        if (bufferViewObject["name"].get_string().get(name) == SUCCESS) {
            view.name = std::string { name };
        }

        parsedAsset->bufferViews.emplace_back(std::move(view));
    }

    return errorCode;
}

fg::Error fg::glTF::parseImages() {
    using namespace simdjson;
    dom::array images;
    auto imageError = getJsonArray(data->root, "images", &images);
    if (imageError == Error::MissingField) {
        return Error::None;
    } else if (imageError != Error::None) {
        return returnError(imageError);
    }

    parsedAsset->images.reserve(images.size());
    for (auto imageValue : images) {
        Image image;
        dom::object imageObject;
        if (imageValue.get_object().get(imageObject) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        }

        std::string_view uri;
        if (imageObject["uri"].get_string().get(uri) == SUCCESS) {
            if (imageObject["bufferView"].error() == SUCCESS) {
                // If uri is declared, bufferView cannot be declared.
                return returnError(Error::InvalidGltf);
            }
            auto [error, source, location] = decodeUri(uri);
            if (error != Error::None)
                return returnError(error);

            image.data = source;
            image.location = location;

            std::string_view mimeType;
            if (imageObject["mimeType"].get_string().get(mimeType) == SUCCESS) {
                image.data.mimeType = getMimeTypeFromString(mimeType);
            }
        }

        uint64_t bufferViewIndex;
        if (imageObject["bufferView"].get_uint64().get(bufferViewIndex) == SUCCESS) {
            std::string_view mimeType;
            if (imageObject["mimeType"].get_string().get(mimeType) != SUCCESS) {
                // If bufferView is defined, mimeType needs to also be defined.
                return returnError(Error::InvalidGltf);
            }

            image.data.bufferViewIndex = static_cast<size_t>(bufferViewIndex);
            image.data.mimeType = getMimeTypeFromString(mimeType);
        }

        if (image.location == DataLocation::None) {
            return returnError(Error::InvalidGltf);
        }

        // name is optional.
        std::string_view name;
        if (imageObject["name"].get_string().get(name) == SUCCESS) {
            image.name = std::string{name};
        }

        parsedAsset->images.emplace_back(std::move(image));
    }

    return errorCode;
}

fg::Error fg::glTF::parseMaterials() {
    using namespace simdjson;
    dom::array materials;
    auto materialError = getJsonArray(data->root, "materials", &materials);
    if (materialError == Error::MissingField) {
        return Error::None;
    } else if (materialError != Error::None) {
        return returnError(materialError);
    }

    parsedAsset->materials.reserve(materials.size());
    for (auto materialValue : materials) {
        dom::object materialObject;
        if (materialValue.get_object().get(materialObject) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        }
        Material material = {};

        dom::array emissiveFactor;
        if (materialObject["emissiveFactor"].get_array().get(emissiveFactor) == SUCCESS) {
            if (emissiveFactor.size() != 3)
                return returnError(Error::InvalidGltf);
            for (auto i = 0U; i < 3; ++i) {
                double val;
                if (emissiveFactor.at(i).get_double().get(val) != SUCCESS)
                    return returnError(Error::InvalidGltf);
                material.emissiveFactor[i] = static_cast<float>(val);
            }
        } else {
            material.emissiveFactor = { 0, 0, 0 };
        }

        {
            TextureInfo normalTexture = {};
            auto error = parseTextureObject(&materialObject, "normalTexture", &normalTexture);
            if (error != Error::None) {
                return returnError(error);
            }
            material.normalTexture = normalTexture;
        }
        {
            TextureInfo occlusionTexture = {};
            auto error = parseTextureObject(&materialObject, "occlusionTexture", &occlusionTexture);
            if (error != Error::None) {
                return returnError(error);
            }
            material.occlusionTexture = occlusionTexture;
        }
        {
            TextureInfo emissiveTexture = {};
            auto error = parseTextureObject(&materialObject, "emissiveTexture", &emissiveTexture);
            if (error != Error::None) {
                return returnError(error);
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
                        return returnError(Error::InvalidGltf);
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
                    return returnError(error);
                }
                pbr.baseColorTexture = baseColorTexture;
            }
            {
                TextureInfo metallicRoughnessTexture = {};
                auto error = parseTextureObject(&pbrMetallicRoughness, "metallicRoughnessTexture", &metallicRoughnessTexture);
                if (error != Error::None) {
                    return returnError(error);
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
    }

    return errorCode;
}

fg::Error fg::glTF::parseMeshes() {
    using namespace simdjson;
    dom::array meshes;
    auto meshError = getJsonArray(data->root, "meshes", &meshes);
    if (meshError == Error::MissingField) {
        return Error::None;
    } else if (meshError != Error::None) {
        return returnError(meshError);
    }

    parsedAsset->meshes.reserve(meshes.size());
    for (auto meshValue : meshes) {
        // Required fields: "primitives"
        dom::object meshObject;
        if (meshValue.get_object().get(meshObject) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        }
        Mesh mesh = {};

        dom::array primitives;
        meshError = getJsonArray(meshObject, "primitives", &primitives);
        if (meshError == Error::MissingField) {
            continue;
        } else if (meshError != Error::None) {
            return returnError(meshError);
        } else {
            mesh.primitives.reserve(primitives.size());
            for (auto primitiveValue : primitives) {
                // Required fields: "attributes"
                Primitive primitive = {};
                dom::object primitiveObject;
                if (primitiveValue.get_object().get(primitiveObject) != SUCCESS) {
                    return returnError(Error::InvalidGltf);
                }

                {
                    dom::object attributesObject;
                    if (primitiveObject["attributes"].get_object().get(attributesObject) != SUCCESS) {
                        return returnError(Error::InvalidGltf);
                    }

                    // We iterate through the JSON object and write each key/pair value into the
                    // attributes map. This is not filtered for actual values. TODO?
                    for (auto field : attributesObject) {
                        std::string_view key = field.key;

                        uint64_t attributeIndex;
                        if (field.value.get_uint64().get(attributeIndex) != SUCCESS) {
                            return returnError(Error::InvalidGltf);
                        } else {
                            primitive.attributes[std::string { key }] = static_cast<size_t>(attributeIndex);
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

                uint64_t indicesAccessor;
                if (primitiveObject["indices"].get_uint64().get(indicesAccessor) == SUCCESS) {
                    primitive.indicesAccessor = static_cast<size_t>(indicesAccessor);
                }

                uint64_t materialIndex;
                if (primitiveObject["material"].get_uint64().get(materialIndex) == SUCCESS) {
                    primitive.materialIndex = static_cast<size_t>(materialIndex);
                }

                mesh.primitives.emplace_back(std::move(primitive));
            }
        }

        // name is optional.
        std::string_view name;
        if (meshObject["name"].get_string().get(name) == SUCCESS) {
            mesh.name = std::string { name };
        }

        parsedAsset->meshes.emplace_back(std::move(mesh));
    }

    return errorCode;
}

fg::Error fg::glTF::parseNodes() {
    using namespace simdjson;
    dom::array nodes;
    auto nodeError = getJsonArray(data->root, "nodes", &nodes);
    if (nodeError == Error::MissingField) {
        return Error::None;
    } else if (nodeError != Error::None) {
        return returnError(nodeError);
    }

    parsedAsset->nodes.reserve(nodes.size());
    for (auto nodeValue : nodes) {
        Node node = {};
        dom::object nodeObject;
        if (nodeValue.get_object().get(nodeObject) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        }

        uint64_t index;
        if (nodeObject["mesh"].get_uint64().get(index) == SUCCESS) {
            node.meshIndex = static_cast<size_t>(index);
        }
        if (nodeObject["skin"].get_uint64().get(index) == SUCCESS) {
            node.skinIndex = static_cast<size_t>(index);
        }

        {
            dom::array children;
            auto childError = getJsonArray(nodeObject, "children", &children);
            if (childError == Error::None) {
                node.children.reserve(children.size());
                for (auto childValue : children) {
                    if (childValue.get_uint64().get(index) != SUCCESS) {
                        return returnError(Error::InvalidGltf);
                    }

                    node.children.emplace_back(static_cast<size_t>(index));
                }
            } else if (childError != Error::None && childError != Error::MissingField) {
                return returnError(childError);
            }
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
                    return returnError(Error::InvalidGltf);
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
                    return returnError(Error::InvalidGltf);
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
                    return returnError(Error::InvalidGltf);
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
    }

    return errorCode;
}

fg::Error fg::glTF::parseSamplers() {
    using namespace simdjson;
    dom::array samplers;
    auto samplerError = getJsonArray(data->root, "samplers", &samplers);
    if (samplerError == Error::MissingField) {
        return Error::None;
    } else if (samplerError != Error::None) {
        return returnError(samplerError);
    }

    uint64_t number;
    parsedAsset->samplers.reserve(samplers.size());
    for (auto samplerValue : samplers) {
        Sampler sampler = {};
        dom::object samplerObject;
        if (samplerValue.get_object().get(samplerObject) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        }

        // name is optional.
        std::string_view name;
        if (sceneObject["name"].get_string().get(name) == SUCCESS) {
            scene.name = std::string { name };
        }

        if (sceneObject["magFilter"].get_uint64().get(number) == SUCCESS) {
            sampler.magFilter = static_cast<Filter>(number);
        }
        if (sceneObject["minFilter"].get_uint64().get(number) == SUCCESS) {
            sampler.minFilter = static_cast<Filter>(number);
        }

        if (sceneObject["wrapS"].get_uint64().get(number) == SUCCESS) {
            sampler.wrapS = static_cast<Wrap>(number);
        } else {
            sampler.wrapT = Wrap::Repeat;
        }
        if (sceneObject["wrapT"].get_uint64().get(number) == SUCCESS) {
            sampler.wrapT = static_cast<Wrap>(number);
        } else {
            sampler.wrapT = Wrap::Repeat;
        }

        parsedAsset->samplers.emplace_back(std::move(sampler));
    }

    return errorCode;
}

fg::Error fg::glTF::parseScenes() {
    using namespace simdjson;
    dom::array scenes;
    auto sceneError = getJsonArray(data->root, "scenes", &scenes);
    if (sceneError == Error::MissingField) {
        return Error::None;
    } else if (sceneError != Error::None) {
        return returnError(sceneError);
    }

    uint64_t defaultScene;
    if (data->root["scene"].get_uint64().get(defaultScene) == SUCCESS) {
        parsedAsset->defaultScene = static_cast<size_t>(defaultScene);
    }

    parsedAsset->scenes.reserve(scenes.size());
    for (auto sceneValue : scenes) {
        // The scene object can be completely empty
        Scene scene = {};
        dom::object sceneObject;
        if (sceneValue.get_object().get(sceneObject) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        }

        // name is optional.
        std::string_view name;
        if (sceneObject["name"].get_string().get(name) == SUCCESS) {
            scene.name = std::string { name };
        }

        // Parse the array of nodes.
        dom::array nodes;
        auto nodeError = getJsonArray(sceneObject, "nodes", &nodes);
        if (nodeError == Error::None) {
            scene.nodeIndices.reserve(nodes.size());
            for (auto nodeValue : nodes) {
                uint64_t index;
                if (nodeValue.get_uint64().get(index) != SUCCESS) {
                    return returnError(Error::InvalidGltf);
                }

                scene.nodeIndices.emplace_back(static_cast<size_t>(index));
            }

            parsedAsset->scenes.emplace_back(std::move(scene));
        } else if (nodeError != Error::None && nodeError != Error::MissingField) {
            return returnError(nodeError);
        }
    }

    return errorCode;
}

fg::Error fg::glTF::parseSkins() {
    using namespace simdjson;
    dom::array skins;
    auto skinsError = getJsonArray(data->root, "skins", &skins);
    if (skinsError == Error::MissingField) {
        return errorCode;
    } else if (skinsError != Error::None) {
        return returnError(skinsError);
    }

    parsedAsset->skins.reserve(skins.size());
    for (auto skinValue : skins) {
        Skin skin = {};
        dom::object skinObject;
        if (skinValue.get_object().get(skinObject) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        }

        uint64_t index;
        if (skinObject["inverseBindMatrices"].get_uint64().get(index) == SUCCESS) {
            skin.inverseBindMatrices = static_cast<size_t>(index);
        }
        if (skinObject["skeleton"].get_uint64().get(index) == SUCCESS) {
            skin.skeleton = static_cast<size_t>(index);
        }

        dom::array jointsArray;
        if (skinObject["joints"].get_array().get(jointsArray) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        }
        skin.joints.reserve(jointsArray.size());
        for (auto jointValue : jointsArray) {
            if (jointValue.get_uint64().get(index) != SUCCESS) {
                return returnError(Error::InvalidGltf);
            }
            skin.joints.emplace_back(index);
        }

        // name is optional.
        std::string_view name;
        if (skinObject["name"].get_string().get(name) == SUCCESS) {
            skin.name = std::string { name };
        }
        parsedAsset->skins.emplace_back(std::move(skin));
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

    uint64_t index;
    if (child["index"].get_uint64().get(index) == SUCCESS) {
        info->textureIndex = static_cast<size_t>(index);
    } else {
        return Error::InvalidGltf;
    }

    if (child["texCoord"].get_uint64().get(index) == SUCCESS) {
        info->texCoordIndex = static_cast<size_t>(index);
    } else {
        info->texCoordIndex = 0;
    }

    // scale only applies to normal textures.
    double scale = 1.0f;
    child["scale"].get_double().get(scale);
    info->scale = static_cast<float>(scale);

    if (!hasBit(this->extensions, Extensions::KHR_texture_transform)) {
        info->rotation = 0.0f;
        info->uvOffset = {0.0f, 0.0f};
        info->uvScale = {1.0f, 1.0f};
        return Error::None;
    }

    dom::object extensionsObject;
    if (child["extensions"].get_object().get(extensionsObject) == SUCCESS) {
        dom::object textureTransform;
        if (extensionsObject["KHR_texture_transform"].get_object().get(textureTransform) == SUCCESS) {
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
    dom::array textures;
    auto textureError = getJsonArray(data->root, "textures", &textures);
    if (textureError == Error::MissingField) {
        return Error::None;
    } else if (textureError != Error::None) {
        return returnError(textureError);
    }

    parsedAsset->textures.reserve(textures.size());
    for (auto textureValue : textures) {
        Texture texture;
        dom::object textureObject;
        if (textureValue.get_object().get(textureObject) != SUCCESS) {
            return returnError(Error::InvalidGltf);
        }

        uint64_t sourceIndex;
        if (textureObject["source"].get_uint64().get(sourceIndex) == SUCCESS) {
            texture.imageIndex = static_cast<size_t>(sourceIndex);
        }

        bool hasExtensions = false;
        dom::object extensionsObject;
        if (textureObject["extensions"].get_object().get(extensionsObject) == SUCCESS) {
            hasExtensions = true;
        }

        // If we have extensions, we'll use the normal "source" as the fallback and then parse
        // the extensions for any "source" field.
        if (hasExtensions) {
            if (texture.imageIndex.has_value()) {
                // If the source was specified we'll use that as a fallback.
                texture.fallbackImageIndex = texture.imageIndex;
            }
            if (!parseTextureExtensions(texture, extensionsObject, extensions)) {
                return returnError(Error::InvalidGltf);
            }
        }

        // The index of the sampler used by this texture. When undefined, a sampler with
        // repeat wrapping and auto filtering SHOULD be used.
        uint64_t samplerIndex;
        if (textureObject["sampler"].get_uint64().get(samplerIndex) == SUCCESS) {
            texture.samplerIndex = static_cast<size_t>(samplerIndex);
        }

        // name is optional.
        std::string_view name;
        if (textureObject["name"].get_string().get(name) == SUCCESS) {
            texture.name = std::string { name };
        }

        parsedAsset->textures.emplace_back(std::move(texture));
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
fg::Parser::Parser(Extensions extensionsToLoad) noexcept : extensions(extensionsToLoad) {
    jsonParser = std::make_unique<simdjson::dom::parser>();
}

fg::Parser::~Parser() = default;

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

    if (hasBit(options, Options::DontUseSIMD)) {
        simdjson::get_active_implementation() = simdjson::get_available_implementations()["fallback"];
    }

    auto data = std::make_unique<ParserData>();
    if (jsonParser->parse(*jsonData->data).get(data->root) != SUCCESS) {
        errorCode = Error::InvalidJson;
        return nullptr;
    }

    auto gltf = std::unique_ptr<glTF>(new glTF(std::move(data), std::move(directory), options, extensions));
    if (!hasBit(options, Options::DontRequireValidAssetMember) && !gltf->checkAssetField()) {
        errorCode = Error::InvalidOrMissingAssetField;
        return nullptr;
    }
    if (!gltf->checkExtensions()) {
        errorCode = gltf->errorCode;
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

#if defined(DEBUG) || defined(_DEBUG)
    std::ifstream gltfFile(file, std::ios::ate, std::ios::binary);
    auto length = gltfFile.tellg();
    gltfFile.seekg(0, std::ifstream::beg);
#else
    std::ifstream gltfFile(file, std::ios::binary);
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

    if (hasBit(options, Options::DontUseSIMD)) {
        simdjson::get_active_implementation() = simdjson::get_available_implementations()["fallback"];
    }

    // The 'false' indicates that simdjson doesn't have to copy the data internally.
    auto data = std::make_unique<ParserData>();
    if (jsonParser->parse(jsonData.data(), jsonChunk.chunkLength, false).get(data->root) != SUCCESS) {
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
            gltf = std::unique_ptr<glTF>(new glTF(std::move(data), file, std::move(binary), options, extensions));
        } else {
            gltf = std::unique_ptr<glTF>(new glTF(std::move(data), file, static_cast<size_t>(gltfFile.tellg()), binaryChunk.chunkLength, options, extensions));
        }

        if (!hasBit(options, Options::DontRequireValidAssetMember) && !gltf->checkAssetField()) {
            errorCode = Error::InvalidOrMissingAssetField;
            return nullptr;
        }
        if (!gltf->checkExtensions()) {
            errorCode = gltf->errorCode;
            return nullptr;
        }
        return gltf;
    }

    // We're not loading the GLB buffer or there's none.
    auto gltf = std::unique_ptr<glTF>(new glTF(std::move(data), file, options, extensions));
    if (!hasBit(options, Options::DontRequireValidAssetMember) && !gltf->checkAssetField()) {
        errorCode = Error::InvalidOrMissingAssetField;
        return nullptr;
    }
    if (!gltf->checkExtensions()) {
        errorCode = gltf->errorCode;
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

#ifdef _MSC_VER
#pragma warning(pop)
#endif
