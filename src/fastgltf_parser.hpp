/*
 * Copyright (C) 2022 - 2023 spnda
 * This file is part of fastgltf <https://github.com/spnda/fastgltf>.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "fastgltf_types.hpp"
#include "fastgltf_util.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5030) // attribute 'x' is not recognized
#pragma warning(disable : 4514) // unreferenced inline function has been removed
#endif

// fwd
namespace simdjson::dom {
    class array;
    class object;
    class parser;
}

namespace fastgltf {
    struct BinaryGltfChunk;
    class GltfDataBuffer;
    struct ParserData;

    enum class Error : uint64_t {
        None = 0,
        InvalidPath = 1,
        // One or more extensions were not marked as supported by the client application but are
        // required by the glTF.
        MissingExtensions = 2,
        // A required extensions is not supported by fastgltf.
        UnknownRequiredExtension = 3,
        InvalidJson = 4,
        InvalidGltf = 5,
        InvalidOrMissingAssetField = 6,
        InvalidGLB = 6,
        MissingField = 7,
        MissingExternalBuffer = 8,
        UnsupportedVersion = 9,
    };

    // clang-format off
    enum class Extensions : uint64_t {
        None = 0,

        // See https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_transform/README.md
        KHR_texture_transform = 1 << 1,

        // See https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_basisu/README.md
        KHR_texture_basisu = 1 << 2,

        // See https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/MSFT_texture_dds/README.md
        MSFT_texture_dds = 1 << 3,

        // See https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization/README.md
        KHR_mesh_quantization = 1 << 4,

        // See https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/EXT_meshopt_compression/README.md
        EXT_meshopt_compression = 1 << 5,

        // See https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md
        KHR_lights_punctual = 1 << 6,

        // See https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/EXT_mesh_gpu_instancing/README.md
        EXT_mesh_gpu_instancing = 1 << 7,

        // See https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/EXT_texture_webp/README.md
        EXT_texture_webp = 1 << 8,
    };
    // clang-format on

    constexpr Extensions operator&(Extensions a, Extensions b) noexcept {
        return static_cast<Extensions>(to_underlying(a) & to_underlying(b));
    }

    constexpr Extensions operator|(Extensions a, Extensions b) noexcept {
        return static_cast<Extensions>(to_underlying(a) | to_underlying(b));
    }

    // clang-format off
    enum class Options : uint64_t {
        None                            = 0,
        /**
         * This allows 5130 as an accessor component type. 5130 is the OpenGL constant GL_DOUBLE,
         * which is by default not listed as an allowed component type in the glTF spec.
         *
         * The glTF normally only allows these component types:
         * https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#accessor-data-types
         */
        AllowDouble                     = 1 << 0,

        /**
         * This skips validating the asset field, as it is usually there and not used anyway.
         */
        DontRequireValidAssetMember     = 1 << 1,

        /**
         * Loads all the GLB buffers into CPU memory. If disabled, fastgltf will only provide
         * a byte offset and length into the GLB file, which can be useful when using APIs like
         * DirectStorage or Metal IO.
         */
        LoadGLBBuffers                  = 1 << 3,

        /**
         * Loads all external buffers into CPU memory. If disabled, fastgltf will only provide
         * a full file path to the file holding the buffer, which can be useful when using APIs
         * like DirectStorage or Metal IO.
         */
        LoadExternalBuffers             = 1 << 4,

        /**
         * This option makes fastgltf automatically decompose the transformation matrices of nodes
         * into the translation, rotation, and scale components. This might be useful to have only
         * TRS components, instead of matrices or TRS, which should simplify working with nodes,
         * especially with animations.
         */
        DecomposeNodeMatrices           = 1 << 5,

        /**
         * This option makes fastgltf minimise the JSON file before parsing. In most cases,
         * minimising it beforehand actually reduces the time spent. However, there are plenty
         * of cases where this option slows down parsing drastically, which from my testing seem
         * to all be glTFs which contain embedded buffers and/or are already minimised. Note that
         * fastgltf only minimises the string if the data was loaded using GltfDataBuffer::loadFromFile
         * or GltfDataBuffer::copyBytes, and that the bytes will also be overwritten.
         */
        MinimiseJsonBeforeParsing       = 1 << 6,
    };
    // clang-format on

    constexpr Options operator&(Options a, Options b) noexcept {
        return static_cast<Options>(to_underlying(a) & to_underlying(b));
    }

    constexpr Options operator|(Options a, Options b) noexcept {
        return static_cast<Options>(to_underlying(a) | to_underlying(b));
    }

    // clang-format off
    enum class Category : uint32_t {
        None        = 0,
        Buffers     = 1 <<  0,
        BufferViews = 1 <<  1 | Buffers,
        Accessors   = 1 <<  2 | BufferViews,
        Images      = 1 <<  3 | BufferViews,
        Samplers    = 1 <<  4,
        Textures    = 1 <<  5 | Images | Samplers,
        Animations  = 1 <<  6 | Accessors,
        Cameras     = 1 <<  7,
        Materials   = 1 <<  8 | Textures,
        Meshes      = 1 <<  9 | Accessors | Materials,
        Skins       = 1 << 10 | Accessors | (1 << 11), // Also depends on Nodes
        Nodes       = 1 << 11 | Cameras | Meshes | Skins,
        Scenes      = 1 << 12 | Nodes,
        Asset       = 1 << 13,

        All = Asset | Scenes | Animations,
    };
    // clang-format on

    constexpr Category operator&(Category a, Category b) noexcept {
        return static_cast<Category>(to_underlying(a) & to_underlying(b));
    }

    constexpr Category operator|(Category a, Category b) noexcept {
        return static_cast<Category>(to_underlying(a) | to_underlying(b));
    }

    constexpr Category operator|=(Category a, Category b) noexcept {
        return static_cast<Category>(to_underlying(a) | to_underlying(b));
    }

    // String representations of glTF 2.0 extension identifiers.
    namespace extensions {
        constexpr std::string_view EXT_mesh_gpu_instancing = "EXT_mesh_gpu_instancing";
        constexpr std::string_view EXT_meshopt_compression = "EXT_meshopt_compression";
        constexpr std::string_view EXT_texture_webp = "EXT_texture_webp";
        constexpr std::string_view KHR_lights_punctual = "KHR_lights_punctual";
        constexpr std::string_view KHR_mesh_quantization = "KHR_mesh_quantization";
        constexpr std::string_view KHR_texture_basisu = "KHR_texture_basisu";
        constexpr std::string_view KHR_texture_transform = "KHR_texture_transform";
        constexpr std::string_view MSFT_texture_dds = "MSFT_texture_dds";
    }

    struct BufferInfo {
        void* mappedMemory;
        CustomBufferId customId;
    };

    using BufferMapCallback = BufferInfo(uint64_t bufferSize, void* userPointer);
    using BufferUnmapCallback = void(BufferInfo* bufferInfo, void* userPointer);
    using Base64DecodeCallback = void(std::string_view base64, uint8_t* dataOutput, size_t padding, size_t dataOutputSize, void* userPointer);

    class glTF {
        friend class Parser;

        std::unique_ptr<ParserData> data;
        std::unique_ptr<Asset> parsedAsset;
        DataSource glbBuffer;
        std::filesystem::path directory;
        Options options;
        Error errorCode = Error::None;

        explicit glTF(std::unique_ptr<ParserData> data, std::filesystem::path directory, Options options);

        static auto getMimeTypeFromString(std::string_view mime) -> MimeType;

        [[nodiscard]] auto decodeUri(std::string_view uri) const noexcept -> std::pair<Error, DataSource>;
        [[gnu::always_inline]] inline Error parseTextureObject(void* object, std::string_view key, TextureInfo* info) noexcept;

        void parseAccessors(simdjson::dom::array& array);
        void parseAnimations(simdjson::dom::array& array);
        void parseBuffers(simdjson::dom::array& array);
        void parseBufferViews(simdjson::dom::array& array);
        void parseCameras(simdjson::dom::array& array);
        void parseExtensions(simdjson::dom::object& extensionsObject);
        void parseImages(simdjson::dom::array& array);
        void parseLights(simdjson::dom::array& array);
        void parseMaterials(simdjson::dom::array& array);
        void parseMeshes(simdjson::dom::array& array);
        void parseNodes(simdjson::dom::array& array);
        void parseSamplers(simdjson::dom::array& array);
        void parseScenes(simdjson::dom::array& array);
        void parseSkins(simdjson::dom::array& array);
        void parseTextures(simdjson::dom::array& array);

    public:
        explicit glTF(const glTF& scene) = delete;
        glTF& operator=(const glTF& scene) = delete;

        ~glTF();

        [[nodiscard]] std::unique_ptr<Asset> getParsedAsset();

        /**
         * This function further validates all the input that is parsed from the glTF. Note that
         * this function will only validate the data that has already been parsed through calls to
         * glTF::parse(Category categories). Realistically, this should not be necessary in Release
         * applications, but could be helpful when debugging an asset related issue.
         */
        [[nodiscard]] Error validate();

        /**
         * Parses all of the specified categories. Parses everything by default.
         */
        [[nodiscard]] Error parse(Category categories = Category::All);
    };

    /**
     * Enum to represent the type of a glTF file. glTFs can either be the standard JSON file with
     * paths to buffers or with a base64 embedded buffers, or they can be in a so called GLB
     * container format which has two or more chunks of binary data, where one represents buffers
     * and the other contains the JSON string.
     */
    enum class GltfType {
        glTF,
        GLB,
        Invalid,
    };

    GltfType determineGltfFileType(GltfDataBuffer* buffer);

    /**
     * Gets the amount of byte padding required on the GltfDataBuffer, as simdjson requires to be
     * able to overflow as it uses SIMD to load N bytes at a time.
     */
    size_t getGltfBufferPadding() noexcept;

    /**
     * This class holds a chunk of data that makes up a JSON string that the glTF parser will use
     * and read from.
     */
    class GltfDataBuffer {
        friend class Parser;
        friend GltfType determineGltfFileType(GltfDataBuffer* buffer);

        size_t allocatedSize = 0;
        size_t dataSize = 0;
        uint8_t* bufferPointer = nullptr;

        std::unique_ptr<uint8_t[]> buffer;

        std::filesystem::path filePath = {};

    public:
        explicit GltfDataBuffer() noexcept;
        ~GltfDataBuffer() noexcept;

        /**
         * Saves the pointer including its range. Does not copy any data. This requires the
         * original allocation to outlive the parsing of the glTF, so after the last relevant
         * call to fastgltf::glTF::parse. However, this function asks for a capacity size, as
         * the JSON parsing requires some padding. See getGltfBufferPadding for more information.
         * If the capacity does not have enough padding, the function will instead copy the bytes
         * with the copyBytes method. Also, it will set the padding bytes all to 0, so be sure to
         * not use that for any other data.
         */
        bool fromByteView(uint8_t* bytes, size_t byteCount, size_t capacity) noexcept;
        /**
         * This will create a copy of the passed bytes and allocate a adequately sized buffer.
         */
        bool copyBytes(uint8_t* bytes, size_t byteCount) noexcept;
        /**
         * Loads the file with a optional byte offset into a memory buffer.
         */
        bool loadFromFile(const std::filesystem::path& path, uint64_t byteOffset = 0) noexcept;

        /**
         * Returns the size, in bytes,
         * @return
         */
        [[nodiscard]] inline size_t getBufferSize() const noexcept;
    };

    /**
     * Some internals the parser passes on to each glTF instance.
     */
    struct ParserInternalConfig {
        BufferMapCallback* mapCallback = nullptr;
        BufferUnmapCallback* unmapCallback = nullptr;
        Base64DecodeCallback* decodeCallback = nullptr;

        void* userPointer = nullptr;
        Extensions extensions = Extensions::None;
    };

    /**
     * A parser for one or more glTF files. It uses a SIMD based JSON parser to maximize efficiency
     * and performance at runtime.
     *
     * @note This class is not thread-safe.
     */
    class Parser {
        // The simdjson parser object. We want to share it between runs, so it does not need to
        // reallocate over and over again. We're hiding it here to not leak the simdjson header.
        std::unique_ptr<simdjson::dom::parser> jsonParser;

        // Callbacks
        ParserInternalConfig config = {};
        Error errorCode = Error::None;

    public:
        explicit Parser(Extensions extensionsToLoad = Extensions::None) noexcept;
        explicit Parser(const Parser& parser) = delete;
        Parser(Parser&& parser) noexcept;
        Parser& operator=(const Parser& parser) = delete;
        Parser& operator=(Parser&& other) noexcept;

        ~Parser();

        /**
         * Returns the error that made the parsing fail.
         */
        [[nodiscard]] Error getError() const;

        /**
         * Loads a glTF file from pre-loaded bytes representing a JSON file.
         * @return A glTF instance or nullptr if an error occurred.
         */
        [[nodiscard]] std::unique_ptr<glTF> loadGLTF(GltfDataBuffer* buffer, std::filesystem::path directory, Options options = Options::None);
        [[nodiscard]] std::unique_ptr<glTF> loadBinaryGLTF(GltfDataBuffer* buffer, std::filesystem::path directory, Options options = Options::None);

        /**
         * This function can be used to set callbacks so that you can control memory allocation for
         * large buffers and images that are loaded from a glTF file. For example, one could use
         * the callbacks to map a GPU buffer through Vulkan or DirectX so that fastgltf can write
         * the buffer directly to the GPU to avoid a copy into RAM first. To remove the callbacks
         * for a specific load, call this method with both parameters as nullptr before load*GLTF.
         * Using fastgltf::Parser::setUserPointer you can also set a user pointer to access your
         * own class or other data you may need.
         *
         * @param mapCallback function called when the parser requires a buffer to write data
         * embedded in a GLB file or decoded from a base64 URI, cannot be nullptr.
         * @param unmapCallback function called when the parser is done with writing into a
         * buffer, can be nullptr.
         * @note For advanced users
         */
        void setBufferAllocationCallback(BufferMapCallback* mapCallback, BufferUnmapCallback* unmapCallback = nullptr) noexcept;

        /**
         * Allows setting callbacks for base64 decoding. This can be useful if you have another
         * base64 decoder optimised for a certain platform or architecture, or want to use your own
         * scheduler to schedule multiple threads for working on decoding the data.
         * Using fastgltf::Parser::setUserPointer you can also set a user pointer to access your
         * own class or other data you may need.
         *
         * @param decodeCallback function called when the parser tries to decode a base64 buffer
         */
        void setBase64DecodeCallback(Base64DecodeCallback* decodeCallback) noexcept;
        void setUserPointer(void* pointer) noexcept;
    };
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
