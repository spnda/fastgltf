#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>

#include "fastgltf_util.hpp"

namespace fastgltf {
    struct Asset;
    struct ParserData;
    struct DataSource;
    enum class AccessorType : uint16_t;
    enum class ComponentType : uint32_t;
    enum class DataLocation : uint8_t;
    enum class MimeType : uint8_t;

    enum class Error : uint32_t {
        None = 0,
        InvalidPath = 1,
        WrongExtension = 2,
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
         * By default, fastgltf checks for the file extension and checks if it is .gltf or.glb.
         */
        IgnoreFileExtension             = 1 << 2,

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

    constexpr Options operator&(Options a, Options b) {
        return static_cast<Options>(to_underlying(a) & to_underlying(b));
    }

    constexpr Options operator|(Options a, Options b) {
        return static_cast<Options>(to_underlying(a) | to_underlying(b));
    }

    /**
     * A parser for one or more glTF files. It uses a SIMD based JSON parser to maximize efficiency
     * and performance at runtime.
     *
     * @note This class is not thread-safe.
     */
    class Parser {
        std::unique_ptr<Asset> parsedAsset;

        std::filesystem::path currentDirectory;
        Options currentOptions;

        // The simdjson parser object. We want to share it between runs, so it does not need to
        // reallocate over and over again. We're hiding it here to not leak the simdjson header.
        void* jsonParser;

        Error errorCode = Error::None;

        static auto getMimeTypeFromString(std::string_view mime) -> MimeType;

        bool checkAssetField(ParserData* data);
        /**
         * Checks if the path has an extension, whether it matches the given extension string.
         */
        bool checkFileExtension(std::filesystem::path& path, std::string_view extension);
        [[nodiscard]] auto decodeUri(std::string_view uri) const -> std::tuple<Error, DataSource, DataLocation>;
        bool readJsonFile(std::filesystem::path& path, std::vector<uint8_t>& bytes);

    public:
        explicit Parser() noexcept;
        ~Parser();

        explicit Parser(const Parser& scene) = delete;
        Parser& operator=(const Parser& scene) = delete;

        /**
         * Returns the error that made the parsing fail.
         */
        [[nodiscard]] Error getError() const;

        /**
         * This returns a unique pointer to the parsed asset. This can be only called **once**
         * after parsing a file.
         */
        [[nodiscard]] std::unique_ptr<Asset> getParsedAsset();

        /**
         * Loads a glTF file stored in a .glTF file.
         */
        bool loadGLTF(std::filesystem::path path, Options options = Options::None);

        bool loadGLTF(std::string_view path, Options options = Options::None);

        /**
         * Loads a glTF file stored in a GLB container ending with the .glb extension.
         */
        bool loadBinaryGLTF(std::filesystem::path path, Options options = Options::None);
    };
}
