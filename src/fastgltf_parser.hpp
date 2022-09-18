#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>

#include "fastgltf_util.hpp"

// fwd
namespace simdjson {
    struct padded_string;
}

namespace fastgltf {
    struct Asset;
    struct DataSource;
    struct ParserData;
    struct TextureInfo;
    enum class DataLocation : uint8_t;
    enum class MimeType : uint8_t;

    enum class Error : uint32_t {
        None = 0,
        InvalidPath = 1,
        NonExistentPath = 3,
        InvalidJson = 4,
        InvalidGltf = 5,
        InvalidOrMissingAssetField = 6,
    };

    // clang-format off
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
        DontUseSIMD                     = 1 << 3,

        /**
         * Enables loading of KHR_texture_basisu. Images where both KHR_texture_basisu and
         * MSFT_texture_dds are specified only the basisu extension is loaded.
         * @note This only enables loading textures that report as being DDS, it doesn't load
         * the actual images.
         */
        LoadKTXExtension                = 1 << 4,

        /**
         * Enables loading of MSFT_texture_dds
         * @note This only enables loading textures that report as being DDS, it doesn't load
         * the actual images.
         */
        LoadDDSExtension                = 1 << 5,
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

        std::unique_ptr<fastgltf::ParserData> data;
        std::unique_ptr<Asset> parsedAsset;
        std::filesystem::path directory;
        Options options;
        Error errorCode = Error::None;

        explicit glTF(std::unique_ptr<fastgltf::ParserData> data, std::filesystem::path directory, Options options);

        static auto getMimeTypeFromString(std::string_view mime) -> MimeType;

        [[nodiscard]] bool checkAssetField();
        [[nodiscard]] auto decodeUri(std::string_view uri) const -> std::tuple<Error, DataSource, DataLocation>;

        [[gnu::always_inline]] Error parseTextureObject(void* object, std::string_view key, TextureInfo* info) noexcept;

    public:
        explicit glTF(const glTF& scene) = delete;
        glTF& operator=(const glTF& scene) = delete;

        ~glTF();

        [[nodiscard]] std::unique_ptr<Asset> getParsedAsset();
        [[nodiscard]] Asset* getParsedAssetPointer();

        /**
         * This parses all buffers
         * @return
         */
        Error parseAccessors();
        Error parseBuffers();
        Error parseBufferViews();
        Error parseImages();
        Error parseMaterials();
        Error parseMeshes();
        Error parseNodes();
        Error parseScenes();
        Error parseTextures();
    };

    /**
     * This class represents a chunk of data that makes up a JSON string. It is reusable to
     * reduce memory allocations though has to outlive the glTF object that is created from this.
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
        void* jsonParser;

        Error errorCode = Error::None;

    public:
        explicit Parser() noexcept;
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
    };
}
