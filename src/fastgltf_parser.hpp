#pragma once

#include <filesystem>
#include <memory>
#include <utility>

#include "fastgltf_util.hpp"

namespace fastgltf {
    struct Asset;
    struct ParserData;
    struct DataSource;
    enum class AccessorType : uint16_t;
    enum class ComponentType : uint32_t;
    enum class DataLocation : uint8_t;

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
    enum class Options : uint32_t {
        None                            = 0,
        AllowDouble                     = 1 << 0,
        DontRequireValidAssetMember     = 1 << 1,
        IgnoreFileExtension             = 1 << 2,
        DontUseSIMD                     = 1 << 3, // This is just for benchmarking
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

        bool checkAssetField(ParserData* data);
        /**
         * Checks if the path has an extension, whether it matches the given extension string.
         */
        bool checkFileExtension(std::filesystem::path& path, std::string_view extension);
        std::tuple<Error, DataSource, DataLocation> decodeUri(std::string_view uri);
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
        bool loadGlTF(std::filesystem::path path, Options options = Options::None);

        /**
         * Loads a glTF file stored in a GLB container ending with the .glb extension.
         */
        bool loadBinaryGlTF(std::filesystem::path path, Options options = Options::None);
    };
}
