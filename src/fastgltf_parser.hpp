#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "fastgltf_util.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5030) // attribute 'x' is not recognized
#pragma warning(disable : 4514) // unreferenced inline function has been removed
#endif

// fwd
namespace simdjson {
    struct padded_string;
    namespace dom {
        class parser;
    }
}

namespace fastgltf {
    struct Asset;
    struct BinaryGltfChunk;
    struct DataSource;
    struct ParserData;
    struct TextureInfo;
    enum class DataLocation : uint8_t;
    enum class MimeType : uint16_t;

    enum class Error : uint64_t {
        None = 0,
        InvalidPath = 1,
        MissingExtensions = 2,
        UnsupportedExtensions = 3,
        InvalidJson = 4,
        InvalidGltf = 5,
        InvalidOrMissingAssetField = 6,
        InvalidGLB = 6,
        MissingField = 7,
    };

    // clang-format off
    enum class Extensions : uint64_t {
        None = 0,
        KHR_texture_transform = 1 << 1,
        KHR_texture_basisu = 1 << 2,
        MSFT_texture_dds = 1 << 3,
    };

    constexpr Extensions operator&(Extensions a, Extensions b) noexcept {
        return static_cast<Extensions>(to_underlying(a) & to_underlying(b));
    }

    constexpr Extensions operator|(Extensions a, Extensions b) noexcept {
        return static_cast<Extensions>(to_underlying(a) | to_underlying(b));
    }

    enum class Options : uint64_t {
        None                            = 0,
        /**
         * This allows 5130 as an accessor component type, a 64-bit double precision float.
         */
        AllowDouble                     = 1 << 0,

        /**
         * This skips validating the asset field, as it is usually there and not used anyway.
         */
        DontRequireValidAssetMember     = 1 << 1,

        /**
         * This should only be used for benchmarking
         */
        DontUseSIMD                     = 1 << 2,

        /**
         * Loads all the GLB buffers into CPU memory. If disabled, fastgltf will only provide
         * a byte offset and length into the GLB file, which can be useful when using APIs like
         * DirectStorage or Metal IO.
         */
        LoadGLBBuffers                  = 1 << 3,
    };
    // clang-format on

    constexpr Options operator&(Options a, Options b) noexcept {
        return static_cast<Options>(to_underlying(a) & to_underlying(b));
    }

    constexpr Options operator|(Options a, Options b) noexcept {
        return static_cast<Options>(to_underlying(a) | to_underlying(b));
    }

    class glTF {
        friend class Parser;

        struct GLBBuffer {
            size_t fileOffset;
            size_t fileSize;
            std::filesystem::path file;

            std::vector<uint8_t> buffer;
        };
        std::unique_ptr<GLBBuffer> glb;

        std::unique_ptr<ParserData> data;
        std::unique_ptr<Asset> parsedAsset;
        std::filesystem::path directory;
        Options options;
        Extensions extensions;
        Error errorCode = Error::None;

        explicit glTF(std::unique_ptr<ParserData> data, std::filesystem::path directory, Options options, Extensions extension);
        explicit glTF(std::unique_ptr<ParserData> data, std::filesystem::path file, std::vector<uint8_t>&& glbData, Options options, Extensions extension);
        explicit glTF(std::unique_ptr<ParserData> data, std::filesystem::path file, size_t fileOffset, size_t fileSize, Options options, Extensions extension);

        static auto getMimeTypeFromString(std::string_view mime) -> MimeType;

        [[nodiscard]] bool checkAssetField();
        [[nodiscard]] bool checkExtensions();
        [[nodiscard]] auto decodeUri(std::string_view uri) const -> std::tuple<Error, DataSource, DataLocation>;
        [[nodiscard, gnu::always_inline]] inline Error returnError(Error error) noexcept;
        [[gnu::always_inline]] inline Error parseTextureObject(void* object, std::string_view key, TextureInfo* info) noexcept;

    public:
        explicit glTF(const glTF& scene) = delete;
        glTF& operator=(const glTF& scene) = delete;

        ~glTF();

        [[nodiscard]] std::unique_ptr<Asset> getParsedAsset();
        [[nodiscard]] Asset* getParsedAssetPointer();

        [[nodiscard]] Error parseAll();
        Error parseAccessors();
        Error parseAnimations();
        Error parseBuffers();
        Error parseBufferViews();
        Error parseCameras();
        Error parseImages();
        Error parseMaterials();
        Error parseMeshes();
        Error parseNodes();
        Error parseSamplers();
        Error parseScenes();
        Error parseSkins();
        Error parseTextures();
    };

    /**
     * This class represents a chunk of data that makes up a JSON string. It is reusable to
     * reduce memory allocations though has to outlive the glTF object that is created from this.
     * It is not needed when loading GLB files.
     */
    class JsonData {
        friend class Parser;

        std::unique_ptr<simdjson::padded_string> data;

    public:
        explicit JsonData(uint8_t* bytes, size_t byteCount) noexcept;
        // If this constructor fails, getData will return nullptr.
        explicit JsonData(const std::filesystem::path& path) noexcept;

        ~JsonData();

        [[nodiscard, gnu::const]] const uint8_t* getData() const;
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
        Extensions extensions;

        Error errorCode = Error::None;

    public:
        explicit Parser(Extensions extensionsToLoad = Extensions::None) noexcept;
        explicit Parser(const Parser& scene) = delete;
        Parser& operator=(const Parser& scene) = delete;

        ~Parser();

        /**
         * Returns the error that made the parsing fail.
         */
        [[nodiscard]] Error getError() const;

        /**
         * Loads a glTF file from pre-loaded bytes representing a JSON file.
         * @return A glTF instance or nullptr if an error occurred.
         */
        [[nodiscard]] std::unique_ptr<glTF> loadGLTF(JsonData* jsonData, std::filesystem::path directory, Options options = Options::None);

        [[nodiscard]] std::unique_ptr<glTF> loadGLTF(JsonData* jsonData, std::string_view directory, Options options = Options::None);

        [[nodiscard]] std::unique_ptr<glTF> loadBinaryGLTF(const std::filesystem::path& file, Options options = Options::None);

        [[nodiscard]] std::unique_ptr<glTF> loadBinaryGLTF(std::string_view file, Options options = Options::None);
    };
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
