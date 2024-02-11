/*
 * Copyright (C) 2022 - 2024 spnda
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

#if !defined(__cplusplus) || (!defined(_MSVC_LANG) && __cplusplus < 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG < 201703L)
#error "fastgltf requires C++17"
#endif

#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <mutex>
#include <utility>

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5030) // attribute 'x' is not recognized
#pragma warning(disable : 4514) // unreferenced inline function has been removed
#endif

#include <simdjson.h>

#ifdef SIMDJSON_TARGET_VERSION
// Make sure that SIMDJSON_TARGET_VERSION is equal to SIMDJSON_VERSION.
static_assert(std::string_view { SIMDJSON_TARGET_VERSION } == SIMDJSON_VERSION, "Outdated version of simdjson. Reconfigure project to update.");
#endif

#include <fastgltf/core.hpp>
#include <fastgltf/base64.hpp>

#if defined(FASTGLTF_IS_X86)
#include <nmmintrin.h> // SSE4.2 for the CRC-32C instructions
#endif

namespace fg = fastgltf;
namespace fs = std::filesystem;

namespace fastgltf {
    constexpr std::uint32_t binaryGltfHeaderMagic = 0x46546C67; // ASCII for "glTF".
    constexpr std::uint32_t binaryGltfJsonChunkMagic = 0x4E4F534A;
    constexpr std::uint32_t binaryGltfDataChunkMagic = 0x004E4942;

    struct BinaryGltfHeader {
        std::uint32_t magic;
        std::uint32_t version;
        std::uint32_t length;
    };
    static_assert(sizeof(BinaryGltfHeader) == 12, "Binary gltf header must be 12 bytes");
	static_assert(std::is_trivially_copyable_v<BinaryGltfHeader>);

    struct BinaryGltfChunk {
        std::uint32_t chunkLength;
        std::uint32_t chunkType;
    };
	static_assert(std::is_trivially_copyable_v<BinaryGltfChunk>);

    using CRCStringFunction = std::uint32_t(*)(std::string_view str);

#if defined(FASTGLTF_IS_X86)
    [[gnu::hot, gnu::const, gnu::target("sse4.2")]] std::uint32_t sse_crc32c(std::string_view str) noexcept {
        return sse_crc32c(reinterpret_cast<const std::uint8_t*>(str.data()), str.size());
    }

    [[gnu::hot, gnu::const, gnu::target("sse4.2")]] std::uint32_t sse_crc32c(const std::uint8_t* d, std::size_t len) noexcept {
        std::uint32_t crc = 0;

        // Ddecode as much as possible using 4 byte steps.
        // We specifically don't use the 8 byte instruction here because it uses a 64-bit output integer.
        auto length = static_cast<std::int64_t>(len);
        while ((length -= sizeof(std::uint32_t)) >= 0) {
            std::uint32_t v;
            std::memcpy(&v, d, sizeof v);
            crc = _mm_crc32_u32(crc, v);
            d += sizeof v;
        }

        if (length & sizeof(std::uint16_t)) {
            std::uint16_t v;
            std::memcpy(&v, d, sizeof v);
            crc = _mm_crc32_u16(crc, v);
            d += sizeof v;
        }

        if (length & sizeof(std::uint8_t)) {
            crc = _mm_crc32_u8(crc, *d);
        }

        return crc;
    }
#elif defined(FASTGLTF_ENABLE_ARMV8_CRC)
// MSVC does not provide the arm crc32 intrinsics.
#include <arm_acle.h>

	[[gnu::hot, gnu::const, gnu::target("+crc")]] std::uint32_t armv8_crc32c(std::string_view str) noexcept {
		return armv8_crc32c(reinterpret_cast<const std::uint8_t*>(str.data()), str.size());
	}

	[[gnu::hot, gnu::const, gnu::target("+crc")]] std::uint32_t armv8_crc32c(const std::uint8_t* d, std::size_t len) noexcept {
		std::uint32_t crc = 0;

		// Decrementing the length variable and incrementing the pointer directly has better codegen with Clang
		// than using a std::size_t i = 0.
		// TODO: is there perhaps just some intrinsic we can use instead of inline asm?
		auto length = static_cast<std::int64_t>(len);
		while ((length -= sizeof(std::uint64_t)) >= 0) {
			std::uint64_t value;
			std::memcpy(&value, d, sizeof value);
			crc = __crc32cd(crc, value);
			d += sizeof value;
		}

		if (length & sizeof(std::uint32_t)) {
			std::uint32_t value;
			std::memcpy(&value, d, sizeof value);
			crc = __crc32cw(crc, value);
			d += sizeof value;
		}

		if (length & sizeof(std::uint16_t)) {
			std::uint16_t value;
			std::memcpy(&value, d, sizeof value);
			crc = __crc32ch(crc, value);
			d += sizeof value;
		}

		if (length & sizeof(std::uint8_t)) {
			crc = __crc32cb(crc, *d);
		}

		return crc;
	}
#endif

    /**
     * Points to the most 'optimal' CRC32-C encoding function. After initialiseCrc has been called,
     * this might also point to sse_crc32c or armv8_crc32c. We only use this for runtime evaluation of hashes, and is
     * intended to work for any length of data.
     */
    static CRCStringFunction crcStringFunction = crc32c;

    std::once_flag crcInitialisation;

    /**
     * Checks if SSE4.2 is available to try and use the hardware accelerated version.
     */
    void initialiseCrc() {
#if defined(FASTGLTF_IS_X86)
        const auto& impls = simdjson::get_available_implementations();
        if (const auto* sse4 = impls["westmere"]; sse4 != nullptr && sse4->supported_by_runtime_system()) {
            crcStringFunction = sse_crc32c;
        }
#elif defined(FASTGLTF_ENABLE_ARMV8_CRC)
		const auto& impls = simdjson::get_available_implementations();
		if (const auto* neon = impls["arm64"]; neon != nullptr && neon->supported_by_runtime_system()) {
			crcStringFunction = armv8_crc32c;
		}
#endif
    }

	[[nodiscard, gnu::always_inline]] inline std::tuple<bool, bool, std::size_t> getImageIndexForExtension(simdjson::ondemand::object& object, std::string_view extension) {
		using namespace simdjson;

		ondemand::object sourceExtensionObject;
		if (object[extension].get_object().get(sourceExtensionObject) != SUCCESS) FASTGLTF_LIKELY {
			return std::make_tuple(false, true, 0U);
		}

		std::uint64_t imageIndex;
		if (sourceExtensionObject["source"].get(imageIndex) != SUCCESS) FASTGLTF_UNLIKELY {
			return std::make_tuple(true, false, 0U);
		}

		return std::make_tuple(false, false, imageIndex);
	}

	[[nodiscard, gnu::always_inline]] inline bool parseTextureExtensions(Texture& texture, simdjson::ondemand::object& extensions, Extensions extensionFlags) {
		if (hasBit(extensionFlags, Extensions::KHR_texture_basisu)) {
			auto [invalidGltf, extensionNotPresent, imageIndex] = getImageIndexForExtension(extensions, extensions::KHR_texture_basisu);
			if (invalidGltf) {
				return false;
			}

			if (!extensionNotPresent) {
				texture.basisuImageIndex = imageIndex;
				return true;
			}
		}

		if (hasBit(extensionFlags, Extensions::MSFT_texture_dds)) {
			auto [invalidGltf, extensionNotPresent, imageIndex] = getImageIndexForExtension(extensions, extensions::MSFT_texture_dds);
			if (invalidGltf) {
				return false;
			}

			if (!extensionNotPresent) {
				texture.ddsImageIndex = imageIndex;
				return true;
			}
		}

		if (hasBit(extensionFlags, Extensions::EXT_texture_webp)) {
			auto [invalidGltf, extensionNotPresent, imageIndex] = getImageIndexForExtension(extensions, extensions::EXT_texture_webp);
			if (invalidGltf) {
				return false;
			}

			if (!extensionNotPresent) {
				texture.webpImageIndex = imageIndex;
				return true;
			}
		}

		return false;
	}

	[[nodiscard, gnu::always_inline]] inline Error getJsonArray(simdjson::ondemand::object& parent, std::string_view arrayName, simdjson::ondemand::array* array) noexcept {
		using namespace simdjson;

		const auto error = parent[arrayName].get_array().get(*array);
		if (error == NO_SUCH_FIELD) {
			return Error::MissingField;
		}
		if (error == SUCCESS) FASTGLTF_LIKELY {
			return Error::None;
		}
		return Error::InvalidJson;
	}

    template <typename X, typename T, std::size_t N>
    [[nodiscard, gnu::always_inline]] inline Error copyNumericalJsonArray(simdjson::ondemand::array& array, std::array<T, N>& target) {
        std::size_t i = 0;
        for (auto value : array) {
            if (i >= target.size()) {
                return Error::InvalidGltf;
            }
            X t;
            if (auto error = value.get<X>().get(t); error != simdjson::SUCCESS) {
                return error == simdjson::INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
            }
            target[i++] = static_cast<T>(t);
        }
        if (i != target.size()) {
            return Error::InvalidGltf;
        }
        return Error::None;
    }

	enum class TextureInfoType {
		Standard = 0,
		NormalTexture = 1,
		OcclusionTexture = 2,
	};

	fg::Error parseTextureInfo(simdjson::ondemand::object& object, TextureInfo* info, Extensions extensions, TextureInfoType type = TextureInfoType::Standard) noexcept {
		using namespace simdjson;

		std::uint64_t index;
		if (object["index"].get(index) == SUCCESS) FASTGLTF_LIKELY {
			info->textureIndex = static_cast<std::size_t>(index);
		} else {
			return Error::InvalidGltf;
		}

		if (auto error = object["texCoord"].get(index); error == SUCCESS) FASTGLTF_LIKELY {
			info->texCoordIndex = static_cast<std::size_t>(index);
		} else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
            return Error::InvalidJson;
		}

		if (type == TextureInfoType::NormalTexture) {
            double scale;
			if (auto error = object["scale"].get(scale); error == SUCCESS) FASTGLTF_LIKELY {
				reinterpret_cast<NormalTextureInfo*>(info)->scale = static_cast<num>(scale);
			} else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                return Error::InvalidGltf;
			}
		} else if (type == TextureInfoType::OcclusionTexture) {
			double strength;
			if (auto error = object["strength"].get(strength); error == SUCCESS) FASTGLTF_LIKELY {
				reinterpret_cast<OcclusionTextureInfo*>(info)->strength = static_cast<num>(strength);
			} else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                return Error::InvalidGltf;
            }
		}

		ondemand::object extensionsObject;
		if (object["extensions"].get_object().get(extensionsObject) == SUCCESS) FASTGLTF_LIKELY {
			ondemand::object textureTransform;
			if (hasBit(extensions, Extensions::KHR_texture_transform) && extensionsObject[extensions::KHR_texture_transform].get_object().get(textureTransform) == SUCCESS) {
				auto transform = std::make_unique<TextureTransform>();
				transform->rotation = 0.0F;
				transform->uvOffset = {{ 0.0F, 0.0F }};
				transform->uvScale = {{ 1.0F, 1.0F }};

				if (textureTransform["texCoord"].get(index) == SUCCESS) FASTGLTF_LIKELY {
					transform->texCoordIndex = index;
				}

				double rotation = 0.0F;
				if (textureTransform["rotation"].get(rotation) == SUCCESS) FASTGLTF_LIKELY {
					transform->rotation = static_cast<num>(rotation);
				}

				ondemand::array array;
				if (auto error = textureTransform["offset"].get(array); error == SUCCESS) FASTGLTF_LIKELY {
                    if (auto copyError = copyNumericalJsonArray<double>(array, transform->uvOffset); copyError != Error::None) {
                        return copyError;
                    }
				} else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

				if (auto error = textureTransform["scale"].get(array); error == SUCCESS) FASTGLTF_LIKELY {
                    if (auto copyError = copyNumericalJsonArray<double>(array, transform->uvScale); copyError != Error::None) {
                        return copyError;
                    }
				} else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

				info->transform = std::move(transform);
			}
		}

		return Error::None;
	}

	void writeTextureInfo(std::string& json, const TextureInfo* info, TextureInfoType type = TextureInfoType::Standard) {
		json += '{';
		json += "\"index\":" + std::to_string(info->textureIndex);
		if (info->texCoordIndex != 0) {
			json += ",\"texCoord\":" + std::to_string(info->texCoordIndex);
		}
		if (type == TextureInfoType::NormalTexture) {
			json += ",\"scale\":" + std::to_string(reinterpret_cast<const NormalTextureInfo*>(info)->scale);
		} else if (type == TextureInfoType::OcclusionTexture) {
			json += ",\"strength\":" + std::to_string(reinterpret_cast<const OcclusionTextureInfo*>(info)->strength);
		}

        if (info->transform != nullptr) {
            json += R"(,"extensions":{"KHR_texture_transform":{)";
            const auto& transform = *info->transform;
            if (transform.uvOffset[0] != 0.0 || transform.uvOffset[1] != 0.0) {
                json += "\"offset\":[" + std::to_string(transform.uvOffset[0]) + ',' + std::to_string(transform.uvOffset[1]) + ']';
            }
            if (transform.rotation != 0.0) {
                if (json.back() != '{') json += ',';
                json += "\"rotation\":" + std::to_string(transform.rotation);
            }
            if (transform.uvScale[0] != 1.0 || transform.uvScale[1] != 1.0) {
                if (json.back() != '{') json += ',';
                json += "\"scale\":[" + std::to_string(transform.uvScale[0]) + ',' + std::to_string(transform.uvScale[1]) + ']';
            }
            if (transform.texCoordIndex.has_value()) {
                if (json.back() != '{') json += ',';
                json += "\"texCoord\":" + std::to_string(transform.texCoordIndex.value());
            }
            json += "}}";
        }

		json += '}';
	}
} // namespace fastgltf

#pragma region URI
fg::URIView::URIView() noexcept = default;

fg::URIView::URIView(std::string_view uri) noexcept : view(uri) {
	parse();
}

fg::URIView::URIView(const URIView& other) noexcept {
	*this = other;
}

fg::URIView& fg::URIView::operator=(const URIView& other) {
	view = other.view;
	_scheme = other._scheme;
	_path = other._path;
	_userinfo = other._userinfo;
	_host = other._host;
	_port = other._port;
	_query = other._query;
	_fragment = other._fragment;
	return *this;
}

fg::URIView& fg::URIView::operator=(std::string_view other) {
	view = other;
	parse();
	return *this;
}

void fg::URIView::parse() {
	if (view.empty()) {
		_valid = false;
		return;
	}

	std::size_t idx = 0;
	auto firstColon = view.find(':');
	if (firstColon != std::string::npos) {
		// URI has a scheme.
		if (firstColon == 0) {
			// Empty scheme is invalid
			_valid = false;
			return;
		}
		_scheme = view.substr(0, firstColon);
		idx = firstColon + 1;
	}

	if (startsWith(view.substr(idx), "//")) {
		// URI has an authority part.
		idx += 2;
		auto nextSlash = view.find('/', idx);
		auto userInfo = view.find('@', idx);
		if (userInfo != std::string::npos && userInfo < nextSlash) {
			_userinfo = view.substr(idx, userInfo - idx);
			idx += _userinfo.size() + 1;
		}

		auto hostEnd = nextSlash - 1;
		std::size_t portColon;
		if (view[idx] == '[') {
			hostEnd = view.find(']', idx);
			if (hostEnd == std::string::npos) {
				_valid = false;
				return;
			}
			// IPv6 addresses are made up of colons, so we need to search after its address.
			// This will just be hostEnd + 1 or std::string::npos.
			portColon = view.find(':', hostEnd);
		} else {
			portColon = view.find(':', idx);
		}

		if (portColon != std::string::npos) {
			_host = view.substr(idx, portColon - idx);
			++portColon; // We don't want to include the colon in the port string.
			_port = view.substr(portColon, nextSlash - portColon);
		} else {
			++idx;
			_host = view.substr(idx, hostEnd - idx);
		}

		idx = nextSlash; // Path includes this slash
	}

	if (_scheme == "data") {
		// The data scheme is just followed by a mime and then bytes.
		// Also, let's avoid all the find and substr on very large data strings
		// which can be multiple MB.
		_path = view.substr(idx);
	} else {
		// Parse the path.
		auto questionIdx = view.find('?', idx);
		auto hashIdx = view.find('#', idx);
		if (questionIdx != std::string::npos) {
			_path = view.substr(idx, questionIdx - idx);

			if (hashIdx == std::string::npos) {
				_query = view.substr(++questionIdx);
			} else {
				++questionIdx;
				_query = view.substr(questionIdx, hashIdx - questionIdx);
				_fragment = view.substr(++hashIdx);
			}
		} else if (hashIdx != std::string::npos) {
			_path = view.substr(idx, hashIdx - idx);
			_fragment = view.substr(++hashIdx);
		} else {
			_path = view.substr(idx);
		}
	}
}

const char* fg::URIView::data() const noexcept {
	return view.data();
}

std::string_view fg::URIView::string() const noexcept { return view; }
std::string_view fg::URIView::scheme() const noexcept { return _scheme; }
std::string_view fg::URIView::userinfo() const noexcept { return _userinfo; }
std::string_view fg::URIView::host() const noexcept { return _host; }
std::string_view fg::URIView::port() const noexcept { return _port; }
std::string_view fg::URIView::path() const noexcept { return _path; }
std::string_view fg::URIView::query() const noexcept { return _query; }
std::string_view fg::URIView::fragment() const noexcept { return _fragment; }

fs::path fg::URIView::fspath() const {
	if (!isLocalPath())
		return {};
	return { path() };
}

bool fg::URIView::valid() const noexcept {
	return _valid;
}

bool fg::URIView::isLocalPath() const noexcept {
	return scheme().empty() || (scheme() == "file" && host().empty());
}

bool fg::URIView::isDataUri() const noexcept {
	return scheme() == "data";
}

fg::URI::URI() noexcept = default;

fg::URI::URI(std::string uri) noexcept : uri(std::move(uri)) {
	decodePercents(this->uri);
	view = this->uri; // Also parses.
}

fg::URI::URI(std::string_view uri) noexcept : uri(uri) {
	decodePercents(this->uri);
	view = this->uri; // Also parses.
}

fg::URI::URI(const URIView& view) noexcept : uri(view.view) {
	readjustViews(view);
}

// Some C++ stdlib implementations copy in some cases when moving strings, which invalidates the
// views stored in the URI struct. This function adjusts the views from the old string to the new
// string for safe copying.
fg::URI::URI(const URI& other) {
	*this = other;
}

fg::URI::URI(URI&& other) noexcept {
	*this = other;
}

fg::URI& fg::URI::operator=(const URI& other) {
	uri = other.uri;
	// We'll assume that with copying the string will always have to reallocate.
	readjustViews(other.view);
	return *this;
}

fg::URI& fg::URI::operator=(const URIView& other) {
	uri = other.view;
	auto oldSize = uri.size();
	decodePercents(uri);
	if (uri.size() == oldSize) {
		readjustViews(other);
	} else {
		// We removed some encoded chars, which have now invalidated all the string views.
		// Therefore, the URI needs to be parsed again.
		view = this->uri;
	}
	return *this;
}

fg::URI& fg::URI::operator=(URI&& other) noexcept {
	auto* oldData = other.uri.data();
	uri = std::move(other.uri);

	// Invalidate the previous URI's view.
	view._valid = other.view._valid;
	other.view._valid = false;

	if (uri.data() != oldData) {
		// Allocation changed, we need to readjust views
		readjustViews(other.view);
	} else {
		// No reallocation happened, we can safely copy the view.
		view = other.view;
	}
	return *this;
}

fg::URI::operator fg::URIView() const noexcept {
	return view;
}

void fg::URI::readjustViews(const URIView& other) {
	if (!other._scheme.empty())   { view._scheme     = std::string_view(uri.data() + (other._scheme.data()     - other.view.data()), other._scheme.size()); }
	if (!other._path.empty())     { view._path       = std::string_view(uri.data() + (other._path.data()       - other.view.data()), other._path.size()); }
	if (!other._userinfo.empty()) { view._userinfo   = std::string_view(uri.data() + (other._userinfo.data()   - other.view.data()), other._userinfo.size()); }
	if (!other._host.empty())     { view._host       = std::string_view(uri.data() + (other._host.data()       - other.view.data()), other._host.size()); }
	if (!other._port.empty())     { view._port       = std::string_view(uri.data() + (other._port.data()       - other.view.data()), other._port.size()); }
	if (!other._query.empty())    { view._query      = std::string_view(uri.data() + (other._query.data()      - other.view.data()), other._query.size()); }
	if (!other._fragment.empty()) { view._fragment   = std::string_view(uri.data() + (other._fragment.data()   - other.view.data()), other._fragment.size()); }

	view.view = uri;
}

void fg::URI::decodePercents(std::string& x) noexcept {
	for (auto it = x.begin(); it != x.end(); ++it) {
		if (*it == '%') {
			// Read the next two chars and store them.
			std::array<char, 3> chars = {*(it + 1), *(it + 2), 0};
			*it = static_cast<char>(std::strtoul(chars.data(), nullptr, 16));
			x.erase(it + 1, it + 3);
		}
	}
}

std::string_view fg::URI::string() const noexcept { return uri; }
std::string_view fg::URI::scheme() const noexcept { return view.scheme(); }
std::string_view fg::URI::userinfo() const noexcept { return view.userinfo(); }
std::string_view fg::URI::host() const noexcept { return view.host(); }
std::string_view fg::URI::port() const noexcept { return view.port(); }
std::string_view fg::URI::path() const noexcept { return view.path(); }
std::string_view fg::URI::query() const noexcept { return view.query(); }
std::string_view fg::URI::fragment() const noexcept { return view.fragment(); }

fs::path fg::URI::fspath() const {
	return view.fspath();
}

bool fg::URI::valid() const noexcept {
	return view.valid();
}

bool fg::URI::isLocalPath() const noexcept {
	return view.isLocalPath();
}

bool fg::URI::isDataUri() const noexcept {
	return view.isDataUri();
}
#pragma endregion

#pragma region glTF parsing
fg::Expected<fg::DataSource> fg::Parser::decodeDataUri(URIView& uri) const noexcept {
    auto path = uri.path();
    auto mimeEnd = path.find(';');
    auto mime = path.substr(0, mimeEnd);

    auto encodingEnd = path.find(',');
    auto encoding = path.substr(mimeEnd + 1, encodingEnd - mimeEnd - 1);
    if (encoding != "base64") {
		return Expected<DataSource> { Error::InvalidURI };
    }

    auto encodedData = path.substr(encodingEnd + 1);
    if (config.mapCallback != nullptr) {
        // If a map callback is specified, we use a pointer to memory specified by it.
        auto padding = base64::getPadding(encodedData);
        auto size = base64::getOutputSize(encodedData.size(), padding);
        auto info = config.mapCallback(size, config.userPointer);
        if (info.mappedMemory != nullptr) {
            if (config.decodeCallback != nullptr) {
                config.decodeCallback(encodedData, reinterpret_cast<std::uint8_t*>(info.mappedMemory), padding, size, config.userPointer);
            } else {
                base64::decode_inplace(encodedData, reinterpret_cast<std::uint8_t*>(info.mappedMemory), padding);
            }

            if (config.unmapCallback != nullptr) {
                config.unmapCallback(&info, config.userPointer);
            }

            sources::CustomBuffer source = {};
            source.id = info.customId;
            source.mimeType = getMimeTypeFromString(mime);
			return Expected<DataSource> { source };
        }
    }

    // Decode the base64 data into a traditional vector
    auto padding = base64::getPadding(encodedData);
    fg::StaticVector<std::uint8_t> uriData(base64::getOutputSize(encodedData.size(), padding));
    if (config.decodeCallback != nullptr) {
        config.decodeCallback(encodedData, uriData.data(), padding, uriData.size(), config.userPointer);
    } else {
        base64::decode_inplace(encodedData, uriData.data(), padding);
    }

    sources::Array source = {
		std::move(uriData),
		getMimeTypeFromString(mime),
    };
	return Expected<DataSource> { std::move(source) };
}

fg::Expected<fg::DataSource> fg::Parser::loadFileFromUri(URIView& uri) const noexcept {
    auto path = directory / fs::u8path(uri.path());
    std::error_code error;
    // If we were instructed to load external buffers and the files don't exist, we'll return an error.
    if (!fs::exists(path, error) || error) {
	    return Expected<DataSource> { Error::MissingExternalBuffer };
    }

    auto length = static_cast<std::streamsize>(std::filesystem::file_size(path, error));
    if (error) {
	    return Expected<DataSource> { Error::InvalidURI };
    }

    std::ifstream file(path, std::ios::binary);

    if (config.mapCallback != nullptr) {
        auto info = config.mapCallback(static_cast<std::uint64_t>(length), config.userPointer);
        if (info.mappedMemory != nullptr) {
            const sources::CustomBuffer customBufferSource = { info.customId, MimeType::None };
            file.read(reinterpret_cast<char*>(info.mappedMemory), length);
            if (config.unmapCallback != nullptr) {
                config.unmapCallback(&info, config.userPointer);
            }

	        return Expected<DataSource> { customBufferSource };
        }
    }

	StaticVector<std::uint8_t> data(length);
	file.read(reinterpret_cast<char*>(data.data()), length);
    sources::Array vectorSource = {
		std::move(data),
		MimeType::None,
	};
	return Expected<DataSource> { std::move(vectorSource) };
}

void fg::Parser::fillCategories(Category& inputCategories) noexcept {
    if (inputCategories == Category::All)
        return;

    // The Category enum used to already OR values together so that e.g. Scenes would also implicitly
    // have the Nodes bit set. This, however, caused some issues within the parse function as it tries
    // to bail out when all requested categories have been parsed, as now something that hasn't been
    // parsed could still be set. So, this has to exist...
    if (hasBit(inputCategories, Category::Scenes))
        inputCategories |= Category::Nodes;
    if (hasBit(inputCategories, Category::Nodes))
        inputCategories |= Category::Cameras | Category::Meshes | Category::Skins;
    if (hasBit(inputCategories, Category::Skins))
        // Skins needs nodes, nodes needs skins. To counter this circular dep we just redefine what we just wrote above.
        inputCategories |= Category::Accessors | (Category::Nodes | Category::Cameras | Category::Meshes | Category::Skins);
    if (hasBit(inputCategories, Category::Meshes))
        inputCategories |= Category::Accessors | Category::Materials;
    if (hasBit(inputCategories, Category::Materials))
        inputCategories |= Category::Textures;
    if (hasBit(inputCategories, Category::Animations))
        inputCategories |= Category::Accessors;
    if (hasBit(inputCategories, Category::Textures))
        inputCategories |= Category::Images | Category::Samplers;
    if (hasBit(inputCategories, Category::Images) || hasBit(inputCategories, Category::Accessors))
        inputCategories |= Category::BufferViews;
    if (hasBit(inputCategories, Category::BufferViews))
        inputCategories |= Category::Buffers;
}

fg::MimeType fg::Parser::getMimeTypeFromString(std::string_view mime) {
    const auto hash = crcStringFunction(mime);
    switch (hash) {
        case force_consteval<crc32c(mimeTypeJpeg)>: {
            return MimeType::JPEG;
        }
        case force_consteval<crc32c(mimeTypePng)>: {
            return MimeType::PNG;
        }
        case force_consteval<crc32c(mimeTypeKtx)>: {
            return MimeType::KTX2;
        }
        case force_consteval<crc32c(mimeTypeDds)>: {
            return MimeType::DDS;
        }
        case force_consteval<crc32c(mimeTypeGltfBuffer)>: {
            return MimeType::GltfBuffer;
        }
        case force_consteval<crc32c(mimeTypeOctetStream)>: {
            return MimeType::OctetStream;
        }
        default: {
            return MimeType::None;
        }
    }
}

fg::Error fg::Parser::generateMeshIndices(fastgltf::Asset& asset) const {
	for (auto& mesh : asset.meshes) {
		for (auto& primitive : mesh.primitives) {
			if (primitive.indicesAccessor.has_value())
				continue;

			auto* positionAttribute = primitive.findAttribute("POSITION");
			if (positionAttribute == primitive.attributes.end()) {
				return Error::InvalidGltf;
			}
			auto& positionAccessor = asset.accessors[positionAttribute->second];

			StaticVector<std::uint8_t> generatedIndices(positionAccessor.count * getElementByteSize(positionAccessor.type, positionAccessor.componentType));
			fastgltf::span<std::uint32_t> indices { reinterpret_cast<std::uint32_t*>(generatedIndices.data()),
													generatedIndices.size() / sizeof(std::uint32_t) };
			for (std::size_t i = 0; i < positionAccessor.count; ++i) {
				indices[i] = static_cast<std::uint32_t>(i);
			}

			auto bufferIdx = asset.buffers.size();

			auto bufferViewIdx = asset.bufferViews.size();
			auto& bufferView = asset.bufferViews.emplace_back();
			bufferView.byteLength = generatedIndices.size_bytes();
			bufferView.bufferIndex = bufferIdx;
			bufferView.byteOffset = 0;

			auto accessorIdx = asset.accessors.size();
			auto& accessor = asset.accessors.emplace_back();
			accessor.byteOffset = 0;
			accessor.count = positionAccessor.count;
			accessor.type = AccessorType::Scalar;
			accessor.componentType = ComponentType::UnsignedInt;
			accessor.normalized = false;
			accessor.bufferViewIndex = bufferViewIdx;

			sources::Array indicesArray {
				std::move(generatedIndices),
			};
			auto& buffer = asset.buffers.emplace_back();
			buffer.byteLength = generatedIndices.size_bytes();
			buffer.data = std::move(indicesArray);
			primitive.indicesAccessor = accessorIdx;
		}
	}
	return Error::None;
}

fg::Error fg::validate(const fastgltf::Asset& asset) {
	auto isExtensionUsed = [&used = asset.extensionsUsed](std::string_view extension) {
		for (const auto& extensionUsed : used) {
			if (extension == extensionUsed) {
				return true;
			}
		}
		return false;
	};

	// From the spec: extensionsRequired is a subset of extensionsUsed. All values in extensionsRequired MUST also exist in extensionsUsed.
	if (asset.extensionsRequired.size() > asset.extensionsUsed.size()) {
		return Error::InvalidGltf;
	}

	for (const auto& accessor : asset.accessors) {
		if (accessor.type == AccessorType::Invalid)
			return Error::InvalidGltf;
		if (accessor.componentType == ComponentType::Invalid)
			return Error::InvalidGltf;
		if (accessor.count < 1)
			return Error::InvalidGltf;
		if (accessor.bufferViewIndex.has_value() &&
		    accessor.bufferViewIndex.value() >= asset.bufferViews.size())
			return Error::InvalidGltf;
		if (accessor.byteOffset != 0) {
			// The offset of an accessor into a bufferView (i.e., accessor.byteOffset)
			// and the offset of an accessor into a buffer (i.e., accessor.byteOffset + bufferView.byteOffset)
			// MUST be a multiple of the size of the accessor’s component type.
			auto componentByteSize = getComponentBitSize(accessor.componentType) / 8;
			if (accessor.byteOffset % componentByteSize != 0)
				return Error::InvalidGltf;

			if (accessor.bufferViewIndex.has_value()) {
				const auto& bufferView = asset.bufferViews[accessor.bufferViewIndex.value()];
				if ((accessor.byteOffset + bufferView.byteOffset) % componentByteSize != 0)
					return Error::InvalidGltf;

				// When byteStride is defined, it MUST be a multiple of the size of the accessor’s component type.
				if (bufferView.byteStride.has_value() && bufferView.byteStride.value() % componentByteSize != 0)
					return Error::InvalidGltf;
			}
		}

		if (!std::holds_alternative<std::monostate>(accessor.max)) {
			if ((accessor.componentType == ComponentType::Float || accessor.componentType == ComponentType::Double)
			    && !std::holds_alternative<FASTGLTF_STD_PMR_NS::vector<double>>(accessor.max))
				return Error::InvalidGltf;
		}
		if (!std::holds_alternative<std::monostate>(accessor.min)) {
			if ((accessor.componentType == ComponentType::Float || accessor.componentType == ComponentType::Double)
			    && !std::holds_alternative<FASTGLTF_STD_PMR_NS::vector<double>>(accessor.min))
				return Error::InvalidGltf;
		}
	}

	for (const auto& animation : asset.animations) {
		if (animation.channels.empty())
			return Error::InvalidGltf;
		if (animation.samplers.empty())
			return Error::InvalidGltf;
	}

	for (const auto& buffer : asset.buffers) {
		if (buffer.byteLength < 1)
			return Error::InvalidGltf;
	}

	for (const auto& bufferView : asset.bufferViews) {
		if (bufferView.byteLength < 1)
			return Error::InvalidGltf;
		if (bufferView.byteStride.has_value() && (bufferView.byteStride < 4U || bufferView.byteStride > 252U))
			return Error::InvalidGltf;
		if (bufferView.bufferIndex >= asset.buffers.size())
			return Error::InvalidGltf;

		if (bufferView.meshoptCompression != nullptr && isExtensionUsed(extensions::EXT_meshopt_compression))
			return Error::InvalidGltf;

		if (bufferView.meshoptCompression) {
			const auto& compression = bufferView.meshoptCompression;
			switch (compression->mode) {
				case MeshoptCompressionMode::Attributes:
					if (compression->byteStride % 4 != 0 || compression->byteStride > 256)
						return Error::InvalidGltf;
					break;
				case MeshoptCompressionMode::Triangles:
					if (compression->count % 3 != 0)
						return Error::InvalidGltf;
					[[fallthrough]];
				case MeshoptCompressionMode::Indices:
					if (compression->byteStride != 2 && compression->byteStride != 4)
						return Error::InvalidGltf;
					break;
			}
		}
	}

	for (const auto& camera : asset.cameras) {
		if (const auto* pOrthographic = std::get_if<Camera::Orthographic>(&camera.camera)) {
			if (pOrthographic->zfar == 0)
				return Error::InvalidGltf;
		} else if (const auto* pPerspective = std::get_if<Camera::Perspective>(&camera.camera)) {
			if (pPerspective->aspectRatio.has_value() && pPerspective->aspectRatio == .0f)
				return Error::InvalidGltf;
			if (pPerspective->yfov == 0)
				return Error::InvalidGltf;
			if (pPerspective->zfar.has_value() && pPerspective->zfar == .0f)
				return Error::InvalidGltf;
			if (pPerspective->znear == 0.0F)
				return Error::InvalidGltf;
		}
	}

	for (const auto& image : asset.images) {
		if (const auto* view = std::get_if<sources::BufferView>(&image.data); view != nullptr) {
			if (view->bufferViewIndex >= asset.bufferViews.size()) {
				return Error::InvalidGltf;
			}
		}
	}

	for (const auto& light : asset.lights) {
		if (light.type == LightType::Directional && light.range.has_value())
			return Error::InvalidGltf;
		if (light.range.has_value() && light.range.value() <= 0)
			return Error::InvalidGltf;

		if (light.type != LightType::Spot) {
			if (light.innerConeAngle.has_value() || light.outerConeAngle.has_value()) {
				return Error::InvalidGltf;
			}
		} else {
			if (!light.innerConeAngle.has_value() || !light.outerConeAngle.has_value())
				return Error::InvalidGltf;
			if (light.innerConeAngle.value() < 0)
				return Error::InvalidGltf;
			if (light.innerConeAngle.value() > light.outerConeAngle.value())
				return Error::InvalidGltf;
			static constexpr double pi = 3.141592653589793116;
			if (light.outerConeAngle.value() > pi / 2)
				return Error::InvalidGltf;
		}
	}

	for (const auto& material : asset.materials) {
		auto isInvalidTexture = [&textures = asset.textures](std::optional<std::size_t> textureIndex) {
			return textureIndex.has_value() && textureIndex.value() >= textures.size();
		};
		if (material.normalTexture.has_value() && isInvalidTexture(material.normalTexture->textureIndex))
			return Error::InvalidGltf;
		if (material.emissiveTexture.has_value() && isInvalidTexture(material.emissiveTexture->textureIndex))
			return Error::InvalidGltf;
		if (material.occlusionTexture.has_value() && isInvalidTexture(material.occlusionTexture->textureIndex))
			return Error::InvalidGltf;
		if (material.pbrData.baseColorTexture.has_value() &&
		    isInvalidTexture(material.pbrData.baseColorTexture->textureIndex))
			return Error::InvalidGltf;
		if (material.pbrData.metallicRoughnessTexture.has_value() &&
		    isInvalidTexture(material.pbrData.metallicRoughnessTexture->textureIndex))
			return Error::InvalidGltf;

		// Validate that for every additional material field from an extension the correct extension is marked as used by the asset.
		if (material.anisotropy && !isExtensionUsed(extensions::KHR_materials_anisotropy))
			return Error::InvalidGltf;
		if (material.clearcoat && !isExtensionUsed(extensions::KHR_materials_clearcoat))
			return Error::InvalidGltf;
		if (material.iridescence && !isExtensionUsed(extensions::KHR_materials_iridescence))
			return Error::InvalidGltf;
		if (material.sheen && !isExtensionUsed(extensions::KHR_materials_sheen))
			return Error::InvalidGltf;
		if (material.specular && !isExtensionUsed(extensions::KHR_materials_specular))
			return Error::InvalidGltf;
#if FASTGLTF_ENABLE_DEPRECATED_EXT
		if (material.specularGlossiness && !isExtensionUsed(extensions::KHR_materials_pbrSpecularGlossiness))
			return Error::InvalidGltf;
#endif
		if (material.transmission && !isExtensionUsed(extensions::KHR_materials_transmission))
			return Error::InvalidGltf;
		if (material.volume && !isExtensionUsed(extensions::KHR_materials_volume))
			return Error::InvalidGltf;
		if (material.emissiveStrength != 1.0f && !isExtensionUsed(extensions::KHR_materials_emissive_strength))
			return Error::InvalidGltf;
		if (material.ior != 1.5f && !isExtensionUsed(extensions::KHR_materials_ior))
			return Error::InvalidGltf;
		if (material.packedNormalMetallicRoughnessTexture && !isExtensionUsed(extensions::MSFT_packing_normalRoughnessMetallic))
			return Error::InvalidGltf;
		if (material.packedOcclusionRoughnessMetallicTextures && !isExtensionUsed(extensions::MSFT_packing_occlusionRoughnessMetallic))
			return Error::InvalidGltf;
	}

	for (const auto& mesh : asset.meshes) {
		for (const auto& primitives : mesh.primitives) {
			for (auto [name, index] : primitives.attributes) {
				if (asset.accessors.size() <= index)
					return Error::InvalidGltf;

				// The spec provides a list of attributes that it accepts and mentions that all
				// custom attributes have to start with an underscore. We'll enforce this.
				if (!startsWith(name, "_")) {
					if (name != "POSITION" && name != "NORMAL" && name != "TANGENT" &&
					    !startsWith(name, "TEXCOORD_") && !startsWith(name, "COLOR_") &&
					    !startsWith(name, "JOINTS_") && !startsWith(name, "WEIGHTS_")) {
						return Error::InvalidGltf;
					}
				}

				// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview
				const auto& accessor = asset.accessors[index];
				if (name == "POSITION") {
					// Animation input and vertex position attribute accessors MUST have accessor.min and accessor.max defined.
					if (std::holds_alternative<std::monostate>(accessor.max) || std::holds_alternative<std::monostate>(accessor.min))
						return Error::InvalidGltf;
					if (accessor.type != AccessorType::Vec3)
						return Error::InvalidGltf;
					if (!isExtensionUsed(extensions::KHR_mesh_quantization)) {
						if (accessor.componentType != ComponentType::Float)
							return Error::InvalidGltf;
					} else {
						if (accessor.componentType == ComponentType::Double || accessor.componentType == ComponentType::UnsignedInt)
							return Error::InvalidGltf;
					}
				} else if (name == "NORMAL") {
					if (accessor.type != AccessorType::Vec3)
						return Error::InvalidGltf;
					if (!isExtensionUsed(extensions::KHR_mesh_quantization)) {
						if (accessor.componentType != ComponentType::Float)
							return Error::InvalidGltf;
					} else {
						if (accessor.componentType != ComponentType::Float &&
						    accessor.componentType != ComponentType::Short &&
						    accessor.componentType != ComponentType::Byte)
							return Error::InvalidGltf;
					}
				} else if (name == "TANGENT") {
					if (accessor.type != AccessorType::Vec4)
						return Error::InvalidGltf;
					if (!isExtensionUsed(extensions::KHR_mesh_quantization)) {
						if (accessor.componentType != ComponentType::Float)
							return Error::InvalidGltf;
					} else {
						if (accessor.componentType != ComponentType::Float &&
						    accessor.componentType != ComponentType::Short &&
						    accessor.componentType != ComponentType::Byte)
							return Error::InvalidGltf;
					}
				} else if (startsWith(name, "TEXCOORD_")) {
					if (accessor.type != AccessorType::Vec2)
						return Error::InvalidGltf;
					if (!isExtensionUsed(extensions::KHR_mesh_quantization)) {
						if (accessor.componentType != ComponentType::Float &&
						    accessor.componentType != ComponentType::UnsignedByte &&
						    accessor.componentType != ComponentType::UnsignedShort) {
							return Error::InvalidGltf;
						}
					} else {
						if (accessor.componentType == ComponentType::Double ||
						    accessor.componentType == ComponentType::UnsignedInt) {
							return Error::InvalidGltf;
						}
					}
				} else if (startsWith(name, "COLOR_")) {
					if (accessor.type != AccessorType::Vec3 && accessor.type != AccessorType::Vec4)
						return Error::InvalidGltf;
					if (accessor.componentType != ComponentType::Float &&
					    accessor.componentType != ComponentType::UnsignedByte &&
					    accessor.componentType != ComponentType::UnsignedShort) {
						return Error::InvalidGltf;
					}
				} else if (startsWith(name, "JOINTS_")) {
					if (accessor.type != AccessorType::Vec4)
						return Error::InvalidGltf;
					if (accessor.componentType != ComponentType::UnsignedByte &&
					    accessor.componentType != ComponentType::UnsignedShort) {
						return Error::InvalidGltf;
					}
				} else if (startsWith(name, "WEIGHTS_")) {
					if (accessor.type != AccessorType::Vec4)
						return Error::InvalidGltf;
					if (accessor.componentType != ComponentType::Float &&
					    accessor.componentType != ComponentType::UnsignedByte &&
					    accessor.componentType != ComponentType::UnsignedShort) {
						return Error::InvalidGltf;
					}
				} else if (startsWith(name, "_")) {
					// Application-specific attribute semantics MUST start with an underscore, e.g., _TEMPERATURE.
					// Application-specific attribute semantics MUST NOT use unsigned int component type.
					if (accessor.componentType == ComponentType::UnsignedInt) {
						return Error::InvalidGltf;
					}
				}
			}
		}
	}

	for (const auto& node : asset.nodes) {
		if (node.cameraIndex.has_value() && asset.cameras.size() <= node.cameraIndex.value())
			return Error::InvalidGltf;
		if (node.skinIndex.has_value() && asset.skins.size() <= node.skinIndex.value())
			return Error::InvalidGltf;
		if (node.meshIndex.has_value() && asset.meshes.size() <= node.meshIndex.value())
			return Error::InvalidGltf;

		if (const auto* pTRS = std::get_if<TRS>(&node.transform)) {
			for (const auto& x : pTRS->rotation)
				if (x > 1.0 || x < -1.0)
					return Error::InvalidGltf;
		}

		if ((node.skinIndex.has_value() || !node.weights.empty()) && !node.meshIndex.has_value()) {
			return Error::InvalidGltf;
		}

		if (node.skinIndex.has_value()) {
			// "When the node contains skin, all mesh.primitives MUST contain JOINTS_0 and WEIGHTS_0 attributes."
			const auto& mesh = asset.meshes[node.meshIndex.value()];
			for (const auto& primitive : mesh.primitives) {
				const auto* joints0 = primitive.findAttribute("JOINTS_0");
				const auto* weights0 = primitive.findAttribute("WEIGHTS_0");
				if (joints0 == primitive.attributes.end() || weights0 == primitive.attributes.end())
					return Error::InvalidGltf;
			}
		}
	}

	for (const auto& sampler : asset.samplers) {
		if (sampler.magFilter.has_value() && (sampler.magFilter != Filter::Nearest && sampler.magFilter != Filter::Linear)) {
			return Error::InvalidGltf;
		}
	}

	for (const auto& scene : asset.scenes) {
		for (const auto& node : scene.nodeIndices) {
			if (node >= asset.nodes.size())
				return Error::InvalidGltf;
		}
	}

	for (const auto& skin : asset.skins) {
		if (skin.joints.empty())
			return Error::InvalidGltf;
		if (skin.skeleton.has_value() && skin.skeleton.value() >= asset.nodes.size())
			return Error::InvalidGltf;
		if (skin.inverseBindMatrices.has_value() && skin.inverseBindMatrices.value() >= asset.accessors.size())
			return Error::InvalidGltf;
	}

	for (const auto& texture : asset.textures) {
		if (texture.samplerIndex.has_value() && texture.samplerIndex.value() >= asset.samplers.size())
			return Error::InvalidGltf;
		// imageIndex needs to be defined, unless one of the texture extensions were enabled and define another image index.
		if (isExtensionUsed(extensions::KHR_texture_basisu) || isExtensionUsed(extensions::MSFT_texture_dds) || isExtensionUsed(extensions::EXT_texture_webp)) {
			if (!texture.imageIndex.has_value() && (!texture.basisuImageIndex.has_value() && !texture.ddsImageIndex.has_value() && !texture.webpImageIndex.has_value())) {
				return Error::InvalidGltf;
			}
		} else if (!texture.imageIndex.has_value()) {
			return Error::InvalidGltf;
		}
		if (texture.imageIndex.has_value() && texture.imageIndex.value() >= asset.images.size())
			return Error::InvalidGltf;
		if (texture.basisuImageIndex.has_value() && texture.basisuImageIndex.value() >= asset.images.size())
			return Error::InvalidGltf;
		if (texture.ddsImageIndex.has_value() && texture.ddsImageIndex.value() >= asset.images.size())
			return Error::InvalidGltf;
		if (texture.webpImageIndex.has_value() && texture.webpImageIndex.value() >= asset.images.size())
			return Error::InvalidGltf;
	}

	return Error::None;
}

fg::Expected<fg::Asset> fg::Parser::parse(simdjson::ondemand::object root, Category categories) {
	using namespace simdjson;
	fillCategories(categories);

	Asset asset {};

#if !FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL
	// Create a new chunk memory resource for each asset we parse.
	asset.memoryResource = resourceAllocator = std::make_shared<ChunkMemoryResource>();
#endif

	bool hasAsset = false; // Asset is the only category required by the spec
	Category readCategories = Category::None;
	for (auto object : root) {
        std::string_view key;
        if (object.unescaped_key().get(key) != SUCCESS) {
            return Expected<Asset>(Error::InvalidJson);
        }

		auto hashedKey = crcStringFunction(key);

        switch (hashedKey) {
            case force_consteval<crc32c("scene")>: {
                std::uint64_t defaultScene;
                if (object.value().get(defaultScene) != SUCCESS) FASTGLTF_UNLIKELY {
                    return Expected<Asset>(Error::InvalidGltf);
                }
                asset.defaultScene = static_cast<std::size_t>(defaultScene);
                continue;
            }
            case force_consteval<crc32c("extensions")>: {
                ondemand::object extensionsObject;
                if (object.value().get(extensionsObject) != SUCCESS) FASTGLTF_UNLIKELY {
                    return Expected<Asset>(Error::InvalidGltf);
                }

                if (auto error = parseExtensions(extensionsObject, asset); error != Error::None)
                    return Expected<Asset>(error);
                continue;
            }
            case force_consteval<crc32c("asset")>: {
				hasAsset = true;

                if (hasBit(options, Options::DontRequireValidAssetMember)) {
                    continue;
                }

                ondemand::object assetObject;
                if (object.value().get(assetObject) != SUCCESS) FASTGLTF_UNLIKELY {
                    return Expected<Asset>(Error::InvalidJson);
                }

                AssetInfo info = {};
                std::string_view version;
                if (assetObject["version"].get_string().get(version) != SUCCESS) FASTGLTF_UNLIKELY {
                    return Expected<Asset>(Error::InvalidOrMissingAssetField);
                } else {
                    uint32_t major = static_cast<uint32_t>(version.substr(0, 1)[0] - '0');
                    // uint32_t minor = version.substr(2, 3)[0] - '0';
                    if (major != 2) {
                        return Expected<Asset>(Error::UnsupportedVersion);
                    }
                }
                info.gltfVersion = std::string { version };

                std::string_view copyright;
                if (assetObject["copyright"].get_string().get(copyright) == SUCCESS) FASTGLTF_LIKELY {
                    info.copyright = std::string { copyright };
                }

                std::string_view generator;
                if (assetObject["generator"].get_string().get(generator) == SUCCESS) FASTGLTF_LIKELY {
                    info.generator = std::string { generator };
                }

                asset.assetInfo = std::move(info);
                readCategories |= Category::Asset;
                continue;
            }
            case force_consteval<crc32c("extras")>:
                continue;
            default:
                break;
        }

		ondemand::array array;
		if (object.value().get_array().get(array) != SUCCESS) FASTGLTF_UNLIKELY {
			return Expected<Asset>(Error::InvalidGltf);
		}

#define KEY_SWITCH_CASE(name, id) case force_consteval<crc32c(FASTGLTF_QUOTE(id))>:       \
                if (hasBit(categories, Category::name))   \
                    error = parse##name(array, asset);                     \
                readCategories |= Category::name;         \
                break;

		Error error = Error::None;
		switch (hashedKey) {
			KEY_SWITCH_CASE(Accessors, accessors)
			KEY_SWITCH_CASE(Animations, animations)
			KEY_SWITCH_CASE(Buffers, buffers)
			KEY_SWITCH_CASE(BufferViews, bufferViews)
			KEY_SWITCH_CASE(Cameras, cameras)
			KEY_SWITCH_CASE(Images, images)
			KEY_SWITCH_CASE(Materials, materials)
			KEY_SWITCH_CASE(Meshes, meshes)
			KEY_SWITCH_CASE(Nodes, nodes)
			KEY_SWITCH_CASE(Samplers, samplers)
			KEY_SWITCH_CASE(Scenes, scenes)
			KEY_SWITCH_CASE(Skins, skins)
			KEY_SWITCH_CASE(Textures, textures)
			case force_consteval<crc32c("extensionsUsed")>: {
				for (auto usedValue : array) {
					std::string_view usedString;
					if (auto eError = usedValue.get_string().get(usedString); eError == SUCCESS) FASTGLTF_LIKELY {
						FASTGLTF_STD_PMR_NS::string FASTGLTF_CONSTRUCT_PMR_RESOURCE(string, resourceAllocator.get(), usedString);
						asset.extensionsUsed.emplace_back(std::move(string));
					} else {
                        return Expected<Asset>(Error::InvalidGltf);
					}
				}
				break;
			}
			case force_consteval<crc32c("extensionsRequired")>: {
                for (auto extension : array) {
                    std::string_view string;
                    if (extension.get_string().get(string) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Expected<Asset>(Error::InvalidGltf);
                    }

                    bool known = false;
                    for (const auto& [extensionString, extensionEnum] : extensionStrings) {
                        if (extensionString == string) {
                            known = true;
                            if (!hasBit(config.extensions, extensionEnum)) {
                                // The extension is required, but not enabled by the user.
                                return Expected<Asset>(Error::MissingExtensions);
                            }
                            break;
                        }
                    }
                    if (!known) FASTGLTF_UNLIKELY {
                        return Expected<Asset>(Error::UnknownRequiredExtension);
                    }

                    FASTGLTF_STD_PMR_NS::string FASTGLTF_CONSTRUCT_PMR_RESOURCE(requiredExtension, resourceAllocator.get(), string);
                    asset.extensionsRequired.emplace_back(std::move(requiredExtension));
                }
				break;
			}
			default:
				break;
		}

		if (error != Error::None)
			return Expected<Asset>(error);

#undef KEY_SWITCH_CASE
	}

	if (!hasAsset && !hasBit(options, Options::DontRequireValidAssetMember)) {
		return Expected<Asset>(Error::InvalidOrMissingAssetField);
	}

	asset.availableCategories = readCategories;

	if (hasBit(options, Options::GenerateMeshIndices)) {
		if (auto error = generateMeshIndices(asset); error != Error::None) {
			return Expected<Asset>(error);
		}
	}

	return Expected(std::move(asset));
}

fg::Error fg::Parser::parseAccessors(simdjson::ondemand::array& accessors, Asset& asset) {
    using namespace simdjson;

	// asset.accessors.reserve(accessors.size());
    for (auto accessorValue : accessors) {
        ondemand::object accessorObject;
        if (accessorValue.get_object().get(accessorObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& accessor = asset.accessors.emplace_back();
        accessor.byteOffset = 0UL;
        accessor.normalized = false;

		// Function for parsing the min and max arrays for accessors
        auto parseMinMax = [&](simdjson::ondemand::array& array, decltype(Accessor::max)& ref) -> fastgltf::Error {
            decltype(Accessor::max) variant;

            using double_vec = std::variant_alternative_t<1, decltype(Accessor::max)>;
            using int64_vec = std::variant_alternative_t<2, decltype(Accessor::max)>;

            // It's possible the min/max fields come before the accessor type is declared, in which case we don't know the size.
			// This single line is actually incredibly important as this function without it takes up roughly 15% of the entire
			// parsing process on average due to vector resizing.
			// When no exact count is known (accessor type field comes after min/max fields) we'll just count them and take the penalty.
            auto initialCount = accessor.type == AccessorType::Invalid ? array.count_elements().value() : getNumComponents(accessor.type);
            if (accessor.componentType == ComponentType::Float || accessor.componentType == ComponentType::Double) {
                auto vec = FASTGLTF_CONSTRUCT_PMR_RESOURCE(double_vec, resourceAllocator.get(), 0);
				vec.reserve(initialCount);
				variant = std::move(vec);
            } else {
                auto vec = FASTGLTF_CONSTRUCT_PMR_RESOURCE(int64_vec, resourceAllocator.get(), 0);
				vec.reserve(initialCount);
				variant = std::move(vec);
            }

			for (auto element : array) {
				ondemand::number num;
				if (auto error = element.get_number().get(num); error != SUCCESS) FASTGLTF_UNLIKELY {
					return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
				}

				switch (num.get_number_type()) {
					case ondemand::number_type::floating_point_number: {
						// We can't safely promote double to ints. Therefore, if the element is a double,
						// but our component type is not a floating point, that's invalid.
						if (accessor.componentType != ComponentType::Float && accessor.componentType != ComponentType::Double) {
							return Error::InvalidGltf;
						}

						if (!std::holds_alternative<double_vec>(variant)) FASTGLTF_UNLIKELY {
							return Error::InvalidGltf;
						}
						std::get<double_vec>(variant).emplace_back(num.get_double());
						break;
					}
					case ondemand::number_type::signed_integer: {
						std::int64_t value = num.get_int64();

						if (auto* doubles = std::get_if<double_vec>(&variant); doubles != nullptr) {
							(*doubles).emplace_back(static_cast<double>(value));
						} else if (auto* ints = std::get_if<int64_vec>(&variant); ints != nullptr) {
							(*ints).emplace_back(static_cast<std::int64_t>(value));
						} else {
							return Error::InvalidGltf;
						}
						break;
					}
					case ondemand::number_type::unsigned_integer: {
						// Note that the glTF spec doesn't care about any integer larger than 32-bits, so
						// truncating uint64 to int64 wouldn't make any difference, as those large values
						// aren't allowed anyway.
						std::uint64_t value = num.get_uint64();

						if (auto* doubles = std::get_if<double_vec>(&variant); doubles != nullptr) {
							(*doubles).emplace_back(static_cast<double>(value));
						} else if (auto* ints = std::get_if<int64_vec>(&variant); ints != nullptr) {
							(*ints).emplace_back(static_cast<std::int64_t>(value));
						} else {
							return Error::InvalidGltf;
						}
						break;
					}
				}
			}

            ref = std::move(variant);
            return Error::None;
        };

        for (auto field : accessorObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidJson;
            }

            auto hashedKey = crcStringFunction(key);
            switch (hashedKey) {
                case force_consteval<crc32c("bufferView")>: {
                    std::uint64_t bufferView;
                    if (auto error = field.value().get(bufferView); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    accessor.bufferViewIndex = static_cast<std::size_t>(bufferView);
                    break;
                }
                case force_consteval<crc32c("byteOffset")>:
                    std::uint64_t byteOffset;
                    if (auto error = field.value().get(byteOffset); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    } else {
                        accessor.byteOffset = static_cast<std::size_t>(byteOffset);
                    }
                    break;
                case force_consteval<crc32c("componentType")>:
                    std::uint64_t componentType;
                    if (auto error = field.value().get(componentType); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    accessor.componentType = getComponentType(
                            static_cast<std::underlying_type_t<ComponentType>>(componentType));
                    if (accessor.componentType == ComponentType::Double && !hasBit(options, Options::AllowDouble)) {
                        return Error::InvalidGltf;
                    }
                    break;
                case force_consteval<crc32c("normalized")>:
                    if (field.value().get_bool().get(accessor.normalized) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    break;
                case force_consteval<crc32c("count")>:
                    std::uint64_t accessorCount;
                    if (field.value().get(accessorCount) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    accessor.count = static_cast<std::size_t>(accessorCount);
                    break;
                case force_consteval<crc32c("type")>: {
                    std::string_view accessorType;
                    if (field.value().get_string().get(accessorType) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    accessor.type = getAccessorType(accessorType);
                    break;
                }
                case force_consteval<crc32c("max")>: {
                    ondemand::array array;
                    if (auto error = field.value().get_array().get(array); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    parseMinMax(array, accessor.max);
                    break;
                }
                case force_consteval<crc32c("min")>: {
                    ondemand::array array;
                    if (auto error = field.value().get_array().get(array); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    parseMinMax(array, accessor.min);
                    break;
                }
                case force_consteval<crc32c("sparse")>: {
                    ondemand::object sparseAccessorObject;
                    if (field.value().get_object().get(sparseAccessorObject) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    SparseAccessor& sparse = accessor.sparse.emplace();
                    std::uint64_t value;
                    ondemand::object child;
                    if (sparseAccessorObject["count"].get(value) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    sparse.count = static_cast<size_t>(value);

                    // Accessor Sparce Indices
                    if (sparseAccessorObject["indices"].get_object().get(child) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    if (child["bufferView"].get(value) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    sparse.indicesBufferView = static_cast<std::size_t>(value);

                    if (auto error = child["byteOffset"].get(value); error == SUCCESS) FASTGLTF_LIKELY {
                        sparse.indicesByteOffset = static_cast<std::size_t>(value);
                    } else if (error != NO_SUCH_FIELD) {
                        return Error::InvalidGltf;
                    }

                    if (child["componentType"].get(value) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    sparse.indexComponentType = getComponentType(static_cast<std::underlying_type_t<ComponentType>>(value));

                    // Accessor Sparse Values
                    if (sparseAccessorObject["values"].get_object().get(child) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    if (child["bufferView"].get(value) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    sparse.valuesBufferView = static_cast<std::size_t>(value);

                    if (auto error = child["byteOffset"].get(value); error == SUCCESS) FASTGLTF_LIKELY {
                        sparse.valuesByteOffset = static_cast<std::size_t>(value);
                    } else if (error != NO_SUCH_FIELD) {
                        return Error::InvalidGltf;
                    }

                    accessor.sparse = sparse;
                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    accessor.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(accessor.name), resourceAllocator.get(), name);
                    break;
                }
                default:
                    // Do we want to allow custom JSON fields?
                    return Error::InvalidGltf;
            }
        }

        if (accessor.componentType == ComponentType::Invalid || accessor.type == AccessorType::Invalid || accessor.count < 1) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseAnimations(simdjson::ondemand::array& animations, Asset& asset) {
    using namespace simdjson;

	// asset.animations.reserve(animations.size());
    for (auto animationValue : animations) {
        ondemand::object animationObject;
        if (animationValue.get_object().get(animationObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& animation = asset.animations.emplace_back();

        for (auto field : animationObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidJson;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("channels")>: {
                    ondemand::array channels;
                    if (auto channelError = field.value().get_array().get(channels); channelError == NO_SUCH_FIELD || channelError == INCORRECT_TYPE) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    } else if (channelError != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidJson;
                    }

                    animation.channels = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(animation.channels), resourceAllocator.get(), 0);
                    // animation.channels.reserve(channels.size());
                    for (auto channelValue : channels) {
                        ondemand::object channelObject;
                        AnimationChannel channel = {};
                        if (channelValue.get_object().get(channelObject) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidGltf;
                        }

                        std::uint64_t sampler;
                        if (channelObject["sampler"].get(sampler) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidGltf;
                        }
                        channel.samplerIndex = static_cast<std::size_t>(sampler);

                        ondemand::object targetObject;
                        if (channelObject["target"].get_object().get(targetObject) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidGltf;
                        } else {
                            std::uint64_t node;
                            if (targetObject["node"].get(node) != SUCCESS) FASTGLTF_UNLIKELY {
                                // We don't support any extensions for animations, so it is required.
                                return Error::InvalidGltf;
                            }
                            channel.nodeIndex = static_cast<std::size_t>(node);

                            std::string_view path;
                            if (targetObject["path"].get_string().get(path) != SUCCESS) FASTGLTF_UNLIKELY {
                                return Error::InvalidGltf;
                            }

                            auto hashedPath = crcStringFunction(path);
                            if (hashedPath == force_consteval<crc32c("translation")>) {
                                channel.path = AnimationPath::Translation;
                            } else if (hashedPath == force_consteval<crc32c("rotation")>) {
                                channel.path = AnimationPath::Rotation;
                            } else if (hashedPath == force_consteval<crc32c("scale")>) {
                                channel.path = AnimationPath::Scale;
                            } else if (hashedPath == force_consteval<crc32c("weights")>) {
                                channel.path = AnimationPath::Weights;
                            }
                        }

                        animation.channels.emplace_back(channel);
                    }
                    break;
                }
                case force_consteval<crc32c("samplers")>: {
                    ondemand::array samplers;
                    if (auto samplerError = field.value().get_array().get(samplers); samplerError == NO_SUCH_FIELD || samplerError == INCORRECT_TYPE) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    } else if (samplerError != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidJson;
                    }

                    animation.samplers = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(animation.samplers), resourceAllocator.get(), 0);
                    // animation.samplers.reserve(samplers.size());
                    for (auto samplerValue : samplers) {
                        ondemand::object samplerObject;
                        AnimationSampler sampler = {};
                        if (samplerValue.get_object().get(samplerObject) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidGltf;
                        }

                        std::uint64_t input;
                        if (samplerObject["input"].get(input) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidGltf;
                        }
                        sampler.inputAccessor = static_cast<std::size_t>(input);

                        std::uint64_t output;
                        if (samplerObject["output"].get(output) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidGltf;
                        }
                        sampler.outputAccessor = static_cast<std::size_t>(output);

                        std::string_view interpolation;
                        if (samplerObject["interpolation"].get_string().get(interpolation) != SUCCESS) FASTGLTF_UNLIKELY {
                            sampler.interpolation = AnimationInterpolation::Linear;
                        } else {
                            if (interpolation == "LINEAR") {
                                sampler.interpolation = AnimationInterpolation::Linear;
                            } else if (interpolation == "STEP") {
                                sampler.interpolation = AnimationInterpolation::Step;
                            } else if (interpolation == "CUBICSPLINE") {
                                sampler.interpolation = AnimationInterpolation::CubicSpline;
                            } else {
                                return Error::InvalidGltf;
                            }
                        }

                        animation.samplers.emplace_back(sampler);
                    }

                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    animation.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(animation.name), resourceAllocator.get(), name);
                    break;
                }
            }
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseBuffers(simdjson::ondemand::array& buffers, Asset& asset) {
    using namespace simdjson;

    // The spec for EXT_meshopt_compression allows so-called 'fallback buffers' which only exist to
    // act as a valid fallback for compressed buffer views, but actually do not contain any data.
    // In these cases, there is either simply no URI, or a fallback boolean is added to the extensions'
    // extension field.
    // In these cases, fastgltf could just leave the std::monostate in the DataSource.
    // However, to make the actual use of these buffers clear, we'll use an empty fallback type.
    bool meshoptCompressionRequired = false;
    for (const auto& extension : asset.extensionsRequired) {
        if (extension == extensions::EXT_meshopt_compression) {
            meshoptCompressionRequired = true;
        }
    }

    // asset.buffers.reserve(buffers.size());
    std::size_t bufferIndex = 0;
    for (auto bufferValue : buffers) {
        // Required fields: "byteLength"
        ondemand::object bufferObject;
        if (bufferValue.get_object().get(bufferObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& buffer = asset.buffers.emplace_back();

		bool hasURI = false;
        for (auto field : bufferObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidJson;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("uri")>: {
                    std::string_view _uri;
                    if (auto error = field.value().get_string().get(_uri); error != SUCCESS || _uri.empty()) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

					URIView uriView(_uri);
					if (!uriView.valid()) {
						return Error::InvalidGltf;
					}

					if (uriView.isDataUri()) {
						auto expected = decodeDataUri(uriView);
						if (expected.error() != Error::None) {
							return expected.error();
						}

						buffer.data = std::move(expected.get());
					} else if (uriView.isLocalPath() && hasBit(options, Options::LoadExternalBuffers)) {
						auto [error, source] = loadFileFromUri(uriView);
						if (error != Error::None) {
							return error;
						}

						buffer.data = std::move(source);
					} else {
						sources::URI filePath;
						filePath.fileByteOffset = 0;
						filePath.uri = std::move(URI(_uri));
						buffer.data = std::move(filePath);
					}
					hasURI = true;
                    break;
                }
                case force_consteval<crc32c("byteLength")>: {
                    std::uint64_t byteLength;
                    if (field.value().get(byteLength) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    } else {
                        buffer.byteLength = static_cast<std::size_t>(byteLength);
                    }
                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    buffer.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(buffer.name), resourceAllocator.get(), name);
                    break;
                }
            }
        }

		if (!hasURI) {
			if (bufferIndex == 0 && !std::holds_alternative<std::monostate>(glbBuffer)) {
				// The first buffer in a GLB points to the embedded buffer when no URI is specified.
				buffer.data = std::move(glbBuffer);
			} else if (meshoptCompressionRequired) {
				// This buffer is not a GLB buffer and has no URI source and is therefore a fallback.
				buffer.data = sources::Fallback();
			} else FASTGLTF_UNLIKELY {
				// All other buffers have to contain an uri field.
				return Error::InvalidGltf;
			}
		}

        if (std::holds_alternative<std::monostate>(buffer.data)) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        ++bufferIndex;
    }

	return Error::None;
}

fg::Error fg::Parser::parseBufferViews(simdjson::ondemand::array& bufferViews, Asset& asset) {
    using namespace simdjson;

	// sset.bufferViews.reserve(bufferViews.size());
    for (auto bufferViewValue : bufferViews) {
        ondemand::object bufferViewObject;
        if (bufferViewValue.get_object().get(bufferViewObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& view = asset.bufferViews.emplace_back();
        view.byteOffset = 0UL;
        for (auto field : bufferViewObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidJson;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("buffer")>: {
                    std::uint64_t buffer;
                    if (auto error = field.value().get(buffer); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    view.bufferIndex = static_cast<std::size_t>(buffer);
                    break;
                }
                case force_consteval<crc32c("byteOffset")>: {
                    std::uint64_t byteOffset;
                    if (auto error = field.value().get(byteOffset); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    view.byteOffset = static_cast<std::size_t>(byteOffset);
                    break;
                }
                case force_consteval<crc32c("byteLength")>: {
                    std::uint64_t byteLength;
                    if (auto error = field.value().get(byteLength); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    view.byteLength = static_cast<std::size_t>(byteLength);
                    break;
                }
                case force_consteval<crc32c("byteStride")>: {
                    std::uint64_t byteStride;
                    if (auto error = field.value().get(byteStride); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    view.byteStride = static_cast<std::size_t>(byteStride);
                    break;
                }
                case force_consteval<crc32c("target")>: {
                    std::uint64_t target;
                    if (auto error = field.value().get(target); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    view.target = static_cast<BufferTarget>(target);
                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    view.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(view.name), resourceAllocator.get(), name);
                    break;
                }
                case force_consteval<crc32c("extensions")>: {
                    auto extensionsObject = field.value();

                    ondemand::object meshoptCompression;
                    if (hasBit(config.extensions, Extensions::EXT_meshopt_compression) && extensionsObject[extensions::EXT_meshopt_compression].get_object().get(meshoptCompression) == SUCCESS) {
                        auto compression = std::make_unique<CompressedBufferView>();

                        std::uint64_t number;
                        if (auto error = meshoptCompression["buffer"].get(number); error != SUCCESS) FASTGLTF_UNLIKELY {
                            return error == NO_SUCH_FIELD ? Error::InvalidGltf : Error::InvalidJson;
                        }
                        compression->bufferIndex = static_cast<std::size_t>(number);

                        if (auto error = meshoptCompression["byteOffset"].get(number); error == SUCCESS) FASTGLTF_LIKELY {
                            compression->byteOffset = static_cast<std::size_t>(number);
                        } else if (error != NO_SUCH_FIELD) {
                            return Error::InvalidJson;
                        }

                        if (auto error = meshoptCompression["byteLength"].get(number); error != SUCCESS) FASTGLTF_UNLIKELY {
                            return error == NO_SUCH_FIELD ? Error::InvalidGltf : Error::InvalidJson;
                        }
                        compression->byteLength = static_cast<std::size_t>(number);

                        if (auto error = meshoptCompression["byteStride"].get(number); error != SUCCESS) FASTGLTF_UNLIKELY {
                            return error == NO_SUCH_FIELD ? Error::InvalidGltf : Error::InvalidJson;
                        }
                        compression->byteStride = static_cast<std::size_t>(number);

                        if (auto error = meshoptCompression["count"].get(number); error != SUCCESS) FASTGLTF_UNLIKELY {
                            return error == NO_SUCH_FIELD ? Error::InvalidGltf : Error::InvalidJson;
                        }
                        compression->count = number;

                        std::string_view string;
                        if (auto error = meshoptCompression["mode"].get_string().get(string); error != SUCCESS)FASTGLTF_UNLIKELY {
                            return error == NO_SUCH_FIELD ? Error::InvalidGltf : Error::InvalidJson;
                        }
                        switch (crcStringFunction(string)) {
                            case force_consteval<crc32c("ATTRIBUTES")>: {
                                compression->mode = MeshoptCompressionMode::Attributes;
                                break;
                            }
                            case force_consteval<crc32c("TRIANGLES")>: {
                                compression->mode = MeshoptCompressionMode::Triangles;
                                break;
                            }
                            case force_consteval<crc32c("INDICES")>: {
                                compression->mode = MeshoptCompressionMode::Indices;
                                break;
                            }
                            default: {
                                return Error::InvalidGltf;
                            }
                        }

                        if (auto error = meshoptCompression["filter"].get_string().get(string); error == SUCCESS) FASTGLTF_LIKELY {
                            switch (crcStringFunction(string)) {
                                case force_consteval<crc32c("NONE")>: {
                                    compression->filter = MeshoptCompressionFilter::None;
                                    break;
                                }
                                case force_consteval<crc32c("OCTAHEDRAL")>: {
                                    compression->filter = MeshoptCompressionFilter::Octahedral;
                                    break;
                                }
                                case force_consteval<crc32c("QUATERNION")>: {
                                    compression->filter = MeshoptCompressionFilter::Quaternion;
                                    break;
                                }
                                case force_consteval<crc32c("EXPONENTIAL")>: {
                                    compression->filter = MeshoptCompressionFilter::Exponential;
                                    break;
                                }
                                default: {
                                    return Error::InvalidGltf;
                                }
                            }
                        } else if (error == NO_SUCH_FIELD) {
                            compression->filter = MeshoptCompressionFilter::None;
                        } else {
                            return Error::InvalidJson;
                        }

                        view.meshoptCompression = std::move(compression);
                    }
                }
            }
        }

        if (view.byteLength == 0) {
            return Error::InvalidGltf;
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseCameras(simdjson::ondemand::array& cameras, Asset& asset) {
    using namespace simdjson;

	// asset.cameras.reserve(cameras.size());
    for (auto cameraValue : cameras) {
        ondemand::object cameraObject;
        if (cameraValue.get_object().get(cameraObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        std::variant<std::monostate, Camera::Perspective, Camera::Orthographic> cameraVal;
        auto& camera = asset.cameras.emplace_back();
        for (auto field : cameraObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidJson;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("orthographic")>: {
                    if (!std::holds_alternative<std::monostate>(cameraVal)) FASTGLTF_UNLIKELY {
                        // Either doubled key, or we already got a perspective object.
                        return Error::InvalidGltf;
                    }

                    ondemand::object orthographicCamera;
                    if (auto error = field.value().get_object().get(orthographicCamera); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }

                    auto& orthographic = cameraVal.emplace<Camera::Orthographic>();
                    for (auto orthographicField : orthographicCamera) {
                        std::string_view orthographicKey;
                        if (orthographicField.unescaped_key().get(orthographicKey) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidJson;
                        }

                        double value;
                        if (auto error = orthographicField.value().get(value); error != SUCCESS) FASTGLTF_UNLIKELY {
                            return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                        }

                        switch (crcStringFunction(orthographicKey)) {
                            case force_consteval<crc32c("xmag")>:
                                orthographic.xmag = static_cast<num>(value);
                                break;
                            case force_consteval<crc32c("ymag")>:
                                orthographic.ymag = static_cast<num>(value);
                                break;
                            case force_consteval<crc32c("zfar")>:
                                orthographic.zfar = static_cast<num>(value);
                                break;
                            case force_consteval<crc32c("znear")>:
                                orthographic.znear = static_cast<num>(value);
                                break;
                        }
                    }

                    if (orthographic.xmag == 0.0f || orthographic.ymag == 0.0f || orthographic.zfar == 0.0f) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    break;
                }
                case force_consteval<crc32c("perspective")>: {
                    if (!std::holds_alternative<std::monostate>(cameraVal)) FASTGLTF_UNLIKELY {
                        // Either doubled key, or we already got a orthographic object.
                        return Error::InvalidGltf;
                    }

                    ondemand::object perspectiveCamera;
                    if (auto error = field.value().get_object().get(perspectiveCamera); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }

                    auto& perspective = cameraVal.emplace<Camera::Perspective>();
                    for (auto perspectiveField : perspectiveCamera) {
                        std::string_view perspectiveKey;
                        if (perspectiveField.unescaped_key().get(perspectiveKey) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidJson;
                        }

                        double value;
                        if (auto error = perspectiveField.value().get(value); error != SUCCESS) FASTGLTF_UNLIKELY {
                            return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                        }

                        switch (crcStringFunction(perspectiveKey)) {
                            case force_consteval<crc32c("aspectRatio")>:
                                perspective.aspectRatio = static_cast<num>(value);
                                break;
                            case force_consteval<crc32c("zfar")>:
                                perspective.zfar = static_cast<num>(value);
                                break;
                            case force_consteval<crc32c("yfov")>:
                                perspective.yfov = static_cast<num>(value);
                                break;
                            case force_consteval<crc32c("znear")>:
                                perspective.znear = static_cast<num>(value);
                                break;
                        }
                    }

                    if (perspective.aspectRatio == 0.0f || perspective.zfar == 0.0f || perspective.yfov == 0.0f || perspective.znear == 0.0f) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    break;
                }
                case force_consteval<crc32c("type")>: {
                    // We don't store this information, as we just differentiate using the std::variant.
                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    camera.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(camera.name), resourceAllocator.get(), name);
                    break;
                }
            }
        }

        if (std::holds_alternative<std::monostate>(cameraVal)) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        } else if (std::holds_alternative<Camera::Orthographic>(cameraVal)) {
            camera.camera = std::get<Camera::Orthographic>(cameraVal);
        } else if (std::holds_alternative<Camera::Perspective>(cameraVal)) {
            camera.camera = std::get<Camera::Perspective>(cameraVal);
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseExtensions(simdjson::ondemand::object& extensionsObject, Asset& asset) {
    using namespace simdjson;

    for (auto extension : extensionsObject) {
        ondemand::object extensionObject;
        if (auto error = extension.value().get_object().get(extensionObject); error != SUCCESS) FASTGLTF_UNLIKELY {
            if (error == INCORRECT_TYPE) {
                continue; // We want to ignore
            }
            return Error::InvalidGltf;
        }

        std::string_view key;
        if (extension.unescaped_key().get(key) != SUCCESS) {
            return Error::InvalidJson;
        }

        auto hash = crcStringFunction(key);
        switch (hash) {
            case force_consteval<crc32c(extensions::KHR_lights_punctual)>: {
                if (!hasBit(config.extensions, Extensions::KHR_lights_punctual))
                    break;

                ondemand::array lightsArray;
                if (auto error = extensionObject["lights"].get_array().get(lightsArray); error == SUCCESS) FASTGLTF_LIKELY {
                    if (auto lightsError = parseLights(lightsArray, asset); lightsError != Error::None)
						return lightsError;
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }
                break;
            }
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseImages(simdjson::ondemand::array& images, Asset& asset) {
    using namespace simdjson;

	// asset.images.reserve(images.size());
    for (auto imageValue : images) {
        ondemand::object imageObject;
        if (imageValue.get_object().get(imageObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& image = asset.images.emplace_back();

        std::variant<std::monostate, std::string, std::size_t> dataSource;
        auto mimeType = MimeType::None;
        for (auto field : imageObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidJson;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("uri")>: {
                    if (!std::holds_alternative<std::monostate>(dataSource)) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    std::string_view _uri;
                    if (auto error = field.value().get_string().get(_uri); error != SUCCESS || _uri.empty()) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    dataSource = std::string(_uri);
                    break;
                }
                case force_consteval<crc32c("mimeType")>: {
                    std::string_view mimeTypeString;
                    if (auto error = field.value().get_string().get(mimeTypeString); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    // TODO: glTF spec says Allowed values: "image/jpeg", "image/png"
                    mimeType = getMimeTypeFromString(mimeTypeString);
                    break;
                }
                case force_consteval<crc32c("bufferView")>: {
                    if (!std::holds_alternative<std::monostate>(dataSource)) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    std::uint64_t bufferView;
                    if (auto error = field.value().get(bufferView); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    dataSource = bufferView;
                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    image.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(image.name), resourceAllocator.get(), name);
                    break;
                }
            }
        }

        auto visitorError = std::visit(visitor {
            [](std::monostate) { return Error::InvalidGltf; },
            [&](std::string& uri) {
                URIView uriView(uri);

                if (!uriView.valid()) {
                    return Error::InvalidURI;
                }

                if (uriView.isDataUri()) {
                    auto [error, source] = decodeDataUri(uriView);
                    if (error != Error::None) {
                        return error;
                    }

                    image.data = std::move(source);
                } else if (uriView.isLocalPath() && hasBit(options, Options::LoadExternalImages)) {
                    auto [error, source] = loadFileFromUri(uriView);
                    if (error != Error::None) {
                        return error;
                    }

                    image.data = std::move(source);
                } else {
                    sources::URI filePath;
                    filePath.fileByteOffset = 0;
                    filePath.uri = uriView;
					filePath.mimeType = MimeType::None;
                    image.data = std::move(filePath);
                }

                std::visit([&](auto& arg) {
                    using T = std::decay_t<decltype(arg)>;

                    // This is kinda cursed
                    if constexpr (is_any<T, sources::CustomBuffer, sources::BufferView, sources::URI, sources::Array, sources::Vector>()) {
						if (arg.mimeType == MimeType::None) {
							arg.mimeType = mimeType;
						}
                    }
                }, image.data);
                return Error::None;
            },
            [&](std::size_t& bufferView) {
                image.data = sources::BufferView {
                    bufferView,
                    mimeType,
                };
                return Error::None;
            },
        }, dataSource);

        if (visitorError != Error::None) {
            return visitorError;
        }

        if (std::holds_alternative<std::monostate>(image.data)) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseLights(simdjson::ondemand::array& lights, Asset& asset) {
    using namespace simdjson;

    // asset.lights.reserve(lights.size());
    for (auto lightValue : lights) {
        ondemand::object lightObject;
        if (lightValue.get_object().get(lightObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& light = asset.lights.emplace_back();
        light.color = {{1.0f, 1.0f, 1.0f}};
        light.intensity = 0.0f;
        for (auto field : lightObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidJson;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("color")>: {
                    ondemand::array colorArray;
                    if (auto error = field.value().get_array().get(colorArray); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    if (auto error = copyNumericalJsonArray<double>(colorArray, light.color); error != Error::None) FASTGLTF_UNLIKELY {
                        return error;
                    }
                    break;
                }
                case force_consteval<crc32c("intensity")>: {
                    double intensity;
                    if (field.value().get(intensity) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    light.intensity = static_cast<num>(intensity);
                    break;
                }
                case force_consteval<crc32c("type")>: {
                    std::string_view type;
                    if (auto error = field.value().get_string().get(type); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    switch (crcStringFunction(type)) {
                        case force_consteval<crc32c("directional")>: {
                            light.type = LightType::Directional;
                            break;
                        }
                        case force_consteval<crc32c("spot")>: {
                            light.type = LightType::Spot;
                            break;
                        }
                        case force_consteval<crc32c("point")>: {
                            light.type = LightType::Point;
                            break;
                        }
                        default: {
                            return Error::InvalidGltf;
                        }
                    }
                    break;
                }
                case force_consteval<crc32c("range")>: {
                    double range;
                    if (field.value().get(range) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    light.range = static_cast<num>(range);
                    break;
                }
                case force_consteval<crc32c("spot")>: {
                    // TODO: Check if light.type == LightType::Spot?
                    ondemand::object spotObject;
                    if (field.value().get_object().get(spotObject) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    // We'll use the traditional lookup here as its only 2 possible fields.
                    double innerConeAngle;
                    if (auto error = spotObject["innerConeAngle"].get(innerConeAngle); error == SUCCESS) FASTGLTF_LIKELY {
                        light.innerConeAngle = static_cast<num>(innerConeAngle);
                    } else if (error == NO_SUCH_FIELD) {
                        light.innerConeAngle = 0.0f;
                    } else {
                        return Error::InvalidGltf;
                    }

                    double outerConeAngle;
                    if (auto error = spotObject["outerConeAngle"].get(outerConeAngle); error == SUCCESS) FASTGLTF_LIKELY {
                        light.outerConeAngle = static_cast<num>(outerConeAngle);
                    } else if (error == NO_SUCH_FIELD) {
                        static constexpr double pi = 3.141592653589793116;
                        light.outerConeAngle = static_cast<num>(pi / 4.0);
                    } else {
                        return Error::InvalidGltf;
                    }
                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    light.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(light.name), resourceAllocator.get(), name);
                    break;
                }
            }
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseMaterialExtensions(simdjson::ondemand::object& object, Material& material) {
    using namespace simdjson;

    for (auto extensionField : object) {
        std::string_view key;
        if (extensionField.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidJson;
        }

        switch (crcStringFunction(key)) {
            case force_consteval<crc32c(extensions::KHR_materials_anisotropy)>: {
                if (!hasBit(config.extensions, Extensions::KHR_materials_anisotropy))
                    break;

                ondemand::object anisotropyObject;
                auto anisotropyError = extensionField.value().get_object().get(anisotropyObject);
                if (anisotropyError != SUCCESS) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }
                
                auto anisotropy = std::make_unique<MaterialAnisotropy>();

                double anisotropyStrength;
                if (auto error = anisotropyObject["anisotropyStrength"].get_double().get(anisotropyStrength); error == SUCCESS) {
                    anisotropy->anisotropyStrength = static_cast<num>(anisotropyStrength);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidJson;
                }

                double anisotropyRotation;
                if (auto error = anisotropyObject["anisotropyRotation"].get_double().get(anisotropyRotation); error == SUCCESS) {
                    anisotropy->anisotropyRotation = static_cast<num>(anisotropyRotation);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidJson;
                }

                ondemand::object anisotropyTextureObject;
                if (auto error = anisotropyObject["anisotropyTexture"].get_object().get(anisotropyTextureObject); error == SUCCESS) {
                    TextureInfo anisotropyTexture;
                    if (auto parseError = parseTextureInfo(anisotropyTextureObject, &anisotropyTexture, config.extensions); parseError == Error::None) {
                        anisotropy->anisotropyTexture = std::move(anisotropyTexture);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) {
                    return Error::InvalidGltf;
                }
                break;
            }
            case force_consteval<crc32c(extensions::KHR_materials_clearcoat)>: {
                if (!hasBit(config.extensions, Extensions::KHR_materials_clearcoat))
                    break;

                ondemand::object clearcoatObject;
                auto clearcoatError = extensionField.value().get_object().get(clearcoatObject);
                if (clearcoatError != SUCCESS) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }
                
                auto clearcoat = std::make_unique<MaterialClearcoat>();

                double clearcoatFactor;
                if (auto error = clearcoatObject["clearcoatFactor"].get_double().get(clearcoatFactor); error == SUCCESS) {
                    clearcoat->clearcoatFactor = static_cast<num>(clearcoatFactor);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidJson;
                }

                ondemand::object clearcoatTextureObject;
                if (auto error = clearcoatObject["clearcoatTexture"].get_object().get(clearcoatTextureObject); error == SUCCESS) {
                    TextureInfo clearcoatTexture;
                    if (auto parseError = parseTextureInfo(clearcoatTextureObject, &clearcoatTexture, config.extensions); parseError == Error::None) {
                        clearcoat->clearcoatTexture = std::move(clearcoatTexture);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                double clearcoatRoughnessFactor;
                if (auto error = clearcoatObject["clearcoatRoughnessFactor"].get_double().get(clearcoatRoughnessFactor); error == SUCCESS) {
                    clearcoat->clearcoatRoughnessFactor = static_cast<num>(clearcoatRoughnessFactor);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidJson;
                }

                ondemand::object clearcoatRoughnessTextureObject;
                if (auto error = clearcoatObject["clearcoatRoughnessTexture"].get_object().get(clearcoatRoughnessTextureObject); error == SUCCESS) {
                    TextureInfo clearcoatRoughnessTexture;
                    if (auto parseError = parseTextureInfo(clearcoatRoughnessTextureObject, &clearcoatRoughnessTexture, config.extensions); parseError == Error::None) {
                        clearcoat->clearcoatRoughnessTexture = std::move(clearcoatRoughnessTexture);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                ondemand::object clearcoatNormalTextureObject;
                if (auto error = clearcoatObject["clearcoatNormalTexture"].get_object().get(clearcoatNormalTextureObject); error == SUCCESS) {
                    TextureInfo clearcoatNormalTexture;
                    if (auto parseError = parseTextureInfo(clearcoatNormalTextureObject, &clearcoatNormalTexture, config.extensions); parseError == Error::None) {
                        clearcoat->clearcoatNormalTexture = std::move(clearcoatNormalTexture);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                material.clearcoat = std::move(clearcoat);
                
                break;
            }
			case force_consteval<crc32c(extensions::KHR_materials_dispersion)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_dispersion))
					break;

				ondemand::object dispersionObject;
				auto dispersionError = extensionField.value().get_object().get(dispersionObject);
				if (dispersionError != SUCCESS) {
					return Error::InvalidJson;
				}

				double dispersionFactor;
				if (auto error = dispersionObject["dispersion"].get_double().get(dispersionFactor); error == SUCCESS) FASTGLTF_LIKELY {
					material.dispersion = static_cast<num>(dispersionFactor);
				} else if (error != NO_SUCH_FIELD) {
					return Error::InvalidJson;
				}
				break;
			}
            case force_consteval<crc32c(extensions::KHR_materials_emissive_strength)>: {
                if (!hasBit(config.extensions, Extensions::KHR_materials_emissive_strength))
                    break;
                
                ondemand::object emissiveObject;
                auto emissiveError = extensionField.value().get_object().get(emissiveObject);
                if (emissiveError != SUCCESS) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                double emissiveStrength;
                auto error = emissiveObject["emissiveStrength"].get_double().get(emissiveStrength);
                if (error != SUCCESS) {
                    return Error::InvalidGltf;
                }
                material.emissiveStrength = static_cast<num>(emissiveStrength);
                break;
            }
            case force_consteval<crc32c(extensions::KHR_materials_ior)>: {
                if (!hasBit(config.extensions, Extensions::KHR_materials_ior))
                    break;

                ondemand::object iorObject;
                auto iorError = extensionField.value().get_object().get(iorObject);
                if (iorError != SUCCESS) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                double ior;
                auto error = iorObject["ior"].get_double().get(ior);
                if (error == SUCCESS) {
                    material.ior = static_cast<num>(ior);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidJson;
                }
                break;
            }
            case force_consteval<crc32c(extensions::KHR_materials_iridescence)>: {
                if (!hasBit(config.extensions, Extensions::KHR_materials_iridescence))
                    break;

                ondemand::object iridescenceObject;
                auto iridescenceError = extensionField.value().get_object().get(iridescenceObject);
                if (iridescenceError != SUCCESS) FASTGLTF_UNLIKELY {
                    return Error::InvalidJson;
                }

                auto iridescence = std::make_unique<MaterialIridescence>();

                double iridescenceFactor;
                if (auto error = iridescenceObject["iridescenceFactor"].get_double().get(iridescenceFactor); error == SUCCESS) {
                    iridescence->iridescenceFactor = static_cast<num>(iridescenceFactor);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                ondemand::object iridescenceTextureObject;
                if (auto error = iridescenceObject["iridescenceTexture"].get_object().get(iridescenceTextureObject); error == SUCCESS) {
                    TextureInfo iridescenceTexture;
                    if (auto parseError = parseTextureInfo(iridescenceTextureObject, &iridescenceTexture, config.extensions); parseError == Error::None) {
                        iridescence->iridescenceTexture = std::move(iridescenceTexture);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                double iridescenceIor;
                if (auto error = iridescenceObject["iridescenceIor"].get_double().get(iridescenceIor); error == SUCCESS) {
                    iridescence->iridescenceIor = static_cast<num>(iridescenceIor);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                double iridescenceThicknessMinimum;
                if (auto error = iridescenceObject["iridescenceThicknessMinimum"].get_double().get(iridescenceThicknessMinimum); error == SUCCESS) {
                    iridescence->iridescenceThicknessMinimum = static_cast<num>(iridescenceThicknessMinimum);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                double iridescenceThicknessMaximum;
                if (auto error = iridescenceObject["iridescenceThicknessMaximum"].get_double().get(iridescenceThicknessMaximum); error == SUCCESS) {
                    iridescence->iridescenceThicknessMaximum = static_cast<num>(iridescenceThicknessMaximum);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                ondemand::object iridescenceThicknessTextureObject;
                if (auto error = iridescenceObject["iridescenceThicknessTexture"].get_object().get(iridescenceThicknessTextureObject); error == SUCCESS) {
                    TextureInfo iridescenceThicknessTexture;
                    if (auto parseError = parseTextureInfo(iridescenceThicknessTextureObject, &iridescenceThicknessTexture, config.extensions); parseError == Error::None) {
                        iridescence->iridescenceThicknessTexture = std::move(iridescenceThicknessTexture);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                material.iridescence = std::move(iridescence);
                break;
            }
            case force_consteval<crc32c(extensions::KHR_materials_sheen)>: {
                if (!hasBit(config.extensions, Extensions::KHR_materials_sheen))
                    break;

                ondemand::object sheenObject;
                auto sheenError = extensionField.value().get_object().get(sheenObject);
                if (sheenError != SUCCESS) FASTGLTF_UNLIKELY {
                    return Error::InvalidJson;
                }
                
                auto sheen = std::make_unique<MaterialSheen>();

                ondemand::array sheenColorFactor;
                if (auto error = sheenObject["sheenColorFactor"].get_array().get(sheenColorFactor); error == SUCCESS) {
                    if (auto copyError = copyNumericalJsonArray<double>(sheenColorFactor, sheen->sheenColorFactor); copyError != Error::None) {
                        return copyError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                ondemand::object sheenColorTextureObject;
                if (auto error = sheenObject["sheenColorTexture"].get_object().get(sheenColorTextureObject); error == SUCCESS) {
                    TextureInfo sheenColorTexture;
                    if (auto parseError = parseTextureInfo(sheenColorTextureObject, &sheenColorTexture, config.extensions); parseError == Error::None) {
                        sheen->sheenColorTexture = std::move(sheenColorTexture);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                double sheenRoughnessFactor;
                if (auto error = sheenObject["sheenRoughnessFactor"].get_double().get(sheenRoughnessFactor); error == SUCCESS) {
                    sheen->sheenRoughnessFactor = static_cast<num>(sheenRoughnessFactor);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                ondemand::object sheenRoughnessTextureObject;
                if (auto error = sheenObject["sheenRoughnessTexture"].get_object().get(sheenRoughnessTextureObject); error == SUCCESS) {
                    TextureInfo sheenRoughnessTexture;
                    if (auto parseError = parseTextureInfo(sheenRoughnessTextureObject, &sheenRoughnessTexture, config.extensions); parseError == Error::None) {
                        sheen->sheenRoughnessTexture = std::move(sheenRoughnessTexture);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                material.sheen = std::move(sheen);
                break;
            }
            case force_consteval<crc32c(extensions::KHR_materials_specular)>: {
                if (!hasBit(config.extensions, Extensions::KHR_materials_specular))
                    break;
                
                ondemand::object specularObject;
                auto specularError = extensionField.value().get_object().get(specularObject);
                if (specularError != SUCCESS) FASTGLTF_UNLIKELY {
                    return Error::InvalidJson;
                }

                auto specular = std::make_unique<MaterialSpecular>();

                double specularFactor;
                if (auto error = specularObject["specularFactor"].get_double().get(specularFactor); error == SUCCESS) {
                    specular->specularFactor = static_cast<num>(specularFactor);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                ondemand::object specularTextureObject;
                if (auto error = specularObject["specularTexture"].get_object().get(specularTextureObject); error == SUCCESS) {
                    TextureInfo specularTexture;
                    if (auto parseError = parseTextureInfo(specularTextureObject, &specularTexture, config.extensions); parseError == Error::None) {
                        specular->specularTexture = std::move(specularTexture);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                ondemand::array specularColorFactor;
                if (auto error = specularObject["specularColorFactor"].get_array().get(specularColorFactor); error == SUCCESS) {
                    if (auto copyError = copyNumericalJsonArray<double>(specularColorFactor, specular->specularColorFactor); copyError != Error::None) {
                        return copyError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                ondemand::object specularColorTextureObject;
                if (auto error = specularObject["specularColorTexture"].get_object().get(specularColorTextureObject); error == SUCCESS) {
                    TextureInfo specularColorTexture;
                    if (auto parseError = parseTextureInfo(specularColorTextureObject, &specularColorTexture, config.extensions); parseError == Error::None) {
                        specular->specularColorTexture = std::move(specularColorTexture);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                material.specular = std::move(specular);
                break;
            }
            case force_consteval<crc32c(extensions::KHR_materials_transmission)>: {
                if (!hasBit(config.extensions, Extensions::KHR_materials_transmission))
                    break;

                ondemand::object transmissionObject;
                auto transmissionError = extensionField.value().get_object().get(transmissionObject);
                if (transmissionError != SUCCESS) FASTGLTF_UNLIKELY {
                    return Error::InvalidJson;
                }
                
                auto transmission = std::make_unique<MaterialTransmission>();

                double transmissionFactor;
                if (auto error = transmissionObject["transmissionFactor"].get_double().get(transmissionFactor); error == SUCCESS) {
                    transmission->transmissionFactor = static_cast<num>(transmissionFactor);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                ondemand::object transmissionTextureObject;
                if (auto error = transmissionObject["transmissionTexture"].get_object().get(transmissionTextureObject); error == SUCCESS) {
                    TextureInfo transmissionTexture;
                    if (auto parseError = parseTextureInfo(transmissionTextureObject, &transmissionTexture, config.extensions); parseError == Error::None) {
                        transmission->transmissionTexture = std::move(transmissionTexture);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                material.transmission = std::move(transmission);
                break;
            }
            case force_consteval<crc32c(extensions::KHR_materials_unlit)>: {
                if (!hasBit(config.extensions, Extensions::KHR_materials_unlit))
                    break;

                ondemand::object unlitObject;
                auto unlitError = extensionField.value().get_object().get(unlitObject);
                if (unlitError == SUCCESS) {
                    material.unlit = true;
                } else FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }
                break;
            }
            case force_consteval<crc32c(extensions::KHR_materials_volume)>: {
                if (!hasBit(config.extensions, Extensions::KHR_materials_volume))
                    break;

                ondemand::object volumeObject;
                auto volumeError = extensionField.value().get_object().get(volumeObject);
                if (volumeError != SUCCESS) FASTGLTF_UNLIKELY {
                    return Error::InvalidJson;
                }

                auto volume = std::make_unique<MaterialVolume>();

                double thicknessFactor;
                if (auto error = volumeObject["thicknessFactor"].get_double().get(thicknessFactor); error == SUCCESS) {
                    volume->thicknessFactor = static_cast<num>(thicknessFactor);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                ondemand::object thicknessTextureObject;
                if (auto error = volumeObject["thicknessTexture"].get_object().get(thicknessTextureObject); error == SUCCESS) {
                    TextureInfo thicknessTexture;
                    if (auto parseError = parseTextureInfo(thicknessTextureObject, &thicknessTexture, config.extensions); parseError == Error::None) {
                        volume->thicknessTexture = std::move(thicknessTexture);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                double attenuationDistance;
                if (auto error = volumeObject["attenuationDistance"].get_double().get(attenuationDistance); error == SUCCESS) {
                    volume->attenuationDistance = static_cast<num>(attenuationDistance);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                ondemand::array attenuationColor;
                if (auto error = volumeObject["attenuationColor"].get_array().get(attenuationColor); error == SUCCESS) {
                    if (auto copyError = copyNumericalJsonArray<double>(attenuationColor, volume->attenuationColor); copyError != Error::None) {
                        return copyError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                material.volume = std::move(volume);
                break;
            }
            case force_consteval<crc32c(extensions::MSFT_packing_normalRoughnessMetallic)>: {
                if (!hasBit(config.extensions, Extensions::MSFT_packing_normalRoughnessMetallic))
                    break;

                ondemand::object normalRoughnessMetallic;
                auto error = extensionField.value().get_object().get(normalRoughnessMetallic);
                if (error != SUCCESS) {
                    return Error::InvalidGltf;
                }
                
                ondemand::object normalRoughnessMetallicTextureObject;
                if (auto error = normalRoughnessMetallic["normalRoughnessMetallicTexture"].get_object().get(normalRoughnessMetallicTextureObject); error == SUCCESS) {
                    TextureInfo textureInfo;
                    if (auto parseError = parseTextureInfo(normalRoughnessMetallicTextureObject, &textureInfo, config.extensions); parseError == Error::None) {
                        material.packedNormalMetallicRoughnessTexture = std::move(textureInfo);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }
                break;
            }
            case force_consteval<crc32c(extensions::MSFT_packing_occlusionRoughnessMetallic)>: {
                if (!hasBit(config.extensions, Extensions::MSFT_packing_occlusionRoughnessMetallic))
                    break;

                ondemand::object occlusionRoughnessMetallic;
                auto error = extensionField.value().get_object().get(occlusionRoughnessMetallic);
                if (error != SUCCESS) {
                    return Error::InvalidGltf;
                }
                
                auto packedTextures = std::make_unique<MaterialPackedTextures>();

                ondemand::object textureObject;
                if (auto error = occlusionRoughnessMetallic["occlusionRoughnessMetallicTexture"].get_object().get(textureObject); error == SUCCESS) {
                    TextureInfo textureInfo;
                    if (auto parseError = parseTextureInfo(textureObject, &textureInfo, config.extensions); parseError == Error::None) {
                        packedTextures->occlusionRoughnessMetallicTexture = std::move(textureInfo);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                if (auto error = occlusionRoughnessMetallic["roughnessMetallicOcclusionTexture"].get_object().get(textureObject); error == SUCCESS) {
                    TextureInfo textureInfo;
                    if (auto parseError = parseTextureInfo(textureObject, &textureInfo, config.extensions); parseError == Error::None) {
                        packedTextures->roughnessMetallicOcclusionTexture = std::move(textureInfo);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                if (auto error = occlusionRoughnessMetallic["normalTexture"].get_object().get(textureObject); error == SUCCESS) {
                    TextureInfo textureInfo;
                    if (auto parseError = parseTextureInfo(textureObject, &textureInfo, config.extensions); parseError == Error::None) {
                        packedTextures->normalTexture = std::move(textureInfo);
                    } else {
                        return parseError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                material.packedOcclusionRoughnessMetallicTextures = std::move(packedTextures);
                break;
            }
#if FASTGLTF_ENABLE_DEPRECATED_EXT
            case force_consteval<crc32c(extensions::KHR_materials_pbrSpecularGlossiness)>: {
                if (!hasBit(config.extensions, Extensions::KHR_materials_pbrSpecularGlossiness))
                    break;

                ondemand::object specularGlossinessObject;
                auto specularGlossinessError = extensionsObject[extensions::KHR_materials_pbrSpecularGlossiness].get_object().get(specularGlossinessObject);
                if (specularGlossinessError != SUCCESS) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                auto specularGlossiness = std::make_unique<MaterialSpecularGlossiness>();

                ondemand::array diffuseFactor;
                if (auto error = specularGlossinessObject["diffuseFactor"].get_array().get(diffuseFactor); error == SUCCESS) {
                    if (auto copyError = copyNumericalJsonArray<double>(diffuseFactor, specularGlossiness->diffuseFactor); copyError != Error::None) {
                        return copyError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                TextureInfo diffuseTexture;
                if (auto error = parseTextureInfo(specularGlossinessObject, "diffuseTexture", &diffuseTexture, config.extensions); error == Error::None) {
                    specularGlossiness->diffuseTexture = std::move(diffuseTexture);
                } else if (error != Error::MissingField) FASTGLTF_UNLIKELY {
                    return error;
                }

                ondemand::array specularFactor;
                if (auto error = specularGlossinessObject["specularFactor"].get_array().get(specularFactor); error == SUCCESS) {
                    if (auto copyError = copyNumericalJsonArray<double>(specularFactor, specularGlossiness->specularFactor); copyError != Error::None) {
                        return copyError;
                    }
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                double glossinessFactor;
                if (auto error = specularGlossinessObject["glossinessFactor"].get_double().get(glossinessFactor); error == SUCCESS) {
                    specularGlossiness->glossinessFactor = static_cast<num>(glossinessFactor);
                } else if (error != NO_SUCH_FIELD) FASTGLTF_UNLIKELY {
                    return Error::InvalidGltf;
                }

                TextureInfo specularGlossinessTexture;
                if (auto error = parseTextureInfo(specularGlossinessObject, "specularGlossinessTexture", &specularGlossinessTexture, config.extensions); error == Error::None) {
                    specularGlossiness->specularGlossinessTexture = std::move(specularGlossinessTexture);
                } else if (error != Error::MissingField) FASTGLTF_UNLIKELY {
                    return error;
                }

                material.specularGlossiness = std::move(specularGlossiness);
                break;
            }
#endif
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseMaterials(simdjson::ondemand::array& materials, Asset& asset) {
    using namespace simdjson;

    // asset.materials.reserve(materials.size());
    for (auto materialValue : materials) {
        ondemand::object materialObject;
        if (materialValue.get_object().get(materialObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& material = asset.materials.emplace_back();
        material.pbrData.baseColorFactor = {{ 1.0f, 1.0f, 1.0f, 1.0f }};
        for (auto field : materialObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidGltf;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("pbrMetallicRoughness")>: {
                    ondemand::object pbrObject;
                    if (field.value().get_object().get(pbrObject) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    auto& pbr = material.pbrData;
                    for (auto pbrField : pbrObject) {
                        std::string_view pbrKey;
                        if (pbrField.unescaped_key().get(pbrKey) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidGltf;
                        }

                        switch (crcStringFunction(pbrKey)) {
                            case force_consteval<crc32c("baseColorFactor")>: {
                                ondemand::array baseColorFactor;
                                if (pbrField.value().get_array().get(baseColorFactor) != SUCCESS) FASTGLTF_UNLIKELY {
                                    return Error::InvalidGltf;
                                }
                                if (auto error = copyNumericalJsonArray<double>(baseColorFactor, pbr.baseColorFactor); error != Error::None) FASTGLTF_UNLIKELY {
                                    return error;
                                }
                                break;
                            }
                            case force_consteval<crc32c("baseColorTexture")>: {
                                ondemand::object baseColorTextureObject;
                                if (pbrField.value().get_object().get(baseColorTextureObject) != SUCCESS) FASTGLTF_UNLIKELY {
                                    return Error::InvalidGltf;
                                }

                                TextureInfo baseColorTextureInfo = {};
                                if (auto error = parseTextureInfo(baseColorTextureObject, &baseColorTextureInfo, config.extensions); error == Error::None) {
                                    pbr.baseColorTexture = std::move(baseColorTextureInfo);
                                } else if (error != Error::MissingField) FASTGLTF_UNLIKELY {
                                    return error;
                                }
                                break;
                            }
                            case force_consteval<crc32c("metallicFactor")>: {
                                double value;
                                if (pbrField.value().get(value) != SUCCESS) FASTGLTF_UNLIKELY {
                                    return Error::InvalidGltf;
                                }
                                pbr.metallicFactor = static_cast<num>(value);
                                break;
                            }
                            case force_consteval<crc32c("roughnessFactor")>: {
                                double value;
                                if (pbrField.value().get(value) != SUCCESS) FASTGLTF_UNLIKELY {
                                    return Error::InvalidGltf;
                                }
                                pbr.roughnessFactor = static_cast<num>(value);
                                break;
                            }
                            case force_consteval<crc32c("metallicRoughnessTexture")>: {
                                ondemand::object metallicRoughnessTextureObject;
                                if (pbrField.value().get_object().get(metallicRoughnessTextureObject) != SUCCESS) FASTGLTF_UNLIKELY {
                                    return Error::InvalidGltf;
                                }

                                TextureInfo metallicRoughnessTextureInfo = {};
                                if (auto error = parseTextureInfo(metallicRoughnessTextureObject, &metallicRoughnessTextureInfo, config.extensions); error == Error::None) {
                                    pbr.metallicRoughnessTexture = std::move(metallicRoughnessTextureInfo);
                                } else if (error != Error::MissingField) FASTGLTF_UNLIKELY {
                                    return error;
                                }
                                break;
                            }
                        }
                    }
                    break;
                }
                case force_consteval<crc32c("normalTexture")>: {
                    ondemand::object normalTextureObject;
                    if (field.value().get_object().get(normalTextureObject) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    NormalTextureInfo normalTextureInfo = {};
                    if (auto error = parseTextureInfo(normalTextureObject, &normalTextureInfo, config.extensions, TextureInfoType::NormalTexture); error == Error::None) {
                        material.normalTexture = std::move(normalTextureInfo);
                    } else if (error != Error::MissingField) FASTGLTF_UNLIKELY {
                        return error;
                    }
                    break;
                }
                case force_consteval<crc32c("occlusionTexture")>: {
                    ondemand::object occlusionTextureObject;
                    if (field.value().get_object().get(occlusionTextureObject) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    OcclusionTextureInfo occlusionTextureInfo = {};
                    if (auto error = parseTextureInfo(occlusionTextureObject, &occlusionTextureInfo, config.extensions, TextureInfoType::OcclusionTexture); error == Error::None) {
                        material.occlusionTexture = std::move(occlusionTextureInfo);
                    } else if (error != Error::MissingField) FASTGLTF_UNLIKELY {
                        return error;
                    }
                    break;
                }
                case force_consteval<crc32c("emissiveTexture")>: {
                    ondemand::object emissiveTextureObject;
                    if (field.value().get_object().get(emissiveTextureObject) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    NormalTextureInfo emissiveTextureInfo = {};
                    if (auto error = parseTextureInfo(emissiveTextureObject, &emissiveTextureInfo, config.extensions); error == Error::None) {
                        material.emissiveTexture = std::move(emissiveTextureInfo);
                    } else if (error != Error::MissingField) FASTGLTF_UNLIKELY {
                        return error;
                    }
                    break;
                }
                case force_consteval<crc32c("emissiveFactor")>: {
                    ondemand::array emissiveFactor;
                    if (field.value().get_array().get(emissiveFactor) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    if (auto error = copyNumericalJsonArray<double>(emissiveFactor, material.emissiveFactor); error != Error::None) {
                        return error;
                    }
                    break;
                }
                case force_consteval<crc32c("alphaMode")>: {
                    std::string_view alphaMode;
                    if (field.value().get_string().get(alphaMode) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    switch (crcStringFunction(alphaMode)) {
                        case force_consteval<crc32c("OPAQUE")>:
                            material.alphaMode = AlphaMode::Opaque;
                            break;
                        case force_consteval<crc32c("MASK")>:
                            material.alphaMode = AlphaMode::Mask;
                            break;
                        case force_consteval<crc32c("BLEND")>:
                            material.alphaMode = AlphaMode::Blend;
                            break;
                        default:
                            return Error::InvalidGltf;
                    }
                    break;
                }
                case force_consteval<crc32c("alphaCutoff")>: {
                    double value;
                    if (field.value().get(value) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    material.alphaCutoff = static_cast<num>(value);
                    break;
                }
                case force_consteval<crc32c("doubleSided")>: {
                    bool doubleSided;
                    if (field.value().get_bool().get(doubleSided) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    material.doubleSided = doubleSided;
                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    material.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(material.name), resourceAllocator.get(), name);
                    break;
                }
                case force_consteval<crc32c("extensions")>: {
                    ondemand::object extensionsObject;
                    if (field.value().get_object().get(extensionsObject) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    auto error = parseMaterialExtensions(extensionsObject, material);
                    if (error != Error::None) {
                        return error;
                    }
                    break;
                }
            }
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseMeshPrimitives(simdjson::ondemand::array& primitives, fastgltf::Asset& asset,
                                          fastgltf::Mesh& mesh) {
    using namespace simdjson;

    auto parseAttributes = [this](ondemand::object& object, decltype(Primitive::attributes)& attributes) -> auto {
        // We iterate through the JSON object and write each key/pair value into the
        // attribute map. The keys are only validated in the validate() method.
        attributes = FASTGLTF_CONSTRUCT_PMR_RESOURCE(std::remove_reference_t<decltype(attributes)>, resourceAllocator.get(), 0);
        // attributes.reserve(object.size());
        for (auto field : object) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidJson;
            }

            std::uint64_t attributeIndex;
            if (field.value().get(attributeIndex) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidGltf;
            }
            attributes.emplace_back(
                    std::make_pair(FASTGLTF_CONSTRUCT_PMR_RESOURCE(FASTGLTF_STD_PMR_NS::string, resourceAllocator.get(), key), static_cast<std::size_t>(attributeIndex)));
        }
        return Error::None;
    };

    mesh.primitives = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(mesh.primitives), resourceAllocator.get(), 0);
    for (auto primitiveValue : primitives) {
        ondemand::object primitiveObject;
        if (primitiveValue.get_object().get(primitiveObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& primitive = mesh.primitives.emplace_back();
        primitive.type = PrimitiveType::Triangles;
        for (auto field : primitiveObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidGltf;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("attributes")>: {
                    ondemand::object attributesObject;
                    if (field.value().get_object().get(attributesObject) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    auto parseError = parseAttributes(attributesObject, primitive.attributes);
                    if (parseError != Error::None) FASTGLTF_UNLIKELY {
                        return parseError;
                    }
                    break;
                }
                case force_consteval<crc32c("indices")>: {
                    std::uint64_t indices;
                    if (field.value().get(indices) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    primitive.indicesAccessor = static_cast<std::size_t>(indices);
                    break;
                }
                case force_consteval<crc32c("material")>: {
                    std::uint64_t material;
                    if (field.value().get(material) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    primitive.materialIndex = static_cast<std::size_t>(material);
                    break;
                }
                case force_consteval<crc32c("mode")>: {
                    std::uint64_t mode;
                    if (field.value().get(mode) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    primitive.type = static_cast<PrimitiveType>(mode);
                    break;
                }
                case force_consteval<crc32c("targets")>: {
                    ondemand::array targets;
                    if (field.value().get_array().get(targets) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    primitive.targets = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(primitive.targets), resourceAllocator.get(), 0);
                    for (auto targetValue : targets) {
                        ondemand::object attributesObject;
                        if (targetValue.get_object().get(attributesObject) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidGltf;
                        }
                        auto& attributes = primitive.targets.emplace_back();
                        auto parseError = parseAttributes(attributesObject, attributes);
                        if (parseError != Error::None) FASTGLTF_UNLIKELY {
                            return parseError;
                        }
                    }
                    break;
                }
            }
        }
    }

    return Error::None;
}

fg::Error fg::Parser::parseMeshes(simdjson::ondemand::array& meshes, Asset& asset) {
    using namespace simdjson;

    // asset.meshes.reserve(meshes.size());
    for (auto meshValue : meshes) {
        // Required fields: "primitives"
        ondemand::object meshObject;
        if (meshValue.get_object().get(meshObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& mesh = asset.meshes.emplace_back();
        for (auto field : meshObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidGltf;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("primitives")>: {
                    ondemand::array primitives;
                    if (field.value().get_array().get(primitives) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    auto primitivesError = parseMeshPrimitives(primitives, asset, mesh);
                    if (primitivesError != Error::None) FASTGLTF_UNLIKELY {
                        return primitivesError;
                    }
                    break;
                }
                case force_consteval<crc32c("weights")>: {
                    ondemand::array weights;
                    if (field.value().get_array().get(weights) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    mesh.weights = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(mesh.weights), resourceAllocator.get(), 0);
                    for (auto weightValue : weights) {
                        double val;
                        if (weightValue.get(val) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidGltf;
                        }
                        mesh.weights.emplace_back(static_cast<num>(val));
                    }
                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    mesh.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(mesh.name), resourceAllocator.get(), name);
                    break;
                }
            }
        }

        if (mesh.primitives.empty()) {
            return Error::InvalidGltf;
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseNodes(simdjson::ondemand::array& nodes, Asset& asset) {
    using namespace simdjson;

    // asset.nodes.reserve(nodes.size());
    for (auto nodeValue : nodes) {
        ondemand::object nodeObject;
        if (nodeValue.get_object().get(nodeObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& node = asset.nodes.emplace_back();
        node.transform = TRS();
        for (auto field : nodeObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidJson;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("camera")>: {
                    std::uint64_t camera;
                    if (field.value().get(camera) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    node.cameraIndex = static_cast<std::size_t>(camera);
                    break;
                }
                case force_consteval<crc32c("children")>: {
                    ondemand::array children;
                    if (field.value().get_array().get(children) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    node.children = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(node.children), resourceAllocator.get(), 0);
                    for (auto childValue : children) {
                        std::uint64_t idx;
                        if (childValue.get(idx) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidGltf;
                        }
                        node.children.emplace_back(static_cast<std::size_t>(idx));
                    }
                    break;
                }
                case force_consteval<crc32c("skin")>: {
                    std::uint64_t skin;
                    if (field.value().get(skin) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    node.skinIndex = static_cast<std::size_t>(skin);
                    break;
                }
                case force_consteval<crc32c("matrix")>: {
                    ondemand::array matrixArray;
                    if (field.value().get_array().get(matrixArray) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    Node::TransformMatrix matrix;
                    auto error = copyNumericalJsonArray<double>(matrixArray, matrix);
                    if (error != Error::None) FASTGLTF_UNLIKELY {
                        return error;
                    }
                    if (hasBit(options, Options::DecomposeNodeMatrices)) {
                        TRS trs = {};
                        decomposeTransformMatrix(matrix, trs.scale, trs.rotation, trs.translation);
                        node.transform = trs;
                    } else {
                        node.transform = matrix;
                    }
                    break;
                }
                case force_consteval<crc32c("mesh")>: {
                    std::uint64_t mesh;
                    if (field.value().get(mesh) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    node.meshIndex = static_cast<std::size_t>(mesh);
                    break;
                }
                case force_consteval<crc32c("rotation")>: {
                    if (!std::holds_alternative<TRS>(node.transform)) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    ondemand::array rotationArray;
                    if (field.value().get_array().get(rotationArray) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    auto& trs = std::get<TRS>(node.transform);
                    auto error = copyNumericalJsonArray<double>(rotationArray, trs.rotation);
                    if (error != Error::None) FASTGLTF_UNLIKELY {
                        return error;
                    }
                    break;
                }
                case force_consteval<crc32c("scale")>: {
                    if (!std::holds_alternative<TRS>(node.transform)) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    ondemand::array scaleArray;
                    if (field.value().get_array().get(scaleArray) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    auto& trs = std::get<TRS>(node.transform);
                    auto error = copyNumericalJsonArray<double>(scaleArray, trs.scale);
                    if (error != Error::None) FASTGLTF_UNLIKELY {
                        return error;
                    }
                    break;
                }
                case force_consteval<crc32c("translation")>: {
                    if (!std::holds_alternative<TRS>(node.transform)) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    ondemand::array translationArray;
                    if (field.value().get_array().get(translationArray) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    auto& trs = std::get<TRS>(node.transform);
                    auto error = copyNumericalJsonArray<double>(translationArray, trs.translation);
                    if (error != Error::None) FASTGLTF_UNLIKELY {
                        return error;
                    }
                    break;
                }
                case force_consteval<crc32c("weights")>: {
                    ondemand::array weights;
                    if (field.value().get_array().get(weights) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    node.weights = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(node.weights), resourceAllocator.get(), 0);
                    for (auto weightValue : weights) {
                        double val;
                        if (weightValue.get(val) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidGltf;
                        }
                        node.weights.emplace_back(static_cast<num>(val));
                    }
                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    node.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(node.name), resourceAllocator.get(), name);
                    break;
                }
                case force_consteval<crc32c("extensions")>: {
                    ondemand::object extensionsObject;
                    if (field.value().get_object().get(extensionsObject) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

					if (hasBit(config.extensions, Extensions::KHR_lights_punctual)) {
						ondemand::object lightsObject;
						if (auto error = extensionsObject[extensions::KHR_lights_punctual].get_object().get(lightsObject); error == SUCCESS) {
							std::uint64_t light;
							if (auto lightError =lightsObject["light"].get_uint64().get(light); lightError == SUCCESS) FASTGLTF_LIKELY {
								node.lightIndex = static_cast<std::size_t>(light);
							} else {
								return lightError == NO_SUCH_FIELD || lightError == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
							}
						} else if (error != NO_SUCH_FIELD) {
							return Error::InvalidGltf;
						}
					}

					if (hasBit(config.extensions, Extensions::EXT_mesh_gpu_instancing)) {
						ondemand::object gpuInstancingObject;
						if (auto error = extensionsObject[extensions::EXT_mesh_gpu_instancing].get_object().get(gpuInstancingObject); error == SUCCESS) {
							ondemand::object attributesObject;
							if (gpuInstancingObject["attributes"].get_object().get(attributesObject) == SUCCESS) {
								auto parseAttributes = [this](ondemand::object& object, decltype(node.instancingAttributes)& attributes) -> auto {
									// We iterate through the JSON object and write each key/pair value into the
									// attribute map. The keys are only validated in the validate() method.
									attributes = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(node.instancingAttributes),
																				 resourceAllocator.get(), 0);
									// attributes.reserve(object.size());
									for (auto field: object) {
										std::string_view key;
										if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
											return Error::InvalidJson;
										}

										std::uint64_t attributeIndex;
										if (field.value().get_uint64().get(attributeIndex) !=
											SUCCESS) FASTGLTF_UNLIKELY {
											return Error::InvalidGltf;
										}
										attributes.emplace_back(
											std::make_pair(FASTGLTF_CONSTRUCT_PMR_RESOURCE(FASTGLTF_STD_PMR_NS::string, resourceAllocator.get(), key),
														   static_cast<std::size_t>(attributeIndex)));
									}
									return Error::None;
								};
								parseAttributes(attributesObject, node.instancingAttributes);
							} else {
								return Error::InvalidGltf;
							}
						} else if (error != NO_SUCH_FIELD) {
							return Error::InvalidGltf;
						}
					}
                }
            }
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseSamplers(simdjson::ondemand::array& samplers, Asset& asset) {
    using namespace simdjson;

    // asset.samplers.reserve(samplers.size());
    for (auto samplerValue : samplers) {
        ondemand::object samplerObject;
        if (samplerValue.get_object().get(samplerObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& sampler = asset.samplers.emplace_back();
        sampler.wrapS = sampler.wrapT = Wrap::Repeat;
        for (auto field : samplerObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidJson;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("magFilter")>: {
                    std::uint64_t magFilter;
                    if (field.value().get(magFilter) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    sampler.magFilter = static_cast<Filter>(magFilter);
                    break;
                }
                case force_consteval<crc32c("minFilter")>: {
                    std::uint64_t minFilter;
                    if (field.value().get(minFilter) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    sampler.minFilter = static_cast<Filter>(minFilter);
                    break;
                }
                case force_consteval<crc32c("wrapS")>: {
                    std::uint64_t wrapS;
                    if (field.value().get(wrapS) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    sampler.wrapS = static_cast<Wrap>(wrapS);
                    break;
                }
                case force_consteval<crc32c("wrapT")>: {
                    std::uint64_t wrapT;
                    if (field.value().get(wrapT) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    sampler.wrapT = static_cast<Wrap>(wrapT);
                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    sampler.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(sampler.name), resourceAllocator.get(), name);
                    break;
                }
            }
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseScenes(simdjson::ondemand::array& scenes, Asset& asset) {
    using namespace simdjson;

    // asset.scenes.reserve(scenes.size());
    for (auto sceneValue : scenes) {
        ondemand::object sceneObject;
        if (sceneValue.get_object().get(sceneObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& scene = asset.scenes.emplace_back();

        for (auto field : sceneObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidJson;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("nodes")>: {
                    ondemand::array nodes;
                    if (field.value().get_array().get(nodes) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    scene.nodeIndices = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(scene.nodeIndices), resourceAllocator.get(), 0);
                    for (auto nodeValue : nodes) {
                        std::uint64_t index;
                        if (nodeValue.get(index) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidGltf;
                        }

                        scene.nodeIndices.emplace_back(static_cast<std::size_t>(index));
                    }
                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    scene.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(scene.name), resourceAllocator.get(), name);
                    break;
                }
            }
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseSkins(simdjson::ondemand::array& skins, Asset& asset) {
    using namespace simdjson;

    // asset.skins.reserve(skins.size());
    for (auto skinValue : skins) {
        ondemand::object skinObject;
        if (skinValue.get_object().get(skinObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& skin = asset.skins.emplace_back();
        for (auto field : skinObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidJson;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("inverseBindMatrices")>: {
                    std::uint64_t inverseBindMatrices;
                    if (field.value().get(inverseBindMatrices) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    skin.inverseBindMatrices = static_cast<std::size_t>(inverseBindMatrices);
                    break;
                }
                case force_consteval<crc32c("skeleton")>: {
                    std::uint64_t skeleton;
                    if (field.value().get(skeleton) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    skin.skeleton = static_cast<std::size_t>(skeleton);
                    break;
                }
                case force_consteval<crc32c("joints")>: {
                    ondemand::array joints;
                    if (field.value().get_array().get(joints) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    skin.joints = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(skin.joints), resourceAllocator.get(), 0);
                    for (auto nodeValue : joints) {
                        std::uint64_t index;
                        if (nodeValue.get(index) != SUCCESS) FASTGLTF_UNLIKELY {
                            return Error::InvalidGltf;
                        }

                        skin.joints.emplace_back(static_cast<std::size_t>(index));
                    }
                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    skin.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(skin.name), resourceAllocator.get(), name);
                    break;
                }
            }
        }
    }

	return Error::None;
}

fg::Error fg::Parser::parseTextures(simdjson::ondemand::array& textures, Asset& asset) {
    using namespace simdjson;

    // asset.textures.reserve(textures.size());
    for (auto textureValue : textures) {
        ondemand::object textureObject;
        if (textureValue.get_object().get(textureObject) != SUCCESS) FASTGLTF_UNLIKELY {
            return Error::InvalidGltf;
        }

        auto& texture = asset.textures.emplace_back();
        for (auto field : textureObject) {
            std::string_view key;
            if (field.unescaped_key().get(key) != SUCCESS) FASTGLTF_UNLIKELY {
                return Error::InvalidJson;
            }

            switch (crcStringFunction(key)) {
                case force_consteval<crc32c("source")>: {
                    std::uint64_t sourceIndex;
                    if (field.value().get(sourceIndex) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    texture.imageIndex = static_cast<std::size_t>(sourceIndex);
                    break;
                }
                case force_consteval<crc32c("sampler")>: {
                    std::uint64_t sampler;
                    if (field.value().get(sampler) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    texture.samplerIndex = static_cast<std::size_t>(sampler);
                    break;
                }
                case force_consteval<crc32c("name")>: {
                    std::string_view name;
                    if (auto error = field.value().get_string().get(name); error != SUCCESS) FASTGLTF_UNLIKELY {
                        return error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
                    }
                    texture.name = FASTGLTF_CONSTRUCT_PMR_RESOURCE(decltype(texture.name), resourceAllocator.get(), name);
                    break;
                }
                case force_consteval<crc32c("extensions")>: {
                    ondemand::object extensionsObject;
                    if (field.value().get_object().get(extensionsObject) != SUCCESS) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }

                    if (!parseTextureExtensions(texture, extensionsObject, config.extensions)) FASTGLTF_UNLIKELY {
                        return Error::InvalidGltf;
                    }
                    break;
                }
            }
        }
    }

	return Error::None;
}

#pragma endregion

#pragma region GltfDataBuffer
std::size_t fg::getGltfBufferPadding() noexcept {
    return simdjson::SIMDJSON_PADDING;
}

fg::GltfDataBuffer::GltfDataBuffer() noexcept = default;
fg::GltfDataBuffer::~GltfDataBuffer() noexcept = default;

fg::GltfDataBuffer::GltfDataBuffer(span<std::byte> data) noexcept {
	dataSize = data.size();

	allocatedSize = data.size() + getGltfBufferPadding();
	buffer = decltype(buffer)(new std::byte[allocatedSize]);
	auto* ptr = buffer.get();

	std::memcpy(ptr, data.data(), dataSize);
	std::memset(ptr + dataSize, 0, allocatedSize - dataSize);

	bufferPointer = ptr;
}

bool fg::GltfDataBuffer::fromByteView(std::uint8_t* bytes, std::size_t byteCount, std::size_t capacity) noexcept {
    using namespace simdjson;
    if (bytes == nullptr || byteCount == 0 || capacity == 0)
        return false;

    if (capacity - byteCount < simdjson::SIMDJSON_PADDING)
        return copyBytes(bytes, byteCount);

    dataSize = byteCount;
    bufferPointer = reinterpret_cast<std::byte*>(bytes);
    allocatedSize = capacity;
    std::memset(bufferPointer + dataSize, 0, allocatedSize - dataSize);
    return true;
}

bool fg::GltfDataBuffer::copyBytes(const std::uint8_t* bytes, std::size_t byteCount) noexcept {
    using namespace simdjson;
    if (bytes == nullptr || byteCount == 0)
        return false;

    // Allocate a byte array with a bit of padding.
    dataSize = byteCount;
    allocatedSize = byteCount + getGltfBufferPadding();
    buffer = decltype(buffer)(new std::byte[allocatedSize]); // To mimic std::make_unique_for_overwrite (C++20)
    bufferPointer = buffer.get();

    // Copy the data and fill the padding region with zeros.
    std::memcpy(bufferPointer, bytes, dataSize);
    std::memset(bufferPointer + dataSize, 0, allocatedSize - dataSize);
    return true;
}

bool fg::GltfDataBuffer::loadFromFile(const fs::path& path, std::uint64_t byteOffset) noexcept {
    using namespace simdjson;
    std::error_code ec;
    auto length = static_cast<std::streamsize>(std::filesystem::file_size(path, ec));
    if (ec) {
        return false;
    }

    // Open the file and determine the size.
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open() || file.bad())
        return false;

    filePath = path;

    file.seekg(static_cast<std::streamsize>(byteOffset), std::ifstream::beg);

    dataSize = static_cast<std::uint64_t>(length) - byteOffset;
    allocatedSize = dataSize + getGltfBufferPadding();
    buffer = decltype(buffer)(new std::byte[allocatedSize]); // To mimic std::make_unique_for_overwrite (C++20)
    if (!buffer)
        return false;
    bufferPointer = buffer.get();

    // Copy the data and fill the padding region with zeros.
    file.read(reinterpret_cast<char*>(bufferPointer), static_cast<std::streamsize>(dataSize));
    std::memset(bufferPointer + dataSize, 0, allocatedSize - dataSize);
    return true;
}
#pragma endregion

#pragma region AndroidGltfDataBuffer
#if defined(__ANDROID__)
fg::AndroidGltfDataBuffer::AndroidGltfDataBuffer(AAssetManager* assetManager) noexcept : assetManager{assetManager} {}

bool fg::AndroidGltfDataBuffer::loadFromAndroidAsset(const fs::path& path, std::uint64_t byteOffset) noexcept {
    if (assetManager == nullptr) {
        return false;
    }

    using namespace simdjson;

    const auto filenameString = path.string();

    auto assetDeleter = [](AAsset* file) { AAsset_close(file); };
    auto file = std::unique_ptr<AAsset, decltype(assetDeleter)>(AAssetManager_open(assetManager, filenameString.c_str(), AASSET_MODE_BUFFER), assetDeleter);
    if (file == nullptr) {
        return false;
    }

    const auto length = AAsset_getLength(file.get());
    if (length == 0) {
        return false;
    }

    dataSize = length - byteOffset;
    allocatedSize = dataSize + simdjson::SIMDJSON_PADDING;
    buffer = decltype(buffer)(new std::byte[allocatedSize]);
    if (!buffer) {
        return false;
    }

    bufferPointer = buffer.get();

    if (byteOffset > 0) {
        AAsset_seek64(file.get(), byteOffset, SEEK_SET);
    }

    AAsset_read(file.get(), bufferPointer, dataSize);

    std::memset(bufferPointer + dataSize, 0, allocatedSize - dataSize);

    filePath = path;

    return true;
}
#endif
#pragma endregion

#pragma region Parser
fastgltf::GltfType fg::determineGltfFileType(GltfDataBuffer* buffer) {
    // First, check if any of the first four characters is a '{'.
    std::array<std::uint8_t, 4> begin = {};
    std::memcpy(begin.data(), buffer->bufferPointer, sizeof begin);
    for (const auto& i : begin) {
        if ((char)i == '{')
            return GltfType::glTF;
    }

    // We'll try and read a BinaryGltfHeader from the buffer to see if the magic is correct.
    BinaryGltfHeader header = {};
    std::memcpy(&header, buffer->bufferPointer, sizeof header);
    if (header.magic == binaryGltfHeaderMagic) {
        return GltfType::GLB;
    }

    return GltfType::Invalid;
}

fg::Parser::Parser(Extensions extensionsToLoad) noexcept {
    std::call_once(crcInitialisation, initialiseCrc);
    jsonParser = std::make_unique<simdjson::ondemand::parser>();
    config.extensions = extensionsToLoad;
}

fg::Parser::Parser(Parser&& other) noexcept : jsonParser(std::move(other.jsonParser)), config(other.config) {}

fg::Parser& fg::Parser::operator=(Parser&& other) noexcept {
    jsonParser = std::move(other.jsonParser);
    config = other.config;
    return *this;
}

fg::Parser::~Parser() = default;

fg::Expected<fg::Asset> fg::Parser::loadGltf(GltfDataBuffer* buffer, fs::path directory, Options options, Category categories) {
    auto type = fastgltf::determineGltfFileType(buffer);

    if (type == fastgltf::GltfType::glTF) {
        return loadGltfJson(buffer, std::move(directory), options, categories);
    }

    if (type == fastgltf::GltfType::GLB) {
        return loadGltfBinary(buffer, std::move(directory), options, categories);
    }

    return Expected<Asset> { Error::InvalidFileData };
}

fg::Expected<fg::Asset> fg::Parser::loadGltfJson(GltfDataBuffer* buffer, fs::path directory, Options options, Category categories) {
    using namespace simdjson;

    // If we never have to load the files ourselves, we're fine with the directory being invalid/blank.
    if (hasBit(options, Options::LoadExternalBuffers) && !fs::is_directory(directory)) {
        return Expected<Asset>(Error::InvalidPath);
    }

	this->options = options;
	this->directory = std::move(directory);

    // If we own the allocation of the JSON data, we'll try to minify the JSON, which, in most cases,
    // will speed up the parsing by a small amount.
    std::size_t jsonLength = buffer->getBufferSize();
    if (buffer->buffer != nullptr && hasBit(options, Options::MinimiseJsonBeforeParsing)) {
        std::size_t newLength = 0;
        auto result = simdjson::minify(reinterpret_cast<const char*>(buffer->bufferPointer), buffer->getBufferSize(),
                                       reinterpret_cast<char*>(buffer->bufferPointer), newLength);
        if (result != SUCCESS || newLength == 0) {
            return Expected<Asset>(Error::InvalidJson);
        }
        buffer->dataSize = jsonLength = newLength;
    }

	simdjson::ondemand::document doc;
    if (auto error = jsonParser->iterate(reinterpret_cast<const std::uint8_t*>(buffer->bufferPointer), jsonLength, buffer->allocatedSize)
            .get(doc); error != SUCCESS) FASTGLTF_UNLIKELY {
	    return Expected<Asset>(Error::InvalidJson);
    }
    simdjson::ondemand::object root;
    if (auto error = doc.get_object().get(root); error != SUCCESS) FASTGLTF_UNLIKELY {
        return Expected<Asset>(Error::InvalidJson);
    }

	return parse(root, categories);
}

fg::Expected<fg::Asset> fg::Parser::loadGltfBinary(GltfDataBuffer* buffer, fs::path directory, Options options, Category categories) {
    using namespace simdjson;

    // If we never have to load the files ourselves, we're fine with the directory being invalid/blank.
    if (hasBit(options, Options::LoadExternalBuffers) && !fs::is_directory(directory)) {
	    return Expected<Asset>(Error::InvalidPath);
    }

	this->options = options;
    this->directory = std::move(directory);

	std::size_t offset = 0UL;
    auto read = [&buffer, &offset](void* dst, std::size_t size) mutable {
        std::memcpy(dst, buffer->bufferPointer + offset, size);
        offset += size;
    };

    BinaryGltfHeader header = {};
    read(&header, sizeof header);
    if (header.magic != binaryGltfHeaderMagic) {
	    return Expected<Asset>(Error::InvalidGLB);
    }
	if (header.version != 2) {
		return Expected<Asset>(Error::UnsupportedVersion);
	}
    if (header.length >= buffer->allocatedSize) {
	    return Expected<Asset>(Error::InvalidGLB);
    }

    // The glTF 2 spec specifies that in GLB files the order of chunks is predefined. Specifically,
    //  1. JSON chunk
    //  2. BIN chunk (optional)
    BinaryGltfChunk jsonChunk = {};
    read(&jsonChunk, sizeof jsonChunk);
    if (jsonChunk.chunkType != binaryGltfJsonChunkMagic) {
	    return Expected<Asset>(Error::InvalidGLB);
    }

    // Create a string view of the JSON chunk in the GLB data buffer. The documentation of parse()
    // says the padding can be initialised to anything, apparently. Therefore, this should work.
    simdjson::padded_string_view jsonChunkView(reinterpret_cast<const std::uint8_t*>(buffer->bufferPointer) + offset,
                                               jsonChunk.chunkLength,
                                               jsonChunk.chunkLength + SIMDJSON_PADDING);
    offset += jsonChunk.chunkLength;

	simdjson::ondemand::document doc;
    if (jsonParser->iterate(jsonChunkView).get(doc) != SUCCESS) FASTGLTF_UNLIKELY {
	    return Expected<Asset>(Error::InvalidJson);
    }

    simdjson::ondemand::object root;
    if (doc.get_object().get(root) != SUCCESS) FASTGLTF_UNLIKELY {
        return Expected<Asset>(Error::InvalidJson);
    }

    // Is there enough room for another chunk header?
    if (header.length > (offset + sizeof(BinaryGltfChunk))) {
        BinaryGltfChunk binaryChunk = {};
        read(&binaryChunk, sizeof binaryChunk);

        if (binaryChunk.chunkType != binaryGltfDataChunkMagic) {
	        return Expected<Asset>(Error::InvalidGLB);
        }

        if (hasBit(options, Options::LoadGLBBuffers)) {
            if (config.mapCallback != nullptr) {
                auto info = config.mapCallback(binaryChunk.chunkLength, config.userPointer);
                if (info.mappedMemory != nullptr) {
                    read(info.mappedMemory, binaryChunk.chunkLength);
                    if (config.unmapCallback != nullptr) {
                        config.unmapCallback(&info, config.userPointer);
                    }
                    glbBuffer = sources::CustomBuffer { info.customId, MimeType::None };
                }
            } else {
				StaticVector<std::uint8_t> binaryData(binaryChunk.chunkLength);
				read(binaryData.data(), binaryChunk.chunkLength);

                sources::Array vectorData = {
					std::move(binaryData),
					MimeType::GltfBuffer,
				};
                glbBuffer = std::move(vectorData);
            }
        } else {
            const span<const std::byte> glbBytes(reinterpret_cast<std::byte*>(buffer->bufferPointer + offset), binaryChunk.chunkLength);
            sources::ByteView glbByteView = {};
            glbByteView.bytes = glbBytes;
            glbByteView.mimeType = MimeType::GltfBuffer;
            glbBuffer = glbByteView;
        }
    }

	return parse(root, categories);
}

void fg::Parser::setBufferAllocationCallback(BufferMapCallback* mapCallback, BufferUnmapCallback* unmapCallback) noexcept {
	if (mapCallback == nullptr)
		unmapCallback = nullptr;
	config.mapCallback = mapCallback;
	config.unmapCallback = unmapCallback;
}

void fg::Parser::setBase64DecodeCallback(Base64DecodeCallback* decodeCallback) noexcept {
    config.decodeCallback = decodeCallback;
}

void fg::Parser::setUserPointer(void* pointer) noexcept {
    config.userPointer = pointer;
}
#pragma endregion

#pragma region Exporter
void fg::prettyPrintJson(std::string& json) {
    std::size_t i = 0;
    std::size_t depth = 0;
    auto insertNewline = [&i, &depth, &json]() {
        json.insert(i, 1, '\n');
        json.insert(i + 1, depth, '\t');
        i += 1 + depth;
    };

    while (i < json.size()) {
        if (json[i] == '"') {
            // Skip to the end of the string
            do {
                ++i;
                if (json[i] == '"' && json[i - 1] != '\\') {
                    break;
                }
            } while (true);
            ++i; // Skip over the last "
        }

        switch (json[i]) {
            case '{': case '[':
                ++depth;
                ++i; // Insert \n after the character
                insertNewline();
                break;
            case '}': case ']':
                --depth;
                insertNewline();
                ++i; // Insert \n before the character
                break;
            case ',':
                ++i;  // Insert \n after the character
                insertNewline();
                break;
            default:
                ++i;
                break;
        }
    }
}

std::string fg::escapeString(std::string_view string) {
    std::string ret(string);
    std::size_t i = 0;
    do {
        switch (ret[i]) {
            case '\"': {
                const std::string_view s = "\\\"";
                ret.replace(i, 1, s);
                i += s.size();
                break;
            }
            case '\\': {
                const std::string_view s = "\\\\";
                ret.replace(i, 1, s);
                i += s.size();
                break;
            }
        }
        ++i;
    } while (i < ret.size());
    return ret;
}

auto fg::stringifyExtensionBits(Extensions extensions) -> decltype(Asset::extensionsRequired) {
	decltype(Asset::extensionsRequired) stringified;
	for (std::uint8_t i = 0; i < std::numeric_limits<std::underlying_type_t<Extensions>>::digits; ++i) {
		auto curExtension = static_cast<Extensions>(1 << i);
		if ((extensions & curExtension) == Extensions::None)
			continue;

		// Find the stringified extension name
		for (const auto& [name, ext] : extensionStrings) {
			if (ext == curExtension) {
				stringified.emplace_back(name);
				break;
			}
		}
	}
	return stringified;
}

void fg::Exporter::setBufferPath(std::filesystem::path folder) {
    if (!folder.is_relative()) {
        return;
    }
    bufferFolder = std::move(folder);
}

void fg::Exporter::setImagePath(std::filesystem::path folder) {
    if (!folder.is_relative()) {
        return;
    }
    imageFolder = std::move(folder);
}

void fg::Exporter::writeAccessors(const Asset& asset, std::string& json) {
	if (asset.accessors.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	json += R"("accessors":[)";
	for (auto it = asset.accessors.begin(); it != asset.accessors.end(); ++it) {
		json += '{';

		if (it->byteOffset != 0) {
			json += "\"byteOffset\":" + std::to_string(it->byteOffset) + ',';
		}

		json += "\"count\":" + std::to_string(it->count) + ',';
		json += R"("type":")" + std::string(getAccessorTypeName(it->type)) + "\",";
		json += "\"componentType\":" + std::to_string(getGLComponentType(it->componentType));

		if (it->normalized) {
			json += ",\"normalized\":true";
		}

		if (it->bufferViewIndex.has_value()) {
			json += ",\"bufferView\":" + std::to_string(it->bufferViewIndex.value());
		}

		auto writeMinMax = [&](const decltype(Accessor::max)& ref, std::string_view name) {
			if (std::holds_alternative<std::monostate>(ref))
				return;
			json += ",\"" + std::string(name) + "\":[";
			std::visit(visitor {
				[](std::monostate) {},
				[&](auto arg) {
					for (auto it = arg.begin(); it != arg.end(); ++it) {
						json += std::to_string(*it);
						if (std::distance(arg.begin(), it) + 1 < arg.size())
							json += ',';
					}
				}
			}, ref);
			json += ']';
		};
		writeMinMax(it->max, "max");
		writeMinMax(it->min, "min");

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';

		json += '}';
		if (std::distance(asset.accessors.begin(), it) + 1 < asset.accessors.size())
			json += ',';
	}
	json += ']';
}

void fg::Exporter::writeBuffers(const Asset& asset, std::string& json) {
	if (asset.buffers.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	json += "\"buffers\":[";
	for (auto it = asset.buffers.begin(); it != asset.buffers.end(); ++it) {
		json += '{';

        auto bufferIdx = static_cast<std::size_t>(std::distance(asset.buffers.begin(), it));
		std::visit(visitor {
			[&](auto arg) {
				// Covers BufferView and CustomBuffer.
				errorCode = Error::InvalidGltf;
			},
			[&](const sources::Array& vector) {
                if (bufferIdx == 0 && exportingBinary) {
                    bufferPaths.emplace_back(std::nullopt);
                    return;
                }
                auto path = getBufferFilePath(asset, bufferIdx);
                json += std::string(R"("uri":")") + fg::escapeString(path.string()) + '"' + ',';
                bufferPaths.emplace_back(path);
			},
			[&](const sources::Vector& vector) {
				if (bufferIdx == 0 && exportingBinary) {
					bufferPaths.emplace_back(std::nullopt);
					return;
				}
				auto path = getBufferFilePath(asset, bufferIdx);
				json += std::string(R"("uri":")") + fg::escapeString(path.string()) + '"' + ',';
				bufferPaths.emplace_back(path);
			},
			[&](const sources::ByteView& view) {
                auto path = getBufferFilePath(asset, bufferIdx);
                json += std::string(R"("uri":")") + fg::escapeString(path.string()) + '"' + ',';
                bufferPaths.emplace_back(path);
			},
			[&](const sources::URI& uri) {
				json += std::string(R"("uri":")") + fg::escapeString(uri.uri.string()) + '"' + ',';
                bufferPaths.emplace_back(std::nullopt);
			},
			[&](const sources::Fallback& fallback) {
				json += R"("extensions":{"EXT_meshopt_compression":{"fallback":true}},)";
				bufferPaths.emplace_back(std::nullopt);
			},
		}, it->data);

		json += "\"byteLength\":" + std::to_string(it->byteLength);

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';
		json += '}';
		if (std::distance(asset.buffers.begin(), it) + 1 < asset.buffers.size())
			json += ',';
	}
	json += "]";
}

void fg::Exporter::writeBufferViews(const Asset& asset, std::string& json) {
	if (asset.bufferViews.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	json += "\"bufferViews\":[";
	for (auto it = asset.bufferViews.begin(); it != asset.bufferViews.end(); ++it) {
		json += '{';

		json += "\"buffer\":" + std::to_string(it->bufferIndex) + ',';
		json += "\"byteLength\":" + std::to_string(it->byteLength);

		if (it->byteOffset != 0) {
			json += ",\"byteOffset\":" + std::to_string(it->byteOffset);
		}

		if (it->byteStride.has_value()) {
			json += ",\"byteStride\":" + std::to_string(it->byteStride.value());
		}

		if (it->target.has_value()) {
			json += ",\"target\":" + std::to_string(to_underlying(it->target.value()));
		}

        if (it->meshoptCompression != nullptr) {
            json += R"(,"extensions":{"EXT_meshopt_compression":{)";
            const auto& meshopt = *it->meshoptCompression;
            json += "\"buffer\":" + std::to_string(meshopt.bufferIndex) + ',';
            if (meshopt.byteOffset != 0) {
                json += ",\"byteOffset\":" + std::to_string(meshopt.byteOffset);
            }
            json += "\"byteLength\":" + std::to_string(meshopt.byteLength);
            json += ",\"byteStride\":" + std::to_string(meshopt.byteStride);
            json += ",\"count\":" + std::to_string(meshopt.count);

            json += ",\"mode\":";
            if (meshopt.mode == MeshoptCompressionMode::Attributes) {
                json += "\"ATTRIBUTES\"";
            } else if (meshopt.mode == MeshoptCompressionMode::Triangles) {
                json += "\"TRIANGLES\"";
            } else if (meshopt.mode == MeshoptCompressionMode::Indices) {
                json += "\"INDICES\"";
            }
            if (meshopt.filter != MeshoptCompressionFilter::None) {
                json += ",\"filter\":";
                if (meshopt.filter == MeshoptCompressionFilter::Exponential) {
                    json += "\"EXPONENTIAL\"";
                } else if (meshopt.filter == MeshoptCompressionFilter::Quaternion) {
                    json += "\"QUATERNION\"";
                } else if (meshopt.filter == MeshoptCompressionFilter::Octahedral) {
                    json += "\"OCTAHEDRAL\"";
                }
            }
            json += "}}";
        }

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';

		json += '}';
		if (std::distance(asset.bufferViews.begin(), it) + 1 < asset.bufferViews.size())
			json += ',';
	}
	json += ']';
}

void fg::Exporter::writeCameras(const Asset& asset, std::string& json) {
	if (asset.cameras.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	json += "\"cameras\":[";
	for (auto it = asset.cameras.begin(); it != asset.cameras.end(); ++it) {
		json += '{';

		std::visit(visitor {
			[](auto arg) {},
			[&](const Camera::Perspective& perspective) {
				json += "\"perspective\":{";

				if (perspective.aspectRatio.has_value()) {
					json += "\"aspectRatio\":" + std::to_string(perspective.aspectRatio.value()) + ',';
				}

				json += "\"yfov\":" + std::to_string(perspective.yfov) + ',';

				if (perspective.zfar.has_value()) {
					json += "\"zfar\":" + std::to_string(perspective.zfar.value()) + ',';
				}

				json += "\"znear\":" + std::to_string(perspective.znear);

				json += R"(},"type":"perspective")";
			},
			[&](const Camera::Orthographic& orthographic) {
				json += "\"orthographic\":{";
				json += "\"xmag\":" + std::to_string(orthographic.xmag) + ',';
				json += "\"ymag\":" + std::to_string(orthographic.ymag) + ',';
				json += "\"zfar\":" + std::to_string(orthographic.zfar) + ',';
				json += "\"znear\":" + std::to_string(orthographic.znear);
				json += R"(},"type":"orthographic")";
			}
		}, it->camera);

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';

		json += '}';
		if (std::distance(asset.cameras.begin(), it) + 1 < asset.cameras.size())
			json += ',';
	}
	json += ']';
}

void fg::Exporter::writeImages(const Asset& asset, std::string& json) {
	if (asset.images.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	json += "\"images\":[";
	for (auto it = asset.images.begin(); it != asset.images.end(); ++it) {
		json += '{';

        auto imageIdx = std::distance(asset.images.begin(), it);
		std::visit(visitor {
			[&](auto arg) {
				errorCode = Error::InvalidGltf;
			},
            [&](const sources::BufferView& bufferView) {
                json += std::string(R"("bufferView":)") + std::to_string(bufferView.bufferViewIndex) + ',';
				json += std::string(R"("mimeType":")") + std::string(getMimeTypeString(bufferView.mimeType)) + '"';
                imagePaths.emplace_back(std::nullopt);
            },
            [&](const sources::Array& vector) {
                auto path = getImageFilePath(asset, imageIdx, vector.mimeType);
                json += std::string(R"("uri":")") + fg::escapeString(path.string()) + '"';
				if (vector.mimeType != MimeType::None) {
					json += std::string(R"(,"mimeType":")") + std::string(getMimeTypeString(vector.mimeType)) + '"';
				}
                imagePaths.emplace_back(path);
            },
			[&](const sources::Vector& vector) {
				auto path = getImageFilePath(asset, imageIdx, vector.mimeType);
				json += std::string(R"("uri":")") + fg::escapeString(path.string()) + '"';
				if (vector.mimeType != MimeType::None) {
					json += std::string(R"(,"mimeType":")") + std::string(getMimeTypeString(vector.mimeType)) + '"';
				}
				imagePaths.emplace_back(path);
			},
			[&](const sources::URI& uri) {
				json += std::string(R"("uri":")") + fg::escapeString(uri.uri.string()) + '"';
                imagePaths.emplace_back(std::nullopt);
			},
		}, it->data);

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';
		json += '}';
		if (std::distance(asset.images.begin(), it) + 1 < asset.images.size())
			json += ',';
	}
	json += ']';
}

void fg::Exporter::writeLights(const Asset& asset, std::string& json) {
	if (asset.lights.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	json += R"("KHR_lights_punctual":{"lights":[)";
	for (auto it = asset.lights.begin(); it != asset.lights.end(); ++it) {
		json += '{';

		// [1.0f, 1.0f, 1.0f] is the default.
		if (it->color[0] != 1.0f && it->color[1] != 1.0f && it->color[2] != 1.0f) {
			json += R"("color":[)";
			json += std::to_string(it->color[0]) + ',' + std::to_string(it->color[1]) + ',' + std::to_string(it->color[2]);
			json += "],";
		}

		if (it->intensity != 1.0f) {
			json += R"("intensity":)" + std::to_string(it->intensity) + ',';
		}

		switch (it->type) {
			case LightType::Directional: {
				json += R"("type":"directional")";
				break;
			}
			case LightType::Spot: {
				json += R"("type":"spot")";
				break;
			}
			case LightType::Point: {
				json += R"("type":"point")";
				break;
			}
		}

		if (it->range.has_value()) {
			json += R"(,"range":)" + std::to_string(it->range.value());
		}

		if (it->type == LightType::Spot) {
			if (it->innerConeAngle.has_value())
				json += R"("innerConeAngle":)" + std::to_string(it->innerConeAngle.value()) + ',';

			if (it->outerConeAngle.has_value())
				json += R"("outerConeAngle":)" + std::to_string(it->outerConeAngle.value()) + ',';
		}

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';
		json += '}';
		if (std::distance(asset.lights.begin(), it) + 1 < asset.lights.size())
			json += ',';
	}
	json += "]}";
}

void fg::Exporter::writeMaterials(const Asset& asset, std::string& json) {
	if (asset.materials.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	json += "\"materials\":[";
	for (auto it = asset.materials.begin(); it != asset.materials.end(); ++it) {
		json += '{';

		json += "\"pbrMetallicRoughness\":{";
		if (it->pbrData.baseColorFactor != std::array<num, 4>{{1.0f, 1.0f, 1.0f, 1.0f}}) {
			json += R"("baseColorFactor":[)";
			json += std::to_string(it->pbrData.baseColorFactor[0]) + ',' + std::to_string(it->pbrData.baseColorFactor[1]) + ',' +
				std::to_string(it->pbrData.baseColorFactor[2]) + ',' + std::to_string(it->pbrData.baseColorFactor[3]);
			json += "]";
		}

		if (it->pbrData.baseColorTexture.has_value()) {
			if (json.back() != '{') json += ',';
			json += "\"baseColorTexture\":";
			writeTextureInfo(json, &it->pbrData.baseColorTexture.value());
		}

		if (it->pbrData.metallicFactor != 1.0f) {
			if (json.back() != '{') json += ',';
			json += "\"metallicFactor\":" + std::to_string(it->pbrData.metallicFactor);
		}

		if (it->pbrData.roughnessFactor != 1.0f) {
			if (json.back() != '{') json += ',';
			json += "\"roughnessFactor\":" + std::to_string(it->pbrData.roughnessFactor);
		}

		if (it->pbrData.metallicRoughnessTexture.has_value()) {
			if (json.back() != '{') json += ',';
			json += "\"metallicRoughnessTexture\":";
			writeTextureInfo(json, &it->pbrData.metallicRoughnessTexture.value());
		}

		json += '}';

		if (it->normalTexture.has_value()) {
			if (json.back() != ',') json += ',';
			json += "\"normalTexture\":";
			writeTextureInfo(json, &it->normalTexture.value(), TextureInfoType::NormalTexture);
		}

		if (it->occlusionTexture.has_value()) {
			if (json.back() != ',') json += ',';
			json += "\"occlusionTexture\":";
			writeTextureInfo(json, &it->occlusionTexture.value(), TextureInfoType::OcclusionTexture);
		}

		if (it->emissiveTexture.has_value()) {
			if (json.back() != ',') json += ',';
			json += "\"emissiveTexture\":";
			writeTextureInfo(json, &it->emissiveTexture.value());
		}

		if (it->emissiveFactor != std::array<num, 3>{{.0f, .0f, .0f}}) {
			if (json.back() != ',') json += ',';
			json += R"("emissiveFactor":[)";
			json += std::to_string(it->emissiveFactor[0]) + ',' + std::to_string(it->emissiveFactor[1]) + ',' + std::to_string(it->emissiveFactor[2]);
			json += "],";
		}

		if (it->alphaMode != AlphaMode::Opaque) {
			if (json.back() != ',') json += ',';
			json += R"("alphaMode":)";
			if (it->alphaMode == AlphaMode::Blend) {
				json += "\"BLEND\"";
			} else if (it->alphaMode == AlphaMode::Mask) {
				json += "\"MASK\"";
			}
		}

		if (it->alphaMode == AlphaMode::Mask && it->alphaCutoff != 0.5f) {
			if (json.back() != ',') json += ',';
			json += R"("alphaCutoff":)" + std::to_string(it->alphaCutoff);
		}

		if (it->doubleSided) {
			if (json.back() != ',') json += ',';
			json += R"("doubleSided":true)";
		}

		if (json.back() != ',') json += ',';
		json += R"("extensions":{)";

		if (it->anisotropy) {
			json += R"("KHR_materials_anisotropy":{)";
			if (it->anisotropy->anisotropyStrength != 0.0f) {
				json += R"("anisotropyStrength":)" + std::to_string(it->anisotropy->anisotropyStrength);
			}
			if (it->anisotropy->anisotropyRotation != 0.0f) {
				if (json.back() != '{') json += ',';
				json += R"("anisotropyRotation":)" + std::to_string(it->anisotropy->anisotropyRotation);
			}
			if (it->anisotropy->anisotropyTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"anisotropyTexture\":";
				writeTextureInfo(json, &it->anisotropy->anisotropyTexture.value());
			}
			json += '}';
		}

		if (it->clearcoat) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_clearcoat":{)";
			if (it->clearcoat->clearcoatFactor != 0.0f) {
				json += R"("clearcoatFactor":)" + std::to_string(it->clearcoat->clearcoatFactor);
			}
			if (it->clearcoat->clearcoatTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"clearcoatTexture\":";
				writeTextureInfo(json, &it->clearcoat->clearcoatTexture.value());
			}
			if (it->clearcoat->clearcoatRoughnessFactor != 0.0f) {
				if (json.back() != '{') json += ',';
				json += R"("clearcoatRoughnessFactor":)" + std::to_string(it->clearcoat->clearcoatRoughnessFactor);
			}
			if (it->clearcoat->clearcoatRoughnessTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"clearcoatRoughnessTexture\":";
				writeTextureInfo(json, &it->clearcoat->clearcoatRoughnessTexture.value());
			}
			if (it->clearcoat->clearcoatNormalTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"clearcoatNormalTexture\":";
				writeTextureInfo(json, &it->clearcoat->clearcoatNormalTexture.value());
			}
			json += '}';
		}

		if (it->emissiveStrength != 1.0f) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_emissive_strength":{"emissiveStrength":)" + std::to_string(it->emissiveStrength) + '}';
		}

		if (it->ior != 1.5f) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_ior":{"ior":)" + std::to_string(it->ior) + '}';
		}

		if (it->iridescence) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_iridescence":{)";
			if (it->iridescence->iridescenceFactor != 0.0f) {
				json += R"("iridescenceFactor":)" + std::to_string(it->iridescence->iridescenceFactor);
			}
			if (it->iridescence->iridescenceTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"iridescenceTexture\":";
				writeTextureInfo(json, &it->iridescence->iridescenceTexture.value());
			}
			if (it->iridescence->iridescenceIor != 1.3f) {
				if (json.back() != '{') json += ',';
				json += R"("iridescenceIor":)" + std::to_string(it->iridescence->iridescenceIor);
			}
			if (it->iridescence->iridescenceThicknessMinimum != 100.0f) {
				if (json.back() != '{') json += ',';
				json += R"("iridescenceThicknessMinimum":)" + std::to_string(it->iridescence->iridescenceThicknessMinimum);
			}
			if (it->iridescence->iridescenceThicknessMaximum != 400.0f) {
				if (json.back() != '{') json += ',';
				json += R"("iridescenceThicknessMaximum":)" + std::to_string(it->iridescence->iridescenceThicknessMaximum);
			}
			if (it->iridescence->iridescenceThicknessTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"iridescenceThicknessTexture\":";
				writeTextureInfo(json, &it->iridescence->iridescenceThicknessTexture.value());
			}
			json += '}';
		}

		if (it->sheen) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_sheen":{)";
			if (it->sheen->sheenColorFactor != std::array<num, 3>{{.0f, .0f, .0f}}) {
				json += R"("sheenColorFactor":[)" +
					std::to_string(it->sheen->sheenColorFactor[0]) + ',' +
					std::to_string(it->sheen->sheenColorFactor[1]) + ',' +
					std::to_string(it->sheen->sheenColorFactor[2]) + ']';
			}
			if (it->sheen->sheenColorTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"sheenColorTexture\":";
				writeTextureInfo(json, &it->sheen->sheenColorTexture.value());
			}
			if (it->sheen->sheenRoughnessFactor != 0.0f) {
				if (json.back() != '{') json += ',';
				json += R"("sheenRoughnessFactor":)" + std::to_string(it->sheen->sheenRoughnessFactor);
			}
			if (it->sheen->sheenRoughnessTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"sheenRoughnessTexture\":";
				writeTextureInfo(json, &it->sheen->sheenRoughnessTexture.value());
			}
			json += '}';
		}

		if (it->specular) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_specular":{)";
			if (it->specular->specularFactor != 1.0f) {
				json += R"("specularFactor":)" + std::to_string(it->specular->specularFactor);
			}
			if (it->specular->specularTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"specularTexture\":";
				writeTextureInfo(json, &it->specular->specularTexture.value());
			}
			if (it->specular->specularColorFactor != std::array<num, 3>{{1.0f, 1.0f, 1.0f}}) {
				if (json.back() != '{') json += ',';
				json += R"("specularColorFactor":[)" +
						std::to_string(it->specular->specularColorFactor[0]) + ',' +
						std::to_string(it->specular->specularColorFactor[1]) + ',' +
						std::to_string(it->specular->specularColorFactor[2]) + ']';
			}
			if (it->specular->specularColorTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"specularColorTexture\":";
				writeTextureInfo(json, &it->specular->specularColorTexture.value());
			}
			json += '}';
		}

		if (it->transmission) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_transmission":{)";
			if (it->transmission->transmissionFactor != 0.0f) {
				json += R"("transmissionFactor":)" + std::to_string(it->transmission->transmissionFactor);
			}
			if (it->transmission->transmissionTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"transmissionTexture\":";
				writeTextureInfo(json, &it->transmission->transmissionTexture.value());
			}
			json += '}';
		}

		if (it->unlit) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_unlit":{})";
		}

		if (it->volume) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_volume":{)";
			if (it->volume->thicknessFactor != 0.0f) {
				json += R"("thicknessFactor":)" + std::to_string(it->volume->thicknessFactor);
			}
			if (it->volume->thicknessTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"thicknessTexture\":";
				writeTextureInfo(json, &it->volume->thicknessTexture.value());
			}
			if (it->volume->attenuationDistance != std::numeric_limits<num>::infinity()) {
				if (json.back() != '{') json += ',';
				json += R"("attenuationDistance":)" + std::to_string(it->volume->attenuationDistance);
			}
			if (it->volume->attenuationColor != std::array<num, 3>{{1.0f, 1.0f, 1.0f}}) {
				if (json.back() != '{') json += ',';
				json += R"("attenuationColor":[)" +
						std::to_string(it->volume->attenuationColor[0]) + ',' +
						std::to_string(it->volume->attenuationColor[1]) + ',' +
						std::to_string(it->volume->attenuationColor[2]) + ']';
			}
			json += '}';
		}

		if (it->packedNormalMetallicRoughnessTexture.has_value()) {
			if (json.back() == '}') json += ',';
			json += R"("MSFT_packing_normalRoughnessMetallic":{"normalRoughnessMetallicTexture":)";
			writeTextureInfo(json, &it->packedNormalMetallicRoughnessTexture.value());
			json += '}';
		}

		if (it->packedOcclusionRoughnessMetallicTextures) {
			if (json.back() == '}') json += ',';
			json += R"("MSFT_packing_normalRoughnessMetallic":{)";
			if (it->packedOcclusionRoughnessMetallicTextures->occlusionRoughnessMetallicTexture.has_value()) {
				json += R"("occlusionRoughnessMetallicTexture":)";
				writeTextureInfo(json, &it->packedOcclusionRoughnessMetallicTextures->occlusionRoughnessMetallicTexture.value());
			}
			if (it->packedOcclusionRoughnessMetallicTextures->roughnessMetallicOcclusionTexture.has_value()) {
				json += R"("roughnessMetallicOcclusionTexture":)";
				writeTextureInfo(json, &it->packedOcclusionRoughnessMetallicTextures->roughnessMetallicOcclusionTexture.value());
			}
			if (it->packedOcclusionRoughnessMetallicTextures->normalTexture.has_value()) {
				json += R"("normalTexture":)";
				writeTextureInfo(json, &it->packedOcclusionRoughnessMetallicTextures->normalTexture.value());
			}
			json += '}';
		}

		json += '}';

		if (!it->name.empty()) {
			if (json.back() != ',') json += ',';
			json += R"("name":")" + fg::escapeString(it->name) + '"';
		}
		json += '}';
		if (std::distance(asset.materials.begin(), it) + 1 < asset.materials.size())
			json += ',';
	}
	json += ']';
}

void fg::Exporter::writeMeshes(const Asset& asset, std::string& json) {
	if (asset.meshes.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	json += "\"meshes\":[";
	for (auto it = asset.meshes.begin(); it != asset.meshes.end(); ++it) {
		json += '{';

        if (!it->primitives.empty()) {
            json += R"("primitives":[)";
            auto itp = it->primitives.begin();
            while (itp != it->primitives.end()) {
                json += '{';

                {
                    json += R"("attributes":{)";
                    for (auto ita = itp->attributes.begin(); ita != itp->attributes.end(); ++ita) {
                        json += '"' + std::string(ita->first) + "\":" + std::to_string(ita->second);
                        if (std::distance(itp->attributes.begin(), ita) + 1 < itp->attributes.size())
                            json += ',';
                    }
                    json += '}';
                }

                if (itp->indicesAccessor.has_value()) {
                    json += R"(,"indices":)" + std::to_string(itp->indicesAccessor.value());
                }

                if (itp->materialIndex.has_value()) {
                    json += R"(,"material":)" + std::to_string(itp->materialIndex.value());
                }

                if (itp->type != PrimitiveType::Triangles) {
                    json += R"(,"type":)" + std::to_string(to_underlying(itp->type));
                }

                json += '}';
                ++itp;
                if (std::distance(it->primitives.begin(), itp) < it->primitives.size())
                    json += ',';
            }
            json += ']';
        }

		if (!it->weights.empty()) {
			if (json.back() != '{')
				json += ',';
			json += R"("weights":[)";
			auto itw = it->weights.begin();
			while (itw != it->weights.end()) {
				json += std::to_string(*itw);
				++itw;
				if (std::distance(it->weights.begin(), itw) < it->weights.size())
					json += ',';
			}
			json += ']';
		}

		if (!it->name.empty()) {
            if (json.back() != '{')
                json += ',';
            json += R"("name":")" + fg::escapeString(it->name) + '"';
        }
		json += '}';
		if (std::distance(asset.meshes.begin(), it) + 1 < asset.meshes.size())
			json += ',';
	}
	json += ']';
}

void fg::Exporter::writeNodes(const Asset& asset, std::string& json) {
	if (asset.nodes.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	json += "\"nodes\":[";
	for (auto it = asset.nodes.begin(); it != asset.nodes.end(); ++it) {
		json += '{';

		if (it->meshIndex.has_value()) {
			json += R"("mesh":)" + std::to_string(it->meshIndex.value());
		}
		if (it->cameraIndex.has_value()) {
			if (json.back() != '{')
				json += ',';
			json += R"("camera":)" + std::to_string(it->cameraIndex.value());
		}
		if (it->skinIndex.has_value()) {
			if (json.back() != '{')
				json += ',';
			json += R"("skin":)" + std::to_string(it->skinIndex.value());
		}

		if (!it->children.empty()) {
            if (json.back() != '{')
                json += ',';
			json += R"("children":[)";
			auto itc = it->children.begin();
			while (itc != it->children.end()) {
				json += std::to_string(*itc);
				++itc;
				if (std::distance(it->children.begin(), itc) < it->children.size())
					json += ',';
			}
			json += ']';
		}

		if (!it->weights.empty()) {
            if (json.back() != '{')
                json += ',';
			json += R"("weights":[)";
			auto itw = it->weights.begin();
			while (itw != it->weights.end()) {
				json += std::to_string(*itw);
				++itw;
				if (std::distance(it->weights.begin(), itw) < it->weights.size())
					json += ',';
			}
			json += ']';
		}

		std::visit(visitor {
			[&](const TRS& trs) {
				if (trs.rotation != std::array<num, 4>{{.0f, .0f, .0f, 1.0f}}) {
                    if (json.back() != '{')
                        json += ',';
					json += R"("rotation":[)";
					json += std::to_string(trs.rotation[0]) + ',' + std::to_string(trs.rotation[1]) + ',' + std::to_string(trs.rotation[2]) + ',' + std::to_string(trs.rotation[3]);
					json += "]";
				}

				if (trs.scale != std::array<num, 3>{{1.0f, 1.0f, 1.0f}}) {
                    if (json.back() != '{')
                        json += ',';
					json += R"("scale":[)";
					json += std::to_string(trs.scale[0]) + ',' + std::to_string(trs.scale[1]) + ',' + std::to_string(trs.scale[2]);
					json += "]";
				}

				if (trs.translation != std::array<num, 3>{{.0f, .0f, .0f}}) {
                    if (json.back() != '{')
                        json += ',';
					json += R"("translation":[)";
					json += std::to_string(trs.translation[0]) + ',' + std::to_string(trs.translation[1]) + ',' + std::to_string(trs.translation[2]);
					json += "]";
				}
			},
			[&](const Node::TransformMatrix& matrix) {
				if (json.back() != '{')
					json += ',';
				json += R"("matrix":[)";
				for (std::size_t i = 0; i < matrix.size(); ++i) {
					json += std::to_string(matrix[i]);
					if (i + 1 < matrix.size()) {
						json += ',';
					}
				}
				json += ']';
			},
		}, it->transform);

        if (!it->instancingAttributes.empty()) {
			if (json.back() != '{') json += ',';
            json += R"("extensions":{"EXT_mesh_gpu_instancing":{"attributes":{)";
            for (auto ait = it->instancingAttributes.begin(); ait != it->instancingAttributes.end(); ++ait) {
                json += '"' + std::string(ait->first) + "\":" + std::to_string(ait->second);
                if (std::distance(it->instancingAttributes.begin(), ait) + 1 < it->instancingAttributes.size())
                    json += ',';
            }
            json += "}}}";
        }

		if (!it->name.empty()) {
            if (json.back() != '{')
                json += ',';
			json += R"("name":")" + fg::escapeString(it->name) + '"';
		}
		json += '}';
		if (std::distance(asset.nodes.begin(), it) + 1 < asset.nodes.size())
			json += ',';
	}
	json += ']';
}

void fg::Exporter::writeSamplers(const Asset& asset, std::string& json) {
	if (asset.samplers.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	json += "\"samplers\":[";
	for (auto it = asset.samplers.begin(); it != asset.samplers.end(); ++it) {
		json += '{';

		if (it->magFilter.has_value()) {
			json += R"("magFilter":)" + std::to_string(to_underlying(it->magFilter.value()));
		}
		if (it->minFilter.has_value()) {
			if (json.back() != '{') json += ',';
			json += R"("minFilter":)" + std::to_string(to_underlying(it->minFilter.value()));
		}
		if (it->wrapS != Wrap::Repeat) {
			if (json.back() != '{') json += ',';
			json += R"("wrapS":)" + std::to_string(to_underlying(it->wrapS));
		}
		if (it->wrapT != Wrap::Repeat) {
			if (json.back() != '{') json += ',';
			json += R"("wrapTS":)" + std::to_string(to_underlying(it->wrapT));
		}

		if (!it->name.empty()) {
			if (json.back() != '{') json += ',';
			json += R"("name":")" + fg::escapeString(it->name) + '"';
		}
		json += '}';
		if (std::distance(asset.samplers.begin(), it) + 1 < asset.samplers.size())
			json += ',';
	}
	json += ']';
}

void fg::Exporter::writeScenes(const Asset& asset, std::string& json) {
	if (asset.scenes.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	if (asset.defaultScene.has_value()) {
		json += "\"scene\":" + std::to_string(asset.defaultScene.value()) + ',';
	}

	json += "\"scenes\":[";
	for (auto it = asset.scenes.begin(); it != asset.scenes.end(); ++it) {
		json += '{';

		json += R"("nodes":[)";
		auto itn = it->nodeIndices.begin();
		while (itn != it->nodeIndices.end()) {
			json += std::to_string(*itn);
			++itn;
			if (std::distance(it->nodeIndices.begin(), itn) < it->nodeIndices.size())
				json += ',';
		}
		json += ']';

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';
		json += '}';
		if (std::distance(asset.scenes.begin(), it) + 1 < asset.scenes.size())
			json += ',';
	}
	json += ']';
}

void fg::Exporter::writeSkins(const Asset& asset, std::string& json) {
	if (asset.skins.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	json += "\"skins\":[";
	for (auto it = asset.skins.begin(); it != asset.skins.end(); ++it) {
		json += '{';

		if (it->inverseBindMatrices.has_value())
			json += R"("inverseBindMatrices":)" + std::to_string(it->inverseBindMatrices.value()) + ',';

		if (it->skeleton.has_value())
			json += R"("skeleton":)" + std::to_string(it->skeleton.value()) + ',';

		json += R"("joints":[)";
		auto itj = it->joints.begin();
		while (itj != it->joints.end()) {
			json += std::to_string(*itj);
			++itj;
			if (std::distance(it->joints.begin(), itj) < it->joints.size())
				json += ',';
		}
		json += ']';

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';
		json += '}';
		if (std::distance(asset.skins.begin(), it) + 1 < asset.skins.size())
			json += ',';
	}
	json += ']';
}

void fg::Exporter::writeTextures(const Asset& asset, std::string& json) {
	if (asset.textures.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	json += "\"textures\":[";
	for (auto it = asset.textures.begin(); it != asset.textures.end(); ++it) {
		json += '{';

		if (it->samplerIndex.has_value())
			json += R"("sampler":)" + std::to_string(it->samplerIndex.value());

		if (it->imageIndex.has_value()) {
			if (json.back() != '{') json += ',';
			json += R"("source":)" + std::to_string(it->imageIndex.value());
		}

		if (it->basisuImageIndex.has_value() || it->ddsImageIndex.has_value() || it->webpImageIndex.has_value()) {
			if (json.back() != '{') json += ',';
			json += R"("extensions":{)";
			if (it->basisuImageIndex.has_value()) {
				json += R"("KHR_texture_basisu":{"source":)" + std::to_string(it->basisuImageIndex.value()) + '}';
			}
			if (it->ddsImageIndex.has_value()) {
				if (json.back() == '}') json += ',';
				json += R"("MSFT_texture_dds":{"source":)" + std::to_string(it->ddsImageIndex.value()) + '}';
			}
			if (it->webpImageIndex.has_value()) {
				if (json.back() == '}') json += ',';
				json += R"("EXT_texture_webp":{"source":)" + std::to_string(it->webpImageIndex.value()) + '}';
			}
			json += "}";
		}

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';
		json += '}';
		if (std::distance(asset.textures.begin(), it) + 1 < asset.textures.size())
			json += ',';
	}
	json += ']';
}

void fg::Exporter::writeExtensions(const fastgltf::Asset& asset, std::string& json) {
	if (json.back() == ']' || json.back() == '}')
		json += ',';
    json += "\"extensions\":{";

    writeLights(asset, json);

    json += '}';
}

fs::path fg::Exporter::getBufferFilePath(const Asset& asset, std::size_t index) {
    const auto& bufferName = asset.buffers[index].name;
    if (bufferName.empty()) {
        return bufferFolder / ("buffer" + std::to_string(index) + ".bin");
    } else {
        return bufferFolder / (bufferName + ".bin");
    }
    std::string name = bufferName.empty() ? "buffer" : std::string(bufferName);
    return bufferFolder / (name + std::to_string(index) + ".bin");
}

fs::path fg::Exporter::getImageFilePath(const Asset& asset, std::size_t index, MimeType mimeType) {
    std::string_view extension;
    switch (mimeType) {
        default:
        case MimeType::None:
            extension = ".bin";
            break;
        case MimeType::JPEG:
            extension = ".jpeg";
            break;
        case MimeType::PNG:
            extension = ".png";
            break;
    }

    const auto& imageName = asset.images[index].name;
    std::string name = imageName.empty() ? "image" : std::string(imageName);
    return imageFolder / (name + std::to_string(index) + std::string(extension));
}

std::string fg::Exporter::writeJson(const fastgltf::Asset &asset) {
    // Fairly rudimentary approach of just composing the JSON string using a std::string.
    std::string outputString;

    outputString += "{";

    // Write asset info
    outputString += "\"asset\":{";
    if (asset.assetInfo.has_value()) {
        if (!asset.assetInfo->copyright.empty())
            outputString += R"("copyright":")" + fg::escapeString(asset.assetInfo->copyright) + "\",";
        if (!asset.assetInfo->generator.empty())
            outputString += R"("generator":")" + fg::escapeString(asset.assetInfo->generator) + "\",";
        outputString += R"("version":")" + asset.assetInfo->gltfVersion + '"';
    } else {
        outputString += R"("generator":"fastgltf",)";
        outputString += R"("version":"2.0")";
    }
    outputString += '}';

	// Write extension usage info
	if (!asset.extensionsUsed.empty()) {
		if (outputString.back() != '{') outputString += ',';
		outputString += "\"extensionsUsed\":[";
		for (auto it = asset.extensionsUsed.begin(); it != asset.extensionsUsed.end(); ++it) {
			outputString += '\"' + *it + '\"';
			if (std::distance(asset.extensionsUsed.begin(), it) + 1 < asset.extensionsUsed.size())
				outputString += ',';
		}
		outputString += ']';
	}
	if (!asset.extensionsRequired.empty()) {
		if (outputString.back() != '{') outputString += ',';
		outputString += "\"extensionsRequired\":[";
		for (auto it = asset.extensionsRequired.begin(); it != asset.extensionsRequired.end(); ++it) {
			outputString += '\"' + *it + '\"';
			if (std::distance(asset.extensionsRequired.begin(), it) + 1 < asset.extensionsRequired.size())
				outputString += ',';
		}
		outputString += ']';
	}

    writeAccessors(asset, outputString);
    writeBuffers(asset, outputString);
    writeBufferViews(asset, outputString);
    writeCameras(asset, outputString);
    writeImages(asset, outputString);
    writeMaterials(asset, outputString);
    writeMeshes(asset, outputString);
    writeNodes(asset, outputString);
    writeSamplers(asset, outputString);
    writeScenes(asset, outputString);
    writeSkins(asset, outputString);
    writeTextures(asset, outputString);
    writeExtensions(asset, outputString);

    outputString += "}";

    if (hasBit(options, ExportOptions::PrettyPrintJson)) {
        prettyPrintJson(outputString);
    }

    return outputString;
}

fg::Expected<fg::ExportResult<std::string>> fg::Exporter::writeGltfJson(const Asset& asset, ExportOptions nOptions) {
    bufferPaths.clear();
    imagePaths.clear();
    options = nOptions;
	exportingBinary = false;

    if (hasBit(options, ExportOptions::ValidateAsset)) {
        auto validation = validate(asset);
        if (validation != Error::None) {
            return Expected<ExportResult<std::string>>{errorCode};
        }
    }

    // Fairly rudimentary approach of just composing the JSON string using a std::string.
    std::string outputString = writeJson(asset);
    if (errorCode != Error::None) {
        return Expected<ExportResult<std::string>> { errorCode };
    }

    ExportResult<std::string> result;
    result.output = std::move(outputString);
    result.bufferPaths = std::move(bufferPaths);
    result.imagePaths = std::move(imagePaths);
    return Expected { std::move(result) };
}

fg::Expected<fg::ExportResult<std::vector<std::byte>>> fg::Exporter::writeGltfBinary(const Asset& asset, ExportOptions nOptions) {
    bufferPaths.clear();
    imagePaths.clear();
    options = nOptions;
	exportingBinary = true;

    options &= (~ExportOptions::PrettyPrintJson);

    ExportResult<std::vector<std::byte>> result;
    auto json = writeJson(asset);
    if (errorCode != Error::None) {
        return Expected<ExportResult<std::vector<std::byte>>> { errorCode };
    }

    result.bufferPaths = std::move(bufferPaths);
    result.imagePaths = std::move(imagePaths);

	// TODO: Add ExportOption enumeration for disabling this?
    const bool withEmbeddedBuffer = !asset.buffers.empty()
			// We only support writing Vectors and ByteViews as embedded buffers
			&& (std::holds_alternative<sources::Array>(asset.buffers.front().data) || std::holds_alternative<sources::ByteView>(asset.buffers.front().data))
			&& asset.buffers.front().byteLength < std::numeric_limits<decltype(BinaryGltfChunk::chunkLength)>::max();

	// Align the JSON string to a 4-byte length
	auto alignedJsonLength = alignUp(json.size(), 4);
	json.resize(alignedJsonLength, ' '); // Needs to be padded with space (0x20) chars.

    std::size_t binarySize = sizeof(BinaryGltfHeader) + sizeof(BinaryGltfChunk) + json.size();
    if (withEmbeddedBuffer) {
        binarySize += sizeof(BinaryGltfChunk) + alignUp(asset.buffers.front().byteLength, 4);
    }

    result.output.resize(binarySize);
    auto write = [output = result.output.data()](const void* data, std::size_t size) mutable {
        std::memcpy(output, data, size);
        output += size;
    };

    BinaryGltfHeader header;
    header.magic = binaryGltfHeaderMagic;
    header.version = 2;
    header.length = binarySize;
    write(&header, sizeof header);

    BinaryGltfChunk jsonChunk;
    jsonChunk.chunkType = binaryGltfJsonChunkMagic;
    jsonChunk.chunkLength = json.size();
    write(&jsonChunk, sizeof jsonChunk);

    write(json.data(), json.size() * sizeof(decltype(json)::value_type));

    if (withEmbeddedBuffer) {
        const auto& buffer = asset.buffers.front();

        BinaryGltfChunk dataChunk;
        dataChunk.chunkType = binaryGltfDataChunkMagic;
        dataChunk.chunkLength = alignUp(buffer.byteLength, 4);
        write(&dataChunk, sizeof dataChunk);
		for (std::size_t i = 0; i < buffer.byteLength % 4; ++i) {
			static constexpr std::uint8_t zero = 0x0U;
			write(&zero, sizeof zero);
		}

		std::visit(visitor {
			[](auto arg) {},
			[&](sources::Array& vector) {
				write(vector.bytes.data(), buffer.byteLength);
			},
			[&](sources::Vector& vector) {
				write(vector.bytes.data(), buffer.byteLength);
			},
			[&](sources::ByteView& byteView) {
				write(byteView.bytes.data(), buffer.byteLength);
			},
		}, buffer.data);
    }

    return Expected { std::move(result) };
}

namespace fastgltf {
	void writeFile(const DataSource& dataSource, fs::path baseFolder, fs::path filePath) {
		std::visit(visitor {
			[](auto& arg) {},
			[&](const sources::Array& vector) {
				std::ofstream file(baseFolder / filePath, std::ios::out | std::ios::binary);
				file.write(reinterpret_cast<const char *>(vector.bytes.data()),
						   static_cast<std::streamsize>(vector.bytes.size()));
				file.close();
			},
			[&](const sources::Vector& vector) {
				std::ofstream file(baseFolder / filePath, std::ios::out | std::ios::binary);
				file.write(reinterpret_cast<const char *>(vector.bytes.data()),
						   static_cast<std::streamsize>(vector.bytes.size()));
				file.close();
			},
			[&](const sources::ByteView& view) {
				std::ofstream file(baseFolder / filePath, std::ios::out | std::ios::binary);
				file.write(reinterpret_cast<const char *>(view.bytes.data()),
						   static_cast<std::streamsize>(view.bytes.size()));
				file.close();
			},
		}, dataSource);
	}

    template<typename T>
    void writeFiles(const Asset& asset, ExportResult<T> &result, fs::path baseFolder) {
        for (std::size_t i = 0; i < asset.buffers.size(); ++i) {
            auto &path = result.bufferPaths[i];
            if (path.has_value()) {
                writeFile(asset.buffers[i].data, baseFolder, path.value());
            }
        }

        for (std::size_t i = 0; i < asset.images.size(); ++i) {
            auto &path = result.imagePaths[i];
            if (path.has_value()) {
				writeFile(asset.images[i].data, baseFolder, path.value());
            }
        }
    }
} // namespace fastgltf

fg::Error fg::FileExporter::writeGltfJson(const Asset& asset, std::filesystem::path target, ExportOptions options) {
    auto expected = Exporter::writeGltfJson(asset, options);

    if (!expected) {
        return expected.error();
    }
    auto& result = expected.get();

    std::ofstream file(target, std::ios::out);
    if (!file.is_open()) {
        return fg::Error::InvalidPath;
    }

    file << result.output;
    file.close();

    writeFiles(asset, result, target.parent_path());
    return Error::None;
}

fg::Error fg::FileExporter::writeGltfBinary(const Asset& asset, std::filesystem::path target, ExportOptions options) {
    auto expected = Exporter::writeGltfBinary(asset, options);

    if (!expected) {
        return expected.error();
    }
    auto& result = expected.get();

    std::ofstream file(target, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        return fg::Error::InvalidPath;
    }

    file.write(reinterpret_cast<const char*>(result.output.data()),
               static_cast<std::streamsize>(result.output.size()));

    writeFiles(asset, result, target.parent_path());
    return Error::None;
}
#pragma endregion

#ifdef _MSC_VER
#pragma warning(pop)
#endif
