/*
 * Copyright (C) 2022 - 2026 Sean Apeler
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

#if !defined(__cplusplus) || (!defined(_MSVC_LANG) && __cplusplus < 202002L) || (defined(_MSVC_LANG) && _MSVC_LANG < 202002L)
#error "fastgltf requires C++20"
#endif

#include <charconv>
#include <numeric>

#include <simdjson.h>
#include <simdutf.h>

#include <fastgltf/base64.hpp>
#include <fastgltf/crc32.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/glb.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>

namespace fg = fastgltf;
namespace fs = std::filesystem;

namespace fastgltf {
	namespace internal {
		struct JsonParser {
			simdjson::ondemand::parser parser;
		};
		struct JsonObject {
			simdjson::ondemand::object value;
		};
		struct JsonArray {
			simdjson::ondemand::array value;
		};

		/** GLBs are always little-endian, meaning we need to read the values accordingly */
		[[nodiscard]] inline auto readBinaryHeader(GltfDataGetter& getter) noexcept {
			std::array<std::byte, sizeof(BinaryGltfHeader)> bytes {};
			getter.read(bytes.data(), bytes.size());

			BinaryGltfHeader header {};
			readUint32LE(header.magic, &bytes[offsetof(BinaryGltfHeader, magic)]);
			readUint32LE(header.version, &bytes[offsetof(BinaryGltfHeader, version)]);
			readUint32LE(header.length, &bytes[offsetof(BinaryGltfHeader, length)]);
			return header;
		}

		[[nodiscard]] inline auto readBinaryChunk(GltfDataGetter& getter) noexcept {
			std::array<std::byte, sizeof(BinaryGltfChunk)> bytes {};
			getter.read(bytes.data(), bytes.size());

			BinaryGltfChunk chunk {};
			readUint32LE(chunk.chunkLength, &bytes[offsetof(BinaryGltfChunk, chunkLength)]);
			readUint32LE(chunk.chunkType, &bytes[offsetof(BinaryGltfChunk, chunkType)]);
			return chunk;
		}
	}

	[[nodiscard]] FASTGLTF_FORCEINLINE bool toNum(const double in, num& out) noexcept {
		out = static_cast<num>(in);
		return std::isfinite(out);
	}
	[[nodiscard]] FASTGLTF_FORCEINLINE bool toNum(const double in, optional<num>& out) noexcept {
		out = static_cast<num>(in);
		return std::isfinite(*out);
	}

	template <typename T>
	requires has_flag_traits<T> && (std::is_integral_v<T> || std::is_enum_v<T>)
	[[nodiscard]] FASTGLTF_FORCEINLINE bool isValidOptionalValue(std::uint64_t value) noexcept {
		const auto narrowed = static_cast<T>(value);
		return static_cast<std::uint64_t>(narrowed) == value &&
			!optional_flag_value<T>::is_empty(narrowed);
	}

	/**
	 * Checks if the given glTF index is a valid index and does not collide with the sentinel
	 * used with fastgltf::flagged_optional.
	 */
	[[nodiscard]] FASTGLTF_FORCEINLINE bool isValidIndex(std::uint64_t idx) noexcept {
		return isValidOptionalValue<std::size_t>(idx);
	}

	[[nodiscard]] inline bool isValidEnum(PrimitiveType value) noexcept {
		switch (value) {
			case PrimitiveType::Points:
			case PrimitiveType::Lines:
			case PrimitiveType::LineLoop:
			case PrimitiveType::LineStrip:
			case PrimitiveType::Triangles:
			case PrimitiveType::TriangleStrip:
			case PrimitiveType::TriangleFan:
				return true;
		}
		return false;
	}
	[[nodiscard]] inline bool isValidEnum(BufferTarget value) noexcept {
		switch (value) {
			case BufferTarget::ArrayBuffer:
			case BufferTarget::ElementArrayBuffer:
				return true;
		}
		return false;
	}
	[[nodiscard]] inline bool isValidEnum(Filter value) noexcept {
		switch (value) {
			case Filter::Nearest:
			case Filter::Linear:
			case Filter::NearestMipMapNearest:
			case Filter::LinearMipMapNearest:
			case Filter::NearestMipMapLinear:
			case Filter::LinearMipMapLinear:
				return true;
		}
		return false;
	}
	[[nodiscard]] inline bool isValidEnum(Wrap value) noexcept {
		switch (value) {
			case Wrap::ClampToEdge:
			case Wrap::MirroredRepeat:
			case Wrap::Repeat:
				return true;
		}
		return false;
	}

	template <typename T>
	requires std::is_enum_v<T>
	[[nodiscard]] FASTGLTF_FORCEINLINE bool toEnum(const std::uint64_t in, T& out) noexcept {
		out = static_cast<T>(in);
		return static_cast<std::uint64_t>(out) == in && isValidEnum(out);
	}
	template <typename T>
	requires std::is_enum_v<T>
	[[nodiscard]] FASTGLTF_FORCEINLINE bool toEnum(const std::uint64_t in, optional<T>& out) noexcept {
		if (optional_flag_value<T>::is_empty(static_cast<T>(in)))
			return false;
		out = static_cast<T>(in);
		return static_cast<std::uint64_t>(*out) == in && isValidEnum(*out);
	}

	template <typename Vec>
	[[nodiscard]] FASTGLTF_FORCEINLINE Error reserveElements(simdjson::ondemand::array& array, Vec& vec) {
		std::size_t count;
		if (array.count_elements().get(count) != simdjson::SUCCESS) [[unlikely]]
			return Error::InvalidJson;
		vec.reserve(count);
		return Error::None;
	}

	/** Converts the result of reading JSON with simdjson to a fastgltf Error as closely as possible */
	[[nodiscard]] constexpr Error toError(const simdjson::error_code error) noexcept {
		switch (error) {
			case simdjson::SUCCESS:
				return Error::None;
			case simdjson::INCORRECT_TYPE:
			case simdjson::NO_SUCH_FIELD:
			case simdjson::BIGINT_ERROR:
				return Error::InvalidGltf;
			case simdjson::NUMBER_OUT_OF_RANGE:
				return Error::ValueOutOfRange;
			default:
				return Error::InvalidJson;
		}
	}

	/**
	 * The reverse of toError, for calling functions which return an Error from a handler.
	 * Errors which toError can't return become a generic error.
	 */
	[[nodiscard]] constexpr simdjson::error_code toErrorCode(const Error error) noexcept {
		switch (error) {
			case Error::None:
				return simdjson::SUCCESS;
			case Error::InvalidGltf:
				return simdjson::INCORRECT_TYPE;
			case Error::ValueOutOfRange:
				return simdjson::NUMBER_OUT_OF_RANGE;
			default:
				return simdjson::UNEXPECTED_ERROR;
		}
	}

	/** Walks the JSON array, calling func with each element. func has to append to vec itself. */
	template <typename Vec, typename F>
	[[nodiscard]] simdjson::error_code parseArray(simdjson::ondemand::value value, Vec& vec,
			F&& func) {
		simdjson::ondemand::array array;
		if (const auto error = value.get_array().get(array); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		std::size_t count;
		if (const auto error = array.count_elements().get(count); error != simdjson::SUCCESS) [[unlikely]]
			return error;
		vec.reserve(count);

		for (auto element : array) {
			simdjson::ondemand::value elementValue;
			if (const auto error = element.get(elementValue); error != simdjson::SUCCESS) [[unlikely]]
				return error;
			if (const auto error = func(elementValue); error != simdjson::SUCCESS) [[unlikely]]
				return error;
		}
		return simdjson::SUCCESS;
	}

	template <typename Value, typename T>
	[[nodiscard]] simdjson::error_code parseNumArray(Value& value, T& out) noexcept {
		simdjson::ondemand::array array;
		if (const auto error = value.get_array().get(array); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		std::size_t i = 0;
		for (auto element : array) {
			if (i == out.size()) [[unlikely]]
				return simdjson::INCORRECT_TYPE;
			if (const auto error = element.get(out[i++]); error != simdjson::SUCCESS) [[unlikely]]
				return error;
		}
		return i == out.size() ? simdjson::SUCCESS : simdjson::INCORRECT_TYPE;
	}
} // namespace fastgltf

#pragma region tag_invoke
namespace simdjson {
	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::AccessorType& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (const auto error = val.get_string().get(str); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		if (str == "SCALAR") {
			out = AccessorType::Scalar;
		} else if (str == "VEC2") {
			out = AccessorType::Vec2;
		} else if (str == "VEC3") {
			out = AccessorType::Vec3;
		} else if (str == "VEC4") {
			out = AccessorType::Vec4;
		} else if (str == "MAT2") {
			out = AccessorType::Mat2;
		} else if (str == "MAT3") {
			out = AccessorType::Mat3;
		} else if (str == "MAT4") {
			out = AccessorType::Mat4;
		} else [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::ComponentType& out) noexcept {
		using namespace fastgltf;

		std::underlying_type_t<fastgltf::ComponentType> value;
		if (const auto error = val.get(value); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		out = fastgltf::internal::getComponentType(value);
		if (out == ComponentType::Invalid) [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::PrimitiveType& out) noexcept {
		using namespace fastgltf;

		std::uint64_t value;
		if (const auto error = val.get_uint64().get(value); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		if (!toEnum(value, out)) [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::BufferTarget& out) noexcept {
		using namespace fastgltf;

		std::uint64_t value;
		if (const auto error = val.get_uint64().get(value); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		if (!toEnum(value, out)) [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val,
			fastgltf::MeshoptCompressionMode& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (const auto error = val.get_string().get(str); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		if (str == "ATTRIBUTES") {
			out = MeshoptCompressionMode::Attributes;
		} else if (str == "TRIANGLES") {
			out = MeshoptCompressionMode::Triangles;
		} else if (str == "INDICES") {
			out = MeshoptCompressionMode::Indices;
		} else [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val,
			fastgltf::MeshoptCompressionFilter& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (const auto error = val.get_string().get(str); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		if (str == "NONE") {
			out = MeshoptCompressionFilter::None;
		} else if (str == "OCTAHEDRAL") {
			out = MeshoptCompressionFilter::Octahedral;
		} else if (str == "QUATERNION") {
			out = MeshoptCompressionFilter::Quaternion;
		} else if (str == "EXPONENTIAL") {
			out = MeshoptCompressionFilter::Exponential;
		} else if (str == "COLOR") {
			out = MeshoptCompressionFilter::Color;
		} else [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::Filter& out) noexcept {
		using namespace fastgltf;

		std::uint64_t value;
		if (const auto error = val.get_uint64().get(value); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		if (!toEnum(value, out)) [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::Wrap& out) noexcept {
		using namespace fastgltf;

		std::uint64_t value;
		if (const auto error = val.get_uint64().get(value); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		if (!toEnum(value, out)) [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::AlphaMode& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (const auto error = val.get_string().get(str); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		if (str == "OPAQUE") {
			out = AlphaMode::Opaque;
		} else if (str == "MASK") {
			out = AlphaMode::Mask;
		} else if (str == "BLEND") {
			out = AlphaMode::Blend;
		} else [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val,
			fastgltf::AnimationInterpolation& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (const auto error = val.get_string().get(str); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		if (str == "LINEAR") {
			out = AnimationInterpolation::Linear;
		} else if (str == "STEP") {
			out = AnimationInterpolation::Step;
		} else if (str == "CUBICSPLINE") {
			out = AnimationInterpolation::CubicSpline;
		} else [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::LightType& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (const auto error = val.get_string().get(str); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		if (str == "directional") {
			out = LightType::Directional;
		} else if (str == "spot") {
			out = LightType::Spot;
		} else if (str == "point") {
			out = LightType::Point;
		} else [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::CombineMode& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (const auto error = val.get_string().get(str); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		if (str == "average") {
			out = CombineMode::Average;
		} else if (str == "minimum") {
			out = CombineMode::Minimum;
		} else if (str == "maximum") {
			out = CombineMode::Maximum;
		} else if (str == "multiply") {
			out = CombineMode::Multiply;
		} else [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::DriveType& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (const auto error = val.get_string().get(str); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		if (str == "linear") {
			out = DriveType::Linear;
		} else if (str == "angular") {
			out = DriveType::Angular;
		} else [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::DriveMode& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (const auto error = val.get_string().get(str); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		if (str == "force") {
			out = DriveMode::Force;
		} else if (str == "acceleration") {
			out = DriveMode::Acceleration;
		} else [[unlikely]] {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::num& out) noexcept {
		double number;
		if (const auto error = val.get_double().get(number); error != SUCCESS) [[unlikely]]
			return error;
		return fastgltf::toNum(number, out) ? SUCCESS : INCORRECT_TYPE;
	}

	template <typename T, typename U>
	static error_code tag_invoke(deserialize_tag, T& val,
			fastgltf::flagged_optional<U>& out) noexcept {
		U value;
		if (const auto error = val.get(value); error != SUCCESS) [[unlikely]]
			return error;
		if (fastgltf::optional_flag_value<U>::is_empty(value)) [[unlikely]]
			return NUMBER_OUT_OF_RANGE;
		out = std::move(value);
		return SUCCESS;
	}

	template <typename T, typename U>
	static error_code tag_invoke(deserialize_tag, T& val,
		fastgltf::boxed_optional<U>& out) {
		return val.get(out.emplace());
	}

	template <typename T, std::size_t N>
	static error_code tag_invoke(deserialize_tag, T& val,
			fastgltf::math::vec<fastgltf::num, N>& out) noexcept {
		return fastgltf::parseNumArray(val, out);
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::math::nquat& out) noexcept {
		return fastgltf::parseNumArray(val, out);
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::math::nmat4x4& out) noexcept {
		ondemand::array array;
		if (const auto error = val.get_array().get(array); error != SUCCESS) [[unlikely]]
			return error;

		std::size_t i = 0;
		for (auto element : array) {
			if (i == 16) [[unlikely]]
				return INCORRECT_TYPE;
			if (const auto error = element.get(out.col(i / 4)[i % 4]); error != SUCCESS) [[unlikely]]
				return error;
			++i;
		}
		return i == 16 ? SUCCESS : INCORRECT_TYPE;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, std::vector<fastgltf::Attribute>& out) {
		ondemand::object object;
		if (const auto error = val.get_object().get(object); error != SUCCESS) [[unlikely]]
			return error;

		std::size_t count;
		if (const auto error = object.count_fields().get(count); error != SUCCESS) [[unlikely]]
			return error;
		out.reserve(count);

		// We iterate through the JSON object and write each key/pair value into the
		// attribute map. The keys are only validated in the validate() method.
		for (auto attribute : object) {
			std::string_view name;
			if (const auto error = attribute.unescaped_key().get(name); error != SUCCESS) [[unlikely]]
				return error;

			fastgltf::optional<std::size_t> accessorIndex;
			if (const auto error = attribute.value().get(accessorIndex); error != SUCCESS) [[unlikely]]
				return error;
			out.emplace_back(fastgltf::Attribute {
				.name = std::string(name),
				.accessorIndex = *accessorIndex,
			});
		}
		return SUCCESS;
	}
}
#pragma endregion

namespace fastgltf {
	/** Passes the raw JSON of extras to the extras callback. Extras can have any type. */
	[[nodiscard]] inline auto readExtras(const ParserInternalConfig& config,
			const std::size_t objectIndex, const Category category) {
		return [&config, objectIndex, category](simdjson::ondemand::value value) {
			if (config.extrasCallback == nullptr)
				return simdjson::SUCCESS;

			std::string_view extras;
			if (const auto error = value.raw_json().get(extras); error != simdjson::SUCCESS) [[unlikely]]
				return error;
			config.extrasCallback(extras, objectIndex, category, config.userPointer);
			return simdjson::SUCCESS;
		};
	}
} // namespace fastgltf

namespace simdjson {
	/** Reads the KHR_texture_transform object of a texture info. */
	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::TextureTransform& out) {
		out.rotation = 0;
		return val.get_object().template for_each<"texCoord", "rotation", "offset", "scale">(
			out.texCoordIndex, out.rotation, out.uvOffset, out.uvScale);
	}
} // namespace simdjson

namespace fastgltf {
	template <typename Info>
	[[nodiscard]] simdjson::error_code parseTextureInfo(simdjson::ondemand::value value, Info& info,
			const Extensions extensions) {
		using namespace simdjson;

		const auto readScale = [&](ondemand::value scale) -> error_code {
			if constexpr (std::is_same_v<Info, NormalTextureInfo>)
				return scale.get(info.scale);
			else
				return SUCCESS;
		};
		const auto readStrength = [&](ondemand::value strength) -> error_code {
			if constexpr (std::is_same_v<Info, OcclusionTextureInfo>)
				return strength.get(info.strength);
			else
				return SUCCESS;
		};
		const auto readExtensions = [&](ondemand::value infoExtensions) -> error_code {
			if (!hasBit(extensions, Extensions::KHR_texture_transform))
				return SUCCESS;
			return infoExtensions.get_object().for_each<"KHR_texture_transform">(info.transform);
		};

		optional<std::size_t> index;
		const auto error = value.get_object().for_each<
			"index", "texCoord", "scale", "strength", "extensions">(
			index, info.texCoordIndex, readScale, readStrength, readExtensions);
		if (error != SUCCESS) [[unlikely]]
			return error;
		if (!index) [[unlikely]]
			return NO_SUCH_FIELD;

		info.textureIndex = *index;
		return SUCCESS;
	}

	/** Reads a texture info object into the optional out. */
	template <typename T>
	[[nodiscard]] auto readTextureInfo(T& out, const Extensions extensions) {
		return [&out, extensions](simdjson::ondemand::value value) {
			return parseTextureInfo(value, out.emplace(), extensions);
		};
	}

	[[nodiscard]] inline simdjson::error_code parseTextureExtensions(
			simdjson::ondemand::value value, Texture& texture, const Extensions extensions) {
		using namespace simdjson;

		// Each of these extensions only defines the index of the image to use instead, as "source".
		const auto readSource = [extensions](const Extensions extension,
				optional<std::size_t>& out) {
			return [enabled = hasBit(extensions, extension), &out](
					ondemand::value value) -> error_code {
				if (!enabled)
					return SUCCESS;
				if (const auto error = value.get_object().for_each<"source">(out);
					error != SUCCESS) [[unlikely]]
					return error;
				return out ? SUCCESS : NO_SUCH_FIELD;
			};
		};

		return value.get_object().for_each<
			"KHR_texture_basisu",
			"MSFT_texture_dds",
			"EXT_texture_webp",
			"EXT_texture_avif">(
			readSource(Extensions::KHR_texture_basisu, texture.basisuImageIndex),
			readSource(Extensions::MSFT_texture_dds, texture.ddsImageIndex),
			readSource(Extensions::EXT_texture_webp, texture.webpImageIndex),
			readSource(Extensions::EXT_texture_avif, texture.avifImageIndex));
	}
} // namespace fastgltf

#pragma region glTF parsing
fg::Expected<fg::DataSource> fg::Parser::decodeDataUri(const URIView& uri) const noexcept {
	auto path = uri.path();
	const auto mimeEnd = path.find(';');
	const auto mime = path.substr(0, mimeEnd);

	const auto encodingEnd = path.find(',');
	const auto encoding = path.substr(mimeEnd + 1, encodingEnd - mimeEnd - 1);
	if (encoding != "base64") [[unlikely]] {
		return Error::InvalidURI;
	}

	const auto encodedData = path.substr(encodingEnd + 1);
	if (encodedData.size() < 4 || encodedData.size() % 4 != 0) [[unlikely]] {
		return Error::InvalidURI;
	}

	if (config.mapCallback != nullptr) {
		// If a map callback is specified, we use a pointer to memory specified by it.
		const auto padding = base64::getPadding(encodedData);
		const auto size = base64::getDecodedSize(encodedData.size(), padding);

		auto info = config.mapCallback(size, config.userPointer);
		if (info.mappedMemory != nullptr) {
			const auto success = base64::decode_inplace(encodedData, static_cast<std::uint8_t*>(info.mappedMemory), padding);

			if (config.unmapCallback != nullptr) {
				config.unmapCallback(&info, config.userPointer);
			}

			if (!success) [[unlikely]] {
				return Error::InvalidURI;
			}

			sources::CustomBuffer source {};
			source.id = info.customId;
			source.mimeType = internal::getMimeTypeFromString(mime);
			return { source };
		}
	}

	// Decode the base64 data into a traditional vector
	// also check if the decoded byte count matches the size. they usually only differ when the input has
	// whitespace which we just don't handle atm (simdutf does, tbf) because of static_vector not being resizeable
	const auto padding = base64::getPadding(encodedData);
	static_vector<std::byte> uriData(for_overwrite, base64::getDecodedSize(encodedData.size(), padding));
	if (!base64::decode_inplace(encodedData, reinterpret_cast<std::uint8_t*>(uriData.data()), padding)) [[unlikely]] {
		return Error::InvalidURI;
	}

	sources::Array source {
		.bytes = std::move(uriData),
		.mimeType = internal::getMimeTypeFromString(mime),
	};
	return { std::move(source) };
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

namespace fastgltf {
	template<typename T>
	static void writeIndices(static_vector<std::byte>& generatedIndices) {
		const std::span indices(reinterpret_cast<T*>(generatedIndices.data()),
			generatedIndices.size() / sizeof(T));

		std::iota(indices.begin(), indices.end(), T(0));
	}

	static std::pair<static_vector<std::byte>, ComponentType> writeIndices(
		const std::size_t primitiveCount) {

		if (primitiveCount < 255) {
			static_vector<std::byte> generatedIndices(for_overwrite, primitiveCount * sizeof(std::uint8_t));
			writeIndices<std::uint8_t>(generatedIndices);
			return std::make_pair(std::move(generatedIndices), ComponentType::UnsignedByte);
		}

		if (primitiveCount < 65535) {
			static_vector<std::byte> generatedIndices(for_overwrite, primitiveCount * sizeof(std::uint16_t));
			writeIndices<std::uint16_t>(generatedIndices);
			return std::make_pair(std::move(generatedIndices), ComponentType::UnsignedShort);
		}

		static_vector<std::byte> generatedIndices(for_overwrite, primitiveCount * sizeof(std::uint32_t));
		writeIndices<std::uint32_t>(generatedIndices);
		return std::make_pair(std::move(generatedIndices), ComponentType::UnsignedInt);
	}
}

fg::Error fg::Parser::generateMeshIndices(fastgltf::Asset& asset) const {
	for (auto& mesh : asset.meshes) {
		for (auto& primitive : mesh.primitives) {
			if (primitive.indicesAccessor.has_value())
				continue;

			auto positionAttribute = primitive.findAttribute("POSITION");
			if (positionAttribute == primitive.attributes.end()) [[unlikely]] {
				return Error::InvalidGltf;
			}
			if (positionAttribute->accessorIndex >= asset.accessors.size()) [[unlikely]] {
				return Error::InvalidGltf;
			}
			auto& positionAccessor = asset.accessors[positionAttribute->accessorIndex];
			auto positionCount = positionAccessor.count;

			if (const auto error = validateAccessorBounds(asset, positionAccessor);
				error != Error::None) [[unlikely]]
				return error;

			auto [generatedIndices, componentType] = writeIndices(positionCount);

			auto bufferIdx = asset.buffers.size();
			auto& buffer = asset.buffers.emplace_back();
			buffer.byteLength = generatedIndices.size_bytes();
			sources::Array indicesArray {
				std::move(generatedIndices),
				MimeType::GltfBuffer,
			};
			buffer.data = std::move(indicesArray);

			auto bufferViewIdx = asset.bufferViews.size();
			auto& bufferView = asset.bufferViews.emplace_back();
			bufferView.byteLength = buffer.byteLength;
			bufferView.bufferIndex = bufferIdx;
			bufferView.byteOffset = 0;

			primitive.indicesAccessor = asset.accessors.size();
			auto& accessor = asset.accessors.emplace_back();
			accessor.byteOffset = 0;
			accessor.count = positionCount;
			accessor.type = AccessorType::Scalar;
			accessor.componentType = componentType;
			accessor.normalized = false;
			accessor.bufferViewIndex = bufferViewIdx;
		}
	}
	return Error::None;
}

namespace fastgltf::detail {
	Expected<Version> parseVersionString(const std::string_view version) noexcept {
		Version ret {};
		static constexpr std::uint32_t max_version_num = 999999999;

		// The glTF 2.0 specification allowed versions to be of the form ^[0-9]+\\.[0-9]+$ for both
		// version and minVersion. glTF 2.1 restricted it to ^(0|[1-9][0-9]{0,8})\\.(0|[1-9][0-9]{0,8})$
		// This function implements the glTF 2.1 version parsing to be more strict.

		const auto* const end = version.data() + version.size(); // from_chars requires pointers, not iterators
		auto [dot, majorError] = std::from_chars(version.data(), end, ret.major);
		if (majorError != std::errc {} || dot == end || *dot != '.') [[unlikely]]
			return Error::InvalidVersion;

		const auto majorDigits = dot - version.data();
		if ((majorDigits > 1 && version[0] == '0') || ret.major > max_version_num) [[unlikely]]
			return Error::InvalidVersion;

		auto [last, minorError] = std::from_chars(std::next(dot), end, ret.minor);
		if (minorError != std::errc {} || last != end) [[unlikely]]
			return Error::InvalidVersion;

		const auto minorDigits = last - std::next(dot);
		if ((minorDigits > 1 && *std::next(dot) == '0') || ret.minor > max_version_num) [[unlikely]]
			return Error::InvalidVersion;
		return ret;
	}
}

fg::Error fg::Parser::parseAssetInfo(internal::JsonObject& rootJson, AssetInfo& info) {
	using namespace simdjson;
	auto& root = rootJson.value;

	ondemand::value assetInfo;
	if (const auto error = root["asset"].get(assetInfo); error == NO_SUCH_FIELD) [[unlikely]] {
		return Error::MissingField;
	} else if (error != SUCCESS) [[unlikely]] {
		return Error::InvalidJson;
	}

	std::optional<std::string_view> versionString, minVersionString;
	const auto error = assetInfo.get_object().for_each<
		"version", "minVersion", "copyright", "generator">(
		versionString, minVersionString, info.copyright, info.generator);
	if (error != SUCCESS) [[unlikely]]
		return toError(error);

	if (!versionString) [[unlikely]]
		return Error::MissingField;

	auto version = detail::parseVersionString(*versionString);
	if (version.has_error())
		return version.error();
	if (version->major != 2) [[unlikely]]
		return Error::UnsupportedVersion;
	info.version = *version;

	if (minVersionString) {
		auto minVersion = detail::parseVersionString(*minVersionString);
		if (minVersion.has_error())
			return minVersion.error();
		if (*minVersion > Version { 2, 0 }) [[unlikely]]
			return Error::UnsupportedVersion;
		info.minVersion = *minVersion;
	}
	return Error::None;
}

fg::Expected<fg::Asset> fg::Parser::parse(internal::JsonObject& rootJson, Category categories) {
	using namespace simdjson;
	auto& root = rootJson.value;
	fillCategories(categories);

	Asset asset {};

	// These lookups don't iterate the root object in order. Each string in the document may only be
	// unescaped once, so the loop over the root object below skips the fields read here.
	if (const auto err = parseAssetInfo(rootJson, asset.assetInfo); err != Error::None) {
		return err;
	}

	ondemand::array extensionsRequired;
	if (root["extensionsRequired"].get_array().get(extensionsRequired) == SUCCESS) [[likely]] {
		for (auto extension : extensionsRequired) {
			std::string_view string;
			if (extension.get_string().get(string) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			bool known = false;
			for (const auto& [extensionString, extensionEnum] : extensionStrings) {
				if (extensionString == string) {
					known = true;
					if (!hasBit(config.extensions, extensionEnum)) {
						// The extension is required, but not enabled by the user.
						return Error::MissingExtensions;
					}
					break;
				}
			}
			if (!known) [[unlikely]] {
				return Error::UnknownRequiredExtension;
			}

			asset.extensionsRequired.emplace_back(string);
		}
	}

	variantCount = 0;
	if (hasBit(config.extensions, Extensions::KHR_materials_variants)) {
		// Only counts the variants; their names are read in parseExtensions.
		ondemand::object extensionsObject, variantsObject;
		ondemand::array variants;
		const auto extension = extensions::KHR_materials_variants;
		if (root["extensions"].get_object().get(extensionsObject) == SUCCESS &&
			extensionsObject[extension].get_object().get(variantsObject) == SUCCESS &&
			variantsObject["variants"].get_array().get(variants) == SUCCESS &&
			variants.count_elements().get(variantCount) != SUCCESS) [[unlikely]] {
			return Error::InvalidJson;
		}
	}

	if (root.reset().error() != SUCCESS) [[unlikely]]
		return Error::InvalidJson;

	auto readCategories = Category::None;
	for (auto object : root) {
		std::string_view key;
		if (object.escaped_key().get(key) != SUCCESS) [[unlikely]]
				return Error::InvalidJson;

		auto hashedKey = crc32c(key);
		if (hashedKey == force_consteval<crc32c("scene")>) {
			std::uint64_t defaultScene;
			if (object.value().get_uint64().get(defaultScene) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			if (!isValidIndex(defaultScene)) [[unlikely]]
				return Error::ValueOutOfRange;
			asset.defaultScene = static_cast<std::size_t>(defaultScene);
			continue;
		}

		if (hashedKey == force_consteval<crc32c("extensions")>) {
			internal::JsonObject extensionsObject;
			if (object.value().get_object().get(extensionsObject.value) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			if (const auto error = parseExtensions(extensionsObject, asset);
				error != Error::None) [[unlikely]]
				return error;
			continue;
		}

		if (hashedKey == force_consteval<crc32c("asset")> ||
			hashedKey == force_consteval<crc32c("extras")>) {
			continue;
		}

		internal::JsonArray array;
		if (object.value().get_array().get(array.value) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

#define KEY_SWITCH_CASE(name, id) case force_consteval<crc32c(FASTGLTF_QUOTE(id))>:               \
				if (hasBit(readCategories, Category::name))                                       \
					return Error::InvalidGltf;                                                    \
				if (hasBit(categories, Category::name))                                           \
					error = parse##name(array, asset);                                            \
				readCategories |= Category::name;                                                 \
				break;

		auto error = Error::None;
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
				for (auto usedValue : array.value) {
					std::string_view usedString;
					if (usedValue.get_string().get(usedString) == SUCCESS) [[likely]] {
						asset.extensionsUsed.emplace_back(usedString);
					} else {
						error = Error::InvalidGltf;
					}
				}
				break;
			}
			case force_consteval<crc32c("extensionsRequired")>: {
				// These are already parsed before this section.
				break;
			}
			default:
				break;
		}

		if (error != Error::None) [[unlikely]]
			return error;

#undef KEY_SWITCH_CASE
	}

	asset.availableCategories = readCategories;

	if (hasBit(options, Options::GenerateMeshIndices)) {
		if (const auto error = generateMeshIndices(asset); error != Error::None) [[unlikely]] {
			return error;
		}
	}

	// Resize primitive mappings to match the global variant count
	if (hasBit(config.extensions, Extensions::KHR_materials_variants) &&
		!asset.materialVariants.empty()) {
		const auto variantCount = asset.materialVariants.size();
		for (auto& mesh : asset.meshes) {
			for (auto& primitive : mesh.primitives) {
				if (primitive.mappings.empty() || primitive.mappings.size() == variantCount)
					continue;
				primitive.mappings.resize(variantCount);
			}
		}
	}

	glbBuffer = std::monostate {};

	return asset;
}

namespace fastgltf {
	/**
	 * The numbers of an accessor's min or max array. They're kept until the whole accessor has
	 * been read, since componentType and type may come after them.
	 */
	struct AccessorBoundsNumbers {
		std::array<simdjson::ondemand::number, 16> numbers;
		std::size_t size = 0;
	};

	[[nodiscard]] inline Error convertBounds(const AccessorBoundsNumbers& bounds,
			const Accessor& accessor, optional<AccessorBoundsArray>& out) {
		const auto count = getNumComponents(accessor.type);
		if (count == 0 || bounds.size != count) [[unlikely]]
			return Error::InvalidGltf;

		if (isFloatingPoint(accessor.componentType)) {
			out = AccessorBoundsArray(count, AccessorBoundsArray::BoundsType::float64);
			auto* data = out->data<double>();
			for (std::size_t i = 0; i < count; ++i)
				data[i] = bounds.numbers[i].as_double();
			return Error::None;
		}

		out = AccessorBoundsArray(count, AccessorBoundsArray::BoundsType::int64);
		auto* data = out->data<std::int64_t>();
		for (std::size_t i = 0; i < count; ++i) {
			const auto& number = bounds.numbers[i];
			if (number.is_int64()) {
				data[i] = number.get_int64();
				continue;
			}

			// Reject any values outside of the 2^63 range of std::int64_t.
			// glTF doesn't care about integers larger than 32 bits anyway.
			const auto value = number.as_double();
			if (!(value >= -0x1p63 && value < 0x1p63)) [[unlikely]]
				return Error::InvalidGltf;
			data[i] = static_cast<std::int64_t>(value);
		}
		return Error::None;
	}
} // namespace fastgltf

namespace simdjson {
	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val,
			fastgltf::AccessorBoundsNumbers& out) noexcept {
		ondemand::array array;
		if (const auto error = val.get_array().get(array); error != SUCCESS) [[unlikely]]
			return error;

		for (auto element : array) {
			// No accessor type has more than 16 components.
			if (out.size == out.numbers.size()) [[unlikely]]
				return INCORRECT_TYPE;
			if (const auto error = element.get_number().get(out.numbers[out.size++]);
				error != SUCCESS) [[unlikely]]
				return error;
		}
		return SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::SparseAccessor& out) {
		using namespace fastgltf;

		fastgltf::optional<std::size_t> count, indicesBufferView, valuesBufferView;
		std::optional<ComponentType> indexComponentType;

		const auto readIndices = [&](ondemand::value indices) -> error_code {
			return indices.get_object().for_each<"bufferView", "byteOffset", "componentType">(
				indicesBufferView, out.indicesByteOffset, indexComponentType);
		};
		const auto readValues = [&](ondemand::value values) -> error_code {
			return values.get_object().for_each<"bufferView", "byteOffset">(
				valuesBufferView, out.valuesByteOffset);
		};

		const auto error = val.get_object().template for_each<"count", "indices", "values">(
			count, readIndices, readValues);
		if (error != SUCCESS) [[unlikely]]
			return error;

		if (!count || !indicesBufferView || !valuesBufferView || !indexComponentType) [[unlikely]]
			return NO_SUCH_FIELD;

		out.count = *count;
		out.indicesBufferView = *indicesBufferView;
		out.valuesBufferView = *valuesBufferView;
		out.indexComponentType = *indexComponentType;
		return SUCCESS;
	}
} // namespace simdjson

fg::Error fg::Parser::parseAccessors(internal::JsonArray& accessorsJson, Asset& asset) {
	using namespace simdjson;
	auto& accessors = accessorsJson.value;

	if (const auto error = reserveElements(accessors, asset.accessors);
		error != Error::None) [[unlikely]]
		return error;

	for (auto accessorValue : accessors) {
		const auto extras = readExtras(config, asset.accessors.size(), Category::Accessors);

		auto& accessor = asset.accessors.emplace_back();

		std::optional<ComponentType> componentType;
		std::optional<AccessorType> type;
		optional<std::size_t> count;
		std::optional<AccessorBoundsNumbers> max, min;

		const auto error = accessorValue.get_object().for_each<
			"bufferView", "byteOffset", "componentType", "normalized",
			"count", "type", "max", "min",
			"sparse", "name", "extras">(
			accessor.bufferViewIndex, accessor.byteOffset, componentType, accessor.normalized,
			count, type, max, min,
			accessor.sparse, accessor.name, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);

		if (!componentType || !count || !type) [[unlikely]]
			return Error::InvalidGltf;

		accessor.componentType = *componentType;
		accessor.count = *count;
		accessor.type = *type;

		const auto allowDouble = hasBit(options, Options::AllowDouble) ||
			hasBit(config.extensions, Extensions::KHR_accessor_float64);
		if (accessor.componentType == ComponentType::Double && !allowDouble) [[unlikely]]
			return Error::InvalidGltf;

		// This property MUST NOT be true for accessors with FLOAT or UNSIGNED_INT component type.
		if (accessor.normalized &&
			(accessor.componentType == ComponentType::UnsignedInt ||
				isFloatingPoint(accessor.componentType))) [[unlikely]]
			return Error::InvalidGltf;

		if (max) {
			if (const auto boundsError = convertBounds(*max, accessor, accessor.max);
				boundsError != Error::None) [[unlikely]]
				return boundsError;
		}
		if (min) {
			if (const auto boundsError = convertBounds(*min, accessor, accessor.min);
				boundsError != Error::None) [[unlikely]]
				return boundsError;
		}
	}

	return Error::None;
}

namespace simdjson {
	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::AnimationChannel& out) {
		using namespace fastgltf;

		fastgltf::optional<std::size_t> sampler;
		std::optional<std::string_view> path;

		// When node isn't defined, the channel SHOULD be ignored. That's left to the user.
		const auto readTarget = [&](ondemand::value target) -> error_code {
			return target.get_object().for_each<"node", "path">(out.nodeIndex, path);
		};

		const auto error = val.get_object().template for_each<"sampler", "target">(
			sampler, readTarget);
		if (error != SUCCESS) [[unlikely]]
			return error;

		if (!sampler || !path) [[unlikely]]
			return NO_SUCH_FIELD;

		out.samplerIndex = *sampler;
		// Unknown paths are ignored, as extensions like KHR_animation_pointer may define them.
		if (*path == "translation") {
			out.path = AnimationPath::Translation;
		} else if (*path == "rotation") {
			out.path = AnimationPath::Rotation;
		} else if (*path == "scale") {
			out.path = AnimationPath::Scale;
		} else if (*path == "weights") {
			out.path = AnimationPath::Weights;
		}
		return SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::AnimationSampler& out) {
		fastgltf::optional<std::size_t> input, output;

		const auto error = val.get_object().template for_each<
			"input", "interpolation", "output">(
			input, out.interpolation, output);
		if (error != SUCCESS) [[unlikely]]
			return error;

		if (!input || !output) [[unlikely]]
			return NO_SUCH_FIELD;

		out.inputAccessor = *input;
		out.outputAccessor = *output;
		return SUCCESS;
	}
} // namespace simdjson

fg::Error fg::Parser::parseAnimations(internal::JsonArray& animationsJson, Asset& asset) {
	using namespace simdjson;
	auto& animations = animationsJson.value;

	if (const auto error = reserveElements(animations, asset.animations);
		error != Error::None) [[unlikely]]
		return error;

	for (auto animationValue : animations) {
		const auto extras = readExtras(config, asset.animations.size(), Category::Animations);

		auto& animation = asset.animations.emplace_back();

		const auto error = animationValue.get_object().for_each<
			"channels", "samplers", "name", "extras">(
			animation.channels, animation.samplers, animation.name, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);

		// Both are required and need at least one element.
		if (animation.channels.empty() || animation.samplers.empty()) [[unlikely]]
			return Error::InvalidGltf;
	}

	return Error::None;
}

fg::Error fg::Parser::parseBuffers(internal::JsonArray& buffersJson, Asset& asset) {
	using namespace simdjson;
	auto& buffers = buffersJson.value;

	// The spec for EXT_meshopt_compression allows so-called 'fallback buffers' which only exist to
	// act as a valid fallback for compressed buffer views, but actually do not contain any data.
	// In these cases, there is either simply no URI, or a fallback boolean is added to the
	// extensions' extension field.
	// In these cases, fastgltf could just leave the std::monostate in the DataSource.
	// However, to make the actual use of these buffers clear, we'll use an empty fallback type.
	bool meshoptCompressionRequired = false;
	for (const auto& extension : asset.extensionsRequired) {
		if (extension == extensions::EXT_meshopt_compression ||
			extension == extensions::KHR_meshopt_compression) {
			meshoptCompressionRequired = true;
		}
	}

	if (const auto error = reserveElements(buffers, asset.buffers);
		error != Error::None) [[unlikely]]
		return error;

	for (auto bufferValue : buffers) {
		const auto extras = readExtras(config, asset.buffers.size(), Category::Buffers);

		auto& buffer = asset.buffers.emplace_back();

		optional<std::size_t> byteLength;
		std::optional<std::string_view> uriString;

		const auto error = bufferValue.get_object().for_each<
			"uri", "byteLength", "name", "extras">(
			uriString, byteLength, buffer.name, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);

		if (!byteLength) [[unlikely]]
			return Error::InvalidGltf;
		buffer.byteLength = *byteLength;

		// When parsing GLB, there's a buffer object that will point to the BUF chunk in the
		// file. Otherwise, data must be specified in the "uri" field.
		if (uriString) {
			URIView uriView(*uriString);

			if (!uriView.valid()) [[unlikely]] {
				return Error::InvalidURI;
			}

			if (uriView.isDataUri()) {
				auto decoded = decodeDataUri(uriView);
				if (decoded.has_error()) [[unlikely]] {
					return decoded.error();
				}

				buffer.data = std::move(decoded.get());
			} else if (uriView.isLocalPath() && hasBit(options, Options::LoadExternalBuffers)) {
				auto file = loadFileFromUri(uriView);
				if (file.has_error()) [[unlikely]] {
					return file.error();
				}

				buffer.data = std::move(file.get());
			} else {
				sources::URI filePath;
				filePath.fileByteOffset = 0;
				filePath.uri = uriView;
				buffer.data = std::move(filePath);
			}
		} else if (asset.buffers.size() == 1 &&
			!std::holds_alternative<std::monostate>(glbBuffer)) {
			// The first buffer of a GLB refers to its BIN chunk.
			buffer.data = std::move(glbBuffer);
		} else if (meshoptCompressionRequired) {
			// This buffer is not a GLB buffer and has no URI source and is therefore a fallback.
			buffer.data = sources::Fallback();
		} else [[unlikely]] {
			// All other buffers have to contain an uri field.
			return Error::InvalidGltf;
		}

		if (std::holds_alternative<std::monostate>(buffer.data)) [[unlikely]] {
			return Error::InvalidGltf;
		}
	}

	return Error::None;
}

namespace simdjson {
	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::CompressedBufferView& out) {
		using namespace fastgltf;

		fastgltf::optional<std::size_t> buffer, byteLength, byteStride, count;
		std::optional<MeshoptCompressionMode> mode;

		out.byteOffset = 0;
		out.filter = MeshoptCompressionFilter::None;

		const auto error = val.get_object().template for_each<
			"buffer", "byteOffset", "byteLength", "byteStride",
			"count", "mode", "filter">(
			buffer, out.byteOffset, byteLength, byteStride,
			count, mode, out.filter);
		if (error != SUCCESS) [[unlikely]]
			return error;
		if (!buffer || !byteLength || !byteStride || !count || !mode) [[unlikely]]
			return NO_SUCH_FIELD;

		out.bufferIndex = *buffer;
		out.byteLength = *byteLength;
		out.byteStride = *byteStride;
		out.count = *count;
		out.mode = *mode;
		return SUCCESS;
	}
} // namespace simdjson

fg::Error fg::Parser::parseBufferViews(internal::JsonArray& bufferViewsJson, Asset& asset) {
	using namespace simdjson;
	auto& bufferViews = bufferViewsJson.value;

	const auto readMeshoptCompression = [this](const Extensions extension,
			std::optional<CompressedBufferView>& out) {
		return [enabled = hasBit(config.extensions, extension), &out](ondemand::value value) {
			return enabled ? value.get(out.emplace()) : SUCCESS;
		};
	};

	if (const auto error = reserveElements(bufferViews, asset.bufferViews);
		error != Error::None) [[unlikely]]
		return error;

	for (auto bufferViewValue : bufferViews) {
		const auto extras = readExtras(config, asset.bufferViews.size(), Category::BufferViews);

		auto& view = asset.bufferViews.emplace_back();

		optional<std::size_t> buffer, byteLength;
		std::optional<CompressedBufferView> khrCompression, extCompression;

		const auto readExtensions = [&](ondemand::value extensions) -> error_code {
			return extensions.get_object().for_each<
				"KHR_meshopt_compression", "EXT_meshopt_compression">(
				readMeshoptCompression(Extensions::KHR_meshopt_compression, khrCompression),
				readMeshoptCompression(Extensions::EXT_meshopt_compression, extCompression));
		};

		const auto error = bufferViewValue.get_object().for_each<
			"buffer", "byteOffset", "byteLength", "byteStride",
			"target", "name", "extensions", "extras">(
			buffer, view.byteOffset, byteLength, view.byteStride,
			view.target, view.name, readExtensions, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);

		if (!buffer || !byteLength) [[unlikely]]
			return Error::InvalidGltf;

		view.bufferIndex = *buffer;
		view.byteLength = *byteLength;

		// KHR_meshopt_compression takes precedence when both extensions are used.
		if (khrCompression) {
			view.meshoptCompression = std::move(*khrCompression);
		} else if (extCompression) {
			view.meshoptCompression = std::move(*extCompression);
		}
	}

	return Error::None;
}

namespace simdjson {
	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::Camera::Perspective& out) {
		fastgltf::optional<fastgltf::num> yfov, znear;

		const auto error = val.get_object().template for_each<
			"aspectRatio", "yfov", "zfar", "znear">(
			out.aspectRatio, yfov, out.zfar, znear);
		if (error != SUCCESS) [[unlikely]]
			return error;

		if (!yfov || !znear) [[unlikely]]
			return NO_SUCH_FIELD;

		out.yfov = *yfov;
		out.znear = *znear;
		return SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::Camera::Orthographic& out) {
		fastgltf::optional<fastgltf::num> xmag, ymag, zfar, znear;

		const auto error = val.get_object().template for_each<
			"xmag", "ymag", "zfar", "znear">(
			xmag, ymag, zfar, znear);
		if (error != SUCCESS) [[unlikely]]
			return error;

		if (!xmag || !ymag || !zfar || !znear) [[unlikely]]
			return NO_SUCH_FIELD;

		out.xmag = *xmag;
		out.ymag = *ymag;
		out.zfar = *zfar;
		out.znear = *znear;
		return SUCCESS;
	}
} // namespace simdjson

fg::Error fg::Parser::parseCameras(internal::JsonArray& camerasJson, Asset& asset) {
	using namespace simdjson;
	auto& cameras = camerasJson.value;

	if (const auto error = reserveElements(cameras, asset.cameras);
		error != Error::None) [[unlikely]]
		return error;

	for (auto cameraValue : cameras) {
		const auto extras = readExtras(config, asset.cameras.size(), Category::Cameras);

		auto& camera = asset.cameras.emplace_back();

		std::optional<std::string_view> type;
		std::optional<Camera::Perspective> perspective;
		std::optional<Camera::Orthographic> orthographic;

		const auto error = cameraValue.get_object().for_each<
			"type", "perspective", "orthographic", "name", "extras">(
			type, perspective, orthographic, camera.name, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);

		// The object with the camera's properties has to match the type.
		if (type == "perspective" && perspective) {
			camera.camera = *perspective;
		} else if (type == "orthographic" && orthographic) {
			camera.camera = *orthographic;
		} else [[unlikely]] {
			return Error::InvalidGltf;
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parseExtensions(internal::JsonObject& extensionsObjectJson, Asset& asset) {
	using namespace simdjson;

	// Reads an array with one of the parseXYZ functions.
	const auto readArray = [&](Error (Parser::*parseFunction)(internal::JsonArray&, Asset&)) {
		return [this, &asset, parseFunction](ondemand::value value) {
			internal::JsonArray array;
			if (const auto error = value.get(array.value); error != SUCCESS) [[unlikely]]
				return error;
			return toErrorCode((this->*parseFunction)(array, asset));
		};
	};

	const auto readLights = [&](ondemand::value value) -> error_code {
		if (!hasBit(config.extensions, Extensions::KHR_lights_punctual))
			return SUCCESS;
		return value.get_object().for_each<"lights">(readArray(&Parser::parseLights));
	};

	const auto readVariants = [&](ondemand::value value) -> error_code {
		if (!hasBit(config.extensions, Extensions::KHR_materials_variants))
			return SUCCESS;

		const auto readVariant = [&](ondemand::value variant) -> error_code {
			std::optional<std::string_view> name;
			if (const auto error = variant.get_object().for_each<"name">(name);
				error != SUCCESS) [[unlikely]]
				return error;
			if (!name) [[unlikely]]
				return NO_SUCH_FIELD;
			asset.materialVariants.emplace_back(*name);
			return SUCCESS;
		};
		const auto readVariantArray = [&](ondemand::value variants) {
			return parseArray(variants, asset.materialVariants, readVariant);
		};

		if (const auto error = value.get_object().for_each<"variants">(readVariantArray);
			error != SUCCESS) [[unlikely]]
			return error;
		if (asset.materialVariants.empty()) [[unlikely]]
			return NO_SUCH_FIELD;
		return SUCCESS;
	};

	const auto readShapes = [&](ondemand::value value) -> error_code {
		if (!hasBit(config.extensions, Extensions::KHR_implicit_shapes))
			return SUCCESS;

		const auto error = value.get_object().for_each<"shapes">(
			readArray(&Parser::parseShapes));
		if (error != SUCCESS) [[unlikely]]
			return error;
		if (asset.shapes.empty()) [[unlikely]]
			return NO_SUCH_FIELD;
		return SUCCESS;
	};

	const auto readPhysics = [&](ondemand::value value) -> error_code {
		if (!hasBit(config.extensions, Extensions::KHR_physics_rigid_bodies))
			return SUCCESS;
		return value.get_object().for_each<
			"physicsMaterials",
			"collisionFilters",
			"physicsJoints">(
			readArray(&Parser::parsePhysicsMaterials),
			readArray(&Parser::parseCollisionFilters),
			readArray(&Parser::parsePhysicsJoints));
	};

	return toError(extensionsObjectJson.value.for_each<
		"KHR_lights_punctual", "KHR_materials_variants",
		"KHR_implicit_shapes", "KHR_physics_rigid_bodies">(
		readLights, readVariants,
		readShapes, readPhysics));
}

fg::Error fg::Parser::parseImages(internal::JsonArray& imagesJson, Asset& asset) {
	using namespace simdjson;
	auto& images = imagesJson.value;

	if (const auto error = reserveElements(images, asset.images); error != Error::None) [[unlikely]]
		return error;

	for (auto imageValue : images) {
		const auto extras = readExtras(config, asset.images.size(), Category::Images);

		auto& image = asset.images.emplace_back();

		std::optional<std::string_view> uriString, mimeType;
		optional<std::size_t> bufferViewIndex;

		const auto error = imageValue.get_object().for_each<
			"uri", "mimeType", "bufferView", "name", "extras">(
			uriString, mimeType, bufferViewIndex, image.name, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);

		if (uriString) [[likely]] {
			if (bufferViewIndex) [[unlikely]] {
				// If uri is declared, bufferView cannot be declared.
				return Error::InvalidGltf;
			}

			URIView uriView(*uriString);
			if (!uriView.valid()) [[unlikely]] {
				return Error::InvalidURI;
			}

			if (uriView.isDataUri()) {
				auto decoded = decodeDataUri(uriView);
				if (decoded.has_error()) [[unlikely]] {
					return decoded.error();
				}

				image.data = std::move(decoded.get());
			} else if (uriView.isLocalPath() && hasBit(options, Options::LoadExternalImages)) {
				auto file = loadFileFromUri(uriView);
				if (file.has_error()) [[unlikely]] {
					return file.error();
				}

				image.data = std::move(file.get());
			} else {
				sources::URI filePath;
				filePath.fileByteOffset = 0;
				filePath.uri = uriView;
				image.data = std::move(filePath);
			}

			if (mimeType) [[likely]] {
				std::visit([&]<typename T>(T& arg) {
					// This is kinda cursed
					if constexpr (is_any_of_v<T, sources::CustomBuffer, sources::BufferView,
							sources::URI, sources::Array, sources::Vector>) {
						arg.mimeType = internal::getMimeTypeFromString(*mimeType);
					}
				}, image.data);
			}
		} else if (bufferViewIndex) {
			if (!mimeType) [[unlikely]] {
				// If bufferView is defined, mimeType needs to also be defined.
				return Error::InvalidGltf;
			}

			image.data = sources::BufferView {
				.bufferViewIndex = *bufferViewIndex,
				.mimeType = internal::getMimeTypeFromString(*mimeType),
			};
		}

		if (std::holds_alternative<std::monostate>(image.data)) [[unlikely]] {
			return Error::InvalidGltf;
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parseLights(internal::JsonArray& lightsJson, Asset& asset) {
	using namespace simdjson;
	auto& lights = lightsJson.value;

	if (const auto error = reserveElements(lights, asset.lights); error != Error::None) [[unlikely]]
		return error;

	for (auto lightValue : lights) {
		auto& light = asset.lights.emplace_back();
		light.color = math::nvec3(1);
		light.intensity = 1.0f;

		std::optional<LightType> type;
		bool hasSpot = false;

		const auto readSpot = [&](ondemand::value spot) -> error_code {
			hasSpot = true;
			return spot.get_object().for_each<"innerConeAngle", "outerConeAngle">(
				light.innerConeAngle, light.outerConeAngle);
		};

		const auto error = lightValue.get_object().for_each<
			"type", "color", "intensity", "range", "spot", "name">(
			type, light.color, light.intensity, light.range, readSpot, light.name);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);

		if (!type) [[unlikely]]
			return Error::InvalidGltf;
		light.type = *type;

		// The cone angles only apply to spot lights, which are required to have the spot object.
		if (light.type == LightType::Spot) {
			if (!hasSpot) [[unlikely]]
				return Error::InvalidGltf;
			if (!light.innerConeAngle)
				light.innerConeAngle = 0.0f;
			if (!light.outerConeAngle)
				light.outerConeAngle = static_cast<num>(std::numbers::pi / 4.0);
		} else {
			light.innerConeAngle.reset();
			light.outerConeAngle.reset();
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parseMaterialExtensions(internal::JsonObject& objectJson,
		Material& material) {
	using namespace simdjson;
	const auto extensions = config.extensions;
	const auto readTexture = [extensions](auto& out) {
		return readTextureInfo(out, extensions);
	};

	const auto readAnisotropy = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::KHR_materials_anisotropy))
			return SUCCESS;

		auto& anisotropy = material.anisotropy.emplace();
		return value.get_object().for_each<
			"anisotropyStrength", "anisotropyRotation",
			"anisotropyTexture">(
			anisotropy.anisotropyStrength, anisotropy.anisotropyRotation,
			readTexture(anisotropy.anisotropyTexture));
	};

	const auto readClearcoat = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::KHR_materials_clearcoat))
			return SUCCESS;

		auto& clearcoat = material.clearcoat.emplace();
		return value.get_object().for_each<
			"clearcoatFactor", "clearcoatTexture",
			"clearcoatRoughnessFactor", "clearcoatRoughnessTexture",
			"clearcoatNormalTexture">(
			clearcoat.clearcoatFactor, readTexture(clearcoat.clearcoatTexture),
			clearcoat.clearcoatRoughnessFactor, readTexture(clearcoat.clearcoatRoughnessTexture),
			readTexture(clearcoat.clearcoatNormalTexture));
	};

	const auto readDispersion = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::KHR_materials_dispersion))
			return SUCCESS;

		return value.get_object().for_each<"dispersion">(material.dispersion);
	};

	const auto readEmissiveStrength = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::KHR_materials_emissive_strength))
			return SUCCESS;

		return value.get_object().for_each<"emissiveStrength">(material.emissiveStrength);
	};

	const auto readIor = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::KHR_materials_ior))
			return SUCCESS;

		return value.get_object().for_each<"ior">(material.ior);
	};

	const auto readIridescence = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::KHR_materials_iridescence))
			return SUCCESS;

		auto& iridescence = material.iridescence.emplace();
		return value.get_object().for_each<
			"iridescenceFactor", "iridescenceTexture",
			"iridescenceIor",
			"iridescenceThicknessMinimum", "iridescenceThicknessMaximum",
			"iridescenceThicknessTexture">(
			iridescence.iridescenceFactor, readTexture(iridescence.iridescenceTexture),
			iridescence.iridescenceIor,
			iridescence.iridescenceThicknessMinimum, iridescence.iridescenceThicknessMaximum,
			readTexture(iridescence.iridescenceThicknessTexture));
	};

	const auto readDiffuseTransmission = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::KHR_materials_diffuse_transmission))
			return SUCCESS;

		auto& transmission = material.diffuseTransmission.emplace();
		return value.get_object().for_each<
			"diffuseTransmissionFactor",
			"diffuseTransmissionTexture",
			"diffuseTransmissionColorFactor",
			"diffuseTransmissionColorTexture">(
			transmission.diffuseTransmissionFactor,
			readTexture(transmission.diffuseTransmissionTexture),
			transmission.diffuseTransmissionColorFactor,
			readTexture(transmission.diffuseTransmissionColorTexture));
	};

	const auto readSheen = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::KHR_materials_sheen))
			return SUCCESS;

		auto& sheen = material.sheen.emplace();
		return value.get_object().for_each<
			"sheenColorFactor", "sheenColorTexture",
			"sheenRoughnessFactor", "sheenRoughnessTexture">(
			sheen.sheenColorFactor, readTexture(sheen.sheenColorTexture),
			sheen.sheenRoughnessFactor, readTexture(sheen.sheenRoughnessTexture));
	};

	const auto readSpecular = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::KHR_materials_specular))
			return SUCCESS;

		auto& specular = material.specular.emplace();
		return value.get_object().for_each<
			"specularFactor", "specularTexture",
			"specularColorFactor", "specularColorTexture">(
			specular.specularFactor, readTexture(specular.specularTexture),
			specular.specularColorFactor, readTexture(specular.specularColorTexture));
	};

	const auto readTransmission = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::KHR_materials_transmission))
			return SUCCESS;

		auto& transmission = material.transmission.emplace();
		return value.get_object().for_each<"transmissionFactor", "transmissionTexture">(
			transmission.transmissionFactor, readTexture(transmission.transmissionTexture));
	};

	const auto readUnlit = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::KHR_materials_unlit))
			return SUCCESS;

		material.unlit = true;
		ondemand::object unlit;
		return value.get(unlit);
	};

	const auto readVolume = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::KHR_materials_volume))
			return SUCCESS;

		auto& volume = material.volume.emplace();
		return value.get_object().for_each<
			"thicknessFactor", "thicknessTexture",
			"attenuationDistance", "attenuationColor">(
			volume.thicknessFactor, readTexture(volume.thicknessTexture),
			volume.attenuationDistance, volume.attenuationColor);
	};

	const auto readPackedNormal = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::MSFT_packing_normalRoughnessMetallic))
			return SUCCESS;

		return value.get_object().for_each<"normalRoughnessMetallicTexture">(
			readTexture(material.packedNormalMetallicRoughnessTexture));
	};

	const auto readPackedOcclusion = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::MSFT_packing_occlusionRoughnessMetallic))
			return SUCCESS;

		auto& packed = material.packedOcclusionRoughnessMetallicTextures.emplace();
		return value.get_object().for_each<
			"occlusionRoughnessMetallicTexture",
			"roughnessMetallicOcclusionTexture",
			"normalTexture">(
			readTexture(packed.occlusionRoughnessMetallicTexture),
			readTexture(packed.roughnessMetallicOcclusionTexture),
			readTexture(packed.normalTexture));
	};

	const auto readSpecularGlossiness = [&](ondemand::value value) -> error_code {
		if (!hasBit(extensions, Extensions::KHR_materials_pbrSpecularGlossiness))
			return SUCCESS;

		auto& specularGlossiness = material.specularGlossiness.emplace();
		return value.get_object().for_each<
			"diffuseFactor", "diffuseTexture",
			"specularFactor", "glossinessFactor",
			"specularGlossinessTexture">(
			specularGlossiness.diffuseFactor, readTexture(specularGlossiness.diffuseTexture),
			specularGlossiness.specularFactor, specularGlossiness.glossinessFactor,
			readTexture(specularGlossiness.specularGlossinessTexture));
	};

	return toError(objectJson.value.for_each<
		"KHR_materials_anisotropy", "KHR_materials_clearcoat", "KHR_materials_dispersion",
		"KHR_materials_emissive_strength", "KHR_materials_ior", "KHR_materials_iridescence",
		"KHR_materials_diffuse_transmission", "KHR_materials_sheen", "KHR_materials_specular",
		"KHR_materials_transmission", "KHR_materials_unlit", "KHR_materials_volume",
		"MSFT_packing_normalRoughnessMetallic", "MSFT_packing_occlusionRoughnessMetallic",
		"KHR_materials_pbrSpecularGlossiness">(
		readAnisotropy, readClearcoat, readDispersion,
		readEmissiveStrength, readIor, readIridescence,
		readDiffuseTransmission, readSheen, readSpecular,
		readTransmission, readUnlit, readVolume,
		readPackedNormal, readPackedOcclusion,
		readSpecularGlossiness));
}

fg::Error fg::Parser::parseMaterials(internal::JsonArray& materialsJson, Asset& asset) {
	using namespace simdjson;
	auto& materials = materialsJson.value;

	const auto readTexture = [this](auto& out) {
		return readTextureInfo(out, config.extensions);
	};

	if (const auto error = reserveElements(materials, asset.materials);
		error != Error::None) [[unlikely]]
		return error;

	for (auto materialValue : materials) {
		const auto extras = readExtras(config, asset.materials.size(), Category::Materials);

		auto& material = asset.materials.emplace_back();
		auto& pbr = material.pbrData;

		const auto readPbr = [&](ondemand::value value) -> error_code {
			return value.get_object().for_each<
				"baseColorFactor", "baseColorTexture", "metallicFactor",
				"roughnessFactor", "metallicRoughnessTexture">(
				pbr.baseColorFactor, readTexture(pbr.baseColorTexture), pbr.metallicFactor,
				pbr.roughnessFactor, readTexture(pbr.metallicRoughnessTexture));
		};

		const auto readExtensions = [&](ondemand::value value) -> error_code {
			internal::JsonObject extensions;
			if (const auto error = value.get(extensions.value); error != SUCCESS) [[unlikely]]
				return error;
			return toErrorCode(parseMaterialExtensions(extensions, material));
		};

		const auto error = materialValue.get_object().for_each<
			"name", "pbrMetallicRoughness",
			"normalTexture", "occlusionTexture",
			"emissiveTexture", "emissiveFactor",
			"alphaMode", "alphaCutoff", "doubleSided",
			"extensions", "extras">(
			material.name, readPbr,
			readTexture(material.normalTexture), readTexture(material.occlusionTexture),
			readTexture(material.emissiveTexture), material.emissiveFactor,
			material.alphaMode, material.alphaCutoff, material.doubleSided,
			readExtensions, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);
	}

	return Error::None;
}

fg::Error fg::Parser::parsePrimitiveExtensions(internal::JsonObject& objectJson,
		Primitive& primitive) {
	using namespace simdjson;

	const auto readVariants = [&](ondemand::value value) -> error_code {
		if (!hasBit(config.extensions, Extensions::KHR_materials_variants))
			return SUCCESS;

		const auto readMappings = [&](ondemand::value mappingsValue) -> error_code {
			ondemand::array mappings;
			if (const auto error = mappingsValue.get(mappings); error != SUCCESS) [[unlikely]]
				return error;

			// The material may come after the variants, so the variants are collected first.
			optional<std::size_t> material;
			std::vector<std::size_t> variants;
			for (auto mapping : mappings) {
				material.reset();
				variants.clear();
				const auto error = mapping.get_object().for_each<"material", "variants">(
					material, variants);
				if (error != SUCCESS) [[unlikely]]
					return error;
				if (!material || variants.empty()) [[unlikely]]
					return NO_SUCH_FIELD;

				for (const auto variant : variants) {
					if (variant >= variantCount) [[unlikely]]
						return INCORRECT_TYPE;
					primitive.mappings.resize(max(primitive.mappings.size(), variant + 1));
					primitive.mappings[variant] = *material;
				}
			}
			return SUCCESS;
		};

		if (const auto error = value.get_object().for_each<"mappings">(readMappings);
			error != SUCCESS) [[unlikely]]
			return error;
		if (primitive.mappings.empty()) [[unlikely]]
			return NO_SUCH_FIELD;
		return SUCCESS;
	};

	const auto readDraco = [&](ondemand::value value) -> error_code {
		if (!hasBit(config.extensions, Extensions::KHR_draco_mesh_compression))
			return SUCCESS;

		auto& draco = primitive.dracoCompression.emplace();
		optional<std::size_t> bufferView;
		const auto error = value.get_object().for_each<"bufferView", "attributes">(
			bufferView, draco.attributes);
		if (error != SUCCESS) [[unlikely]]
			return error;
		if (!bufferView || draco.attributes.empty()) [[unlikely]]
			return NO_SUCH_FIELD;

		draco.bufferView = *bufferView;
		return SUCCESS;
	};

	return toError(objectJson.value.for_each<
		"KHR_materials_variants", "KHR_draco_mesh_compression">(
		readVariants, readDraco));
}

fg::Error fg::Parser::parsePrimitives(internal::JsonArray& meshesJson, Mesh& mesh) {
	using namespace simdjson;
	auto& primitives = meshesJson.value;

	if (const auto error = reserveElements(primitives, mesh.primitives); error != Error::None) [[unlikely]]
		return error;

	for (auto value : primitives) {
		auto& primitive = mesh.primitives.emplace_back();

		const auto readExtensions = [&](ondemand::value extensionsValue) -> error_code {
			internal::JsonObject extensions;
			if (const auto error = extensionsValue.get(extensions.value); error != SUCCESS) [[unlikely]]
				return error;
			return toErrorCode(parsePrimitiveExtensions(extensions, primitive));
		};

		// The attributes are required, but may be empty.
		bool hasAttributes = false;
		const auto readAttributes = [&](ondemand::value attributes) {
			hasAttributes = true;
			return attributes.get(primitive.attributes);
		};

		const auto error = value.get_object().for_each<
			"attributes", "indices", "material",
			"mode", "targets", "extensions">(
			readAttributes, primitive.indicesAccessor, primitive.materialIndex,
			primitive.type, primitive.targets, readExtensions);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);

		if (!hasAttributes) [[unlikely]]
			return Error::InvalidGltf;
	}

	return Error::None;
}

fg::Error fg::Parser::parseMeshes(internal::JsonArray& meshesJson, Asset& asset) {
	using namespace simdjson;
	auto& meshes = meshesJson.value;

	if (const auto error = reserveElements(meshes, asset.meshes); error != Error::None) [[unlikely]]
		return error;

	for (auto meshValue : meshes) {
		const auto extras = readExtras(config, asset.meshes.size(), Category::Meshes);

		auto& mesh = asset.meshes.emplace_back();

		const auto readPrimitives = [&](ondemand::value primitivesValue) {
			internal::JsonArray primitives;
			if (const auto error = primitivesValue.get(primitives.value); error != SUCCESS) [[unlikely]]
				return error;
			return toErrorCode(parsePrimitives(primitives, mesh));
		};

		const auto error = meshValue.get_object().for_each<
			"primitives", "weights", "name", "extras">(
			readPrimitives, mesh.weights, mesh.name, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);

		// The primitives are required and need at least one element.
		if (mesh.primitives.empty()) [[unlikely]]
			return Error::InvalidGltf;
	}

	return Error::None;
}

fg::Error fg::Parser::parseNodes(internal::JsonArray& nodesJson, Asset& asset) {
	using namespace simdjson;
	auto& nodes = nodesJson.value;

	const auto parseNodeExtensions = [&](ondemand::value value, Node& node) -> error_code {
		const auto readLight = [&](ondemand::value lights) -> error_code {
			if (!hasBit(config.extensions, Extensions::KHR_lights_punctual))
				return SUCCESS;
			if (const auto error = lights.get_object().for_each<"light">(node.lightIndex);
				error != SUCCESS) [[unlikely]]
				return error;
			if (!node.lightIndex) [[unlikely]]
				return NO_SUCH_FIELD;
			return SUCCESS;
		};
		const auto readInstancing = [&](ondemand::value instancing) -> error_code {
			if (!hasBit(config.extensions, Extensions::EXT_mesh_gpu_instancing))
				return SUCCESS;
			const auto error = instancing.get_object().for_each<"attributes">(
				node.instancingAttributes);
			if (error != SUCCESS) [[unlikely]]
				return error;
			if (node.instancingAttributes.empty()) [[unlikely]]
				return NO_SUCH_FIELD;
			return SUCCESS;
		};
		const auto readRigidBodies = [&](ondemand::value rigidBodies) -> error_code {
			if (!hasBit(config.extensions, Extensions::KHR_physics_rigid_bodies))
				return SUCCESS;
			internal::JsonObject object;
			if (const auto error = rigidBodies.get(object.value); error != SUCCESS) [[unlikely]]
				return error;
			return toErrorCode(parsePhysicsRigidBody(object, node));
		};
		const auto readVisibility = [&](ondemand::value visibility) -> error_code {
			if (!hasBit(config.extensions, Extensions::KHR_node_visibility))
				return SUCCESS;
			return visibility.get_object().for_each<"visible">(node.visible);
		};
		const auto readSelectability = [&](ondemand::value selectability) -> error_code {
			if (!hasBit(config.extensions, Extensions::KHR_node_selectability))
				return SUCCESS;
			return selectability.get_object().for_each<"selectable">(node.selectable);
		};
		const auto readHoverability = [&](ondemand::value hoverability) -> error_code {
			if (!hasBit(config.extensions, Extensions::KHR_node_hoverability))
				return SUCCESS;
			return hoverability.get_object().for_each<"hoverable">(node.hoverable);
		};

		return value.get_object().for_each<
			"KHR_lights_punctual", "EXT_mesh_gpu_instancing", "KHR_physics_rigid_bodies",
			"KHR_node_visibility", "KHR_node_selectability", "KHR_node_hoverability">(
			readLight, readInstancing, readRigidBodies,
			readVisibility, readSelectability, readHoverability);
	};

	if (const auto error = reserveElements(nodes, asset.nodes); error != Error::None) [[unlikely]]
		return error;

	for (auto nodeValue : nodes) {
		const auto extras = readExtras(config, asset.nodes.size(), Category::Nodes);

		auto& node = asset.nodes.emplace_back();

		std::optional<math::nmat4x4> matrix;
		TRS trs {};

		const auto readExtensions = [&](ondemand::value extensions) {
			return parseNodeExtensions(extensions, node);
		};

		const auto error = nodeValue.get_object().for_each<
			"camera", "children", "skin", "matrix",
			"mesh", "rotation", "scale", "translation",
			"weights", "name", "extensions", "extras">(
			node.cameraIndex, node.children, node.skinIndex, matrix,
			node.meshIndex, trs.rotation, trs.scale, trs.translation,
			node.weights, node.name, readExtensions, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);

		// The TRS properties are ignored when there's a matrix.
		if (!matrix) {
			node.transform = trs;
		} else if (hasBit(options, Options::DecomposeNodeMatrices)) {
			TRS decomposed {};
			math::decomposeTransformMatrix(*matrix, decomposed.scale, decomposed.rotation,
				decomposed.translation);
			node.transform = decomposed;
		} else {
			node.transform = *matrix;
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parseSamplers(internal::JsonArray& samplersJson, Asset& asset) {
	using namespace simdjson;
	auto& samplers = samplersJson.value;

	if (const auto error = reserveElements(samplers, asset.samplers);
		error != Error::None) [[unlikely]]
		return error;

	for (auto samplerValue : samplers) {
		const auto extras = readExtras(config, asset.samplers.size(), Category::Samplers);

		auto& sampler = asset.samplers.emplace_back();

		const auto error = samplerValue.get_object().for_each<
			"magFilter", "minFilter", "wrapS", "wrapT",
			"name", "extras">(
			sampler.magFilter, sampler.minFilter, sampler.wrapS, sampler.wrapT,
			sampler.name, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);
	}

	return Error::None;
}

fg::Error fg::Parser::parseScenes(internal::JsonArray& scenesJson, Asset& asset) {
	using namespace simdjson;
	auto& scenes = scenesJson.value;

	if (const auto error = reserveElements(scenes, asset.scenes); error != Error::None) [[unlikely]]
		return error;

	for (auto sceneValue : scenes) {
		const auto extras = readExtras(config, asset.scenes.size(), Category::Scenes);

		// The scene object can be completely empty
		auto& scene = asset.scenes.emplace_back();

		const auto error = sceneValue.get_object().for_each<"nodes", "name", "extras">(
			scene.nodeIndices, scene.name, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);
	}

	return Error::None;
}

fg::Error fg::Parser::parseSkins(internal::JsonArray& skinsJson, Asset& asset) {
	using namespace simdjson;
	auto& skins = skinsJson.value;

	if (const auto error = reserveElements(skins, asset.skins); error != Error::None) [[unlikely]]
		return error;

	for (auto skinValue : skins) {
		const auto extras = readExtras(config, asset.skins.size(), Category::Skins);

		auto& skin = asset.skins.emplace_back();

		const auto error = skinValue.get_object().for_each<
			"inverseBindMatrices", "skeleton", "joints", "name", "extras">(
			skin.inverseBindMatrices, skin.skeleton, skin.joints, skin.name, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);

		// The joints are required and need at least one element.
		if (skin.joints.empty()) [[unlikely]]
			return Error::InvalidGltf;
	}

	return Error::None;
}

fg::Error fg::Parser::parseTextures(internal::JsonArray& texturesJson, Asset& asset) {
	using namespace simdjson;
	auto& textures = texturesJson.value;

	if (const auto error = reserveElements(textures, asset.textures);
		error != Error::None) [[unlikely]]
		return error;

	for (auto textureValue : textures) {
		const auto extras = readExtras(config, asset.textures.size(), Category::Textures);

		auto& texture = asset.textures.emplace_back();

		const auto readExtensions = [&](ondemand::value extensions) {
			return parseTextureExtensions(extensions, texture, config.extensions);
		};

		// The index of the sampler used by this texture. When undefined, a sampler with
		// repeat wrapping and auto filtering SHOULD be used.
		const auto error = textureValue.get_object().for_each<
			"sampler", "source", "name", "extensions", "extras">(
			texture.samplerIndex, texture.imageIndex, texture.name, readExtensions, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);
	}

	return Error::None;
}

namespace simdjson {
	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::SphereShape& out) {
		return val.get_object().template for_each<"radius">(out.radius);
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::BoxShape& out) {
		return val.get_object().template for_each<"size">(out.size);
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::CapsuleShape& out) {
		return val.get_object().template for_each<"height", "radiusBottom", "radiusTop">(
			out.height, out.radiusBottom, out.radiusTop);
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::CylinderShape& out) {
		return val.get_object().template for_each<"height", "radiusBottom", "radiusTop">(
			out.height, out.radiusBottom, out.radiusTop);
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::PlaneShape& out) {
		return val.get_object().template for_each<"sizeX", "sizeZ", "doubleSided">(
			out.sizeX, out.sizeZ, out.doubleSided);
	}
} // namespace simdjson

fg::Error fg::Parser::parseShapes(internal::JsonArray& shapesJson, Asset& asset) {
	using namespace simdjson;
	auto& shapes = shapesJson.value;

	if (const auto error = reserveElements(shapes, asset.shapes); error != Error::None) [[unlikely]]
		return error;

	for (auto shapeValue : shapes) {
		const auto extras = readExtras(config, asset.shapes.size(), Category::Shapes);

		auto& shape = asset.shapes.emplace_back();

		std::optional<std::string_view> type;
		std::optional<SphereShape> sphere;
		std::optional<BoxShape> box;
		std::optional<CapsuleShape> capsule;
		std::optional<CylinderShape> cylinder;
		std::optional<PlaneShape> plane;

		const auto error = shapeValue.get_object().for_each<
			"type", "sphere", "box", "capsule",
			"cylinder", "plane", "name", "extras">(
			type, sphere, box, capsule,
			cylinder, plane, shape.name, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);

		if (!type) [[unlikely]]
			return Error::InvalidGltf;

		// The object with the shape's properties has to match the type.
		const auto setShape = [&](const auto& properties, const std::string_view name) {
			if (properties)
				shape.data = *properties;
			return !properties || *type == name;
		};
		if (!setShape(sphere, "sphere") || !setShape(box, "box") || !setShape(capsule, "capsule") ||
			!setShape(cylinder, "cylinder") || !setShape(plane, "plane")) [[unlikely]]
			return Error::InvalidGltf;
	}

	return Error::None;
}

fg::Error fg::Parser::parsePhysicsMaterials(internal::JsonArray& physicsMaterialsJson,
		Asset& asset) {
	using namespace simdjson;
	auto& materials = physicsMaterialsJson.value;

	if (const auto error = reserveElements(materials, asset.physicsMaterials);
		error != Error::None) [[unlikely]]
		return error;

	for (auto materialValue : materials) {
		const auto extras = readExtras(config, asset.physicsMaterials.size(),
			Category::PhysicsMaterials);

		auto& material = asset.physicsMaterials.emplace_back();

		const auto error = materialValue.get_object().for_each<
			"staticFriction", "dynamicFriction", "restitution",
			"frictionCombine", "restitutionCombine", "extras">(
			material.staticFriction, material.dynamicFriction, material.restitution,
			material.frictionCombine, material.restitutionCombine, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);
	}

	return Error::None;
}

fg::Error fg::Parser::parseCollisionFilters(internal::JsonArray& collisionFiltersJson,
		Asset& asset) {
	using namespace simdjson;
	auto& filters = collisionFiltersJson.value;

	if (const auto error = reserveElements(filters, asset.collisionFilters);
		error != Error::None) [[unlikely]]
		return error;

	for (auto filterValue : filters) {
		const auto extras = readExtras(config, asset.collisionFilters.size(),
			Category::CollisionFilters);

		auto& filter = asset.collisionFilters.emplace_back();

		std::optional<std::vector<std::string>> collideWithSystems, notCollideWithSystems;

		const auto error = filterValue.get_object().for_each<
			"collisionSystems", "collideWithSystems", "notCollideWithSystems", "extras">(
			filter.collisionSystems, collideWithSystems, notCollideWithSystems, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);

		// A collision filter can't have both.
		if (collideWithSystems && notCollideWithSystems) [[unlikely]]
			return Error::InvalidGltf;
		if (collideWithSystems)
			filter.collideWithSystems = std::move(*collideWithSystems);
		if (notCollideWithSystems)
			filter.notCollideWithSystems = std::move(*notCollideWithSystems);
	}

	return Error::None;
}

namespace fastgltf {
	[[nodiscard]] inline simdjson::error_code parseAxes(simdjson::ondemand::value value,
			inplace_vector<std::uint8_t, 3>& axes) noexcept {
		simdjson::ondemand::array array;
		if (const auto error = value.get_array().get(array); error != simdjson::SUCCESS) [[unlikely]]
			return error;

		for (auto element : array) {
			std::uint8_t axis;
			if (const auto error = element.get_uint8().get(axis);
				error != simdjson::SUCCESS) [[unlikely]]
				return error;
			if (axes.size() == axes.max_size() || axis > 2) [[unlikely]]
				return simdjson::INCORRECT_TYPE;
			axes.emplace_back(axis);
		}
		return axes.empty() ? simdjson::INCORRECT_TYPE : simdjson::SUCCESS;
	}
} // namespace fastgltf

namespace simdjson {
	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::JointLimit& out) {
		const auto readLinearAxes = [&](ondemand::value axes) {
			return fastgltf::parseAxes(axes, out.linearAxes);
		};
		const auto readAngularAxes = [&](ondemand::value axes) {
			return fastgltf::parseAxes(axes, out.angularAxes);
		};

		const auto error = val.get_object().template for_each<
			"min", "max", "stiffness", "damping",
			"linearAxes", "angularAxes">(
			out.min, out.max, out.stiffness, out.damping,
			readLinearAxes, readAngularAxes);
		if (error != SUCCESS) [[unlikely]]
			return error;

		// A limit has to have exactly one of these. parseAxes doesn't allow empty arrays.
		if (out.linearAxes.empty() == out.angularAxes.empty()) [[unlikely]]
			return INCORRECT_TYPE;
		return SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::JointDrive& out) {
		using namespace fastgltf;

		std::optional<DriveType> type;
		std::optional<DriveMode> mode;
		std::optional<std::uint8_t> axis;
		fastgltf::optional<fastgltf::num> positionTarget, stiffness, velocityTarget, damping;

		const auto error = val.get_object().template for_each<
			"type", "mode", "axis", "maxForce",
			"positionTarget", "stiffness", "velocityTarget", "damping">(
			type, mode, axis, out.maxForce,
			positionTarget, stiffness, velocityTarget, damping);
		if (error != SUCCESS) [[unlikely]]
			return error;

		if (!type || !mode || !axis) [[unlikely]]
			return NO_SUCH_FIELD;

		// positionTarget requires stiffness, and velocityTarget requires damping.
		if (*axis > 2 || positionTarget.has_value() != stiffness.has_value() ||
			velocityTarget.has_value() != damping.has_value()) [[unlikely]]
			return INCORRECT_TYPE;

		out.type = *type;
		out.mode = *mode;
		out.axis = *axis;
		if (positionTarget) {
			out.positionTarget = *positionTarget;
			out.stiffness = *stiffness;
		}
		if (velocityTarget) {
			out.velocityTarget = *velocityTarget;
			out.damping = *damping;
		}
		return SUCCESS;
	}
} // namespace simdjson

fg::Error fg::Parser::parsePhysicsJoints(internal::JsonArray& physicsJointsJson, Asset& asset) {
	using namespace simdjson;
	auto& joints = physicsJointsJson.value;

	if (const auto error = reserveElements(joints, asset.physicsJoints);
		error != Error::None) [[unlikely]]
		return error;

	for (auto jointValue : joints) {
		const auto extras = readExtras(config, asset.physicsJoints.size(),
			Category::PhysicsJoints);

		auto& joint = asset.physicsJoints.emplace_back();

		const auto error = jointValue.get_object().for_each<"limits", "drives", "extras">(
			joint.limits, joint.drives, extras);
		if (error != SUCCESS) [[unlikely]]
			return toError(error);
	}

	return Error::None;
}

namespace simdjson {
	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::Geometry& out) {
		return val.get_object().template for_each<"shape", "mesh", "convexHull">(
			out.shape, out.mesh, out.convexHull);
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::Motion& out) {
		return val.get_object().template for_each<
			"isKinematic", "mass", "centerOfMass", "inertiaDiagonal",
			"inertiaOrientation", "linearVelocity", "angularVelocity", "gravityFactor">(
			out.isKinematic, out.mass, out.centerOfMass, out.inertialDiagonal,
			out.inertialOrientation, out.linearVelocity, out.angularVelocity, out.gravityFactor);
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::Collider& out) {
		std::optional<fastgltf::Geometry> geometry;

		const auto error = val.get_object().template for_each<
			"geometry", "physicsMaterial", "collisionFilter">(
			geometry, out.physicsMaterial, out.collisionFilter);
		if (error != SUCCESS) [[unlikely]]
			return error;

		if (!geometry) [[unlikely]]
			return NO_SUCH_FIELD;

		out.geometry = *geometry;
		return SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::Joint& out) {
		fastgltf::optional<std::size_t> connectedNode, joint;

		const auto error = val.get_object().template for_each<
			"connectedNode", "joint", "enableCollision">(
			connectedNode, joint, out.enableCollision);
		if (error != SUCCESS) [[unlikely]]
			return error;

		if (!connectedNode || !joint) [[unlikely]]
			return NO_SUCH_FIELD;

		out.connectedNode = *connectedNode;
		out.joint = *joint;
		return SUCCESS;
	}

	template <typename T>
	static error_code tag_invoke(deserialize_tag, T& val, fastgltf::Trigger& out) {
		std::optional<fastgltf::Geometry> geometry;
		std::optional<std::vector<std::size_t>> nodes;
		fastgltf::optional<std::size_t> collisionFilter;

		const auto error = val.get_object().template for_each<
			"geometry", "nodes", "collisionFilter">(
			geometry, nodes, collisionFilter);
		if (error != SUCCESS) [[unlikely]]
			return error;

		if (nodes) {
			out.emplace<fastgltf::NodeTrigger>().nodes = std::move(*nodes);
		} else if (geometry) {
			// Only triggers with geometry have a collision filter, otherwise the nodes' are used.
			auto& trigger = out.emplace<fastgltf::GeometryTrigger>();
			trigger.geometry = *geometry;
			trigger.collisionFilter = collisionFilter;
		} else [[unlikely]] {
			return NO_SUCH_FIELD;
		}
		return SUCCESS;
	}
} // namespace simdjson

fg::Error fg::Parser::parsePhysicsRigidBody(internal::JsonObject& rigidBodyJson, Node& node) {
	using namespace simdjson;
	auto& rigidBody = node.physicsRigidBody.emplace();

	return toError(rigidBodyJson.value.for_each<"motion", "collider", "trigger", "joint">(
		rigidBody.motion, rigidBody.collider, rigidBody.trigger, rigidBody.joint));
}

#pragma endregion

#pragma region Parser
fg::GltfType fg::determineGltfFileType(GltfDataGetter& data) {
	// We'll try and read a BinaryGltfHeader from the buffer to see if the magic is correct.
	auto header = internal::readBinaryHeader(data);
	data.reset();
	if (header.magic == internal::binaryGltfHeaderMagic) {
		return GltfType::GLB;
	}

	// First, check if any of the first four characters is a '{'.
	std::array<std::uint8_t, 4> begin {};
	data.read(begin.data(), begin.size());
	data.reset();
	for (const auto& i : begin) {
		if (static_cast<char>(i) == ' ')
			continue;
		if (static_cast<char>(i) == '{')
			return GltfType::glTF;
	}

	return GltfType::Invalid;
}

fg::Parser::Parser(Extensions extensionsToLoad) noexcept {
	jsonParser = std::make_unique<internal::JsonParser>();
	config.extensions = extensionsToLoad;
}

fg::Parser::Parser(Parser&& other) noexcept : jsonParser(std::move(other.jsonParser)), config(other.config) {}

fg::Parser& fg::Parser::operator=(Parser&& other) noexcept {
	jsonParser = std::move(other.jsonParser);
	config = other.config;
	return *this;
}

fg::Parser::~Parser() = default;

fg::Expected<fg::Asset> fg::Parser::loadGltf(GltfDataGetter& data, fs::path _directory, Options _options, Category categories) {
	auto type = fastgltf::determineGltfFileType(data);

	if (type == fastgltf::GltfType::glTF) {
		return loadGltfJson(data, std::move(_directory), _options, categories);
	}

	if (type == fastgltf::GltfType::GLB) {
		return loadGltfBinary(data, std::move(_directory), _options, categories);
	}

	return Error::InvalidFileData;
}

fg::Expected<fg::Asset> fg::Parser::loadGltfJson(GltfDataGetter& data, fs::path _directory, Options _options, Category categories) {
	using namespace simdjson;

	options = _options;
	directory = std::move(_directory);
	variantCount = 0;
	glbBuffer = std::monostate();

#if !defined(__ANDROID__)
	// If we never have to load the files ourselves, we're fine with the directory being invalid/blank.
	if (std::error_code ec; hasBit(_options, Options::LoadExternalBuffers) && (!fs::is_directory(directory, ec) || ec)) {
		return Error::InvalidPath;
	}
#endif

	data.reset();
	auto jsonSpan = data.read(data.totalSize(), SIMDJSON_PADDING);
	padded_string_view view(reinterpret_cast<const std::uint8_t*>(jsonSpan.data()),
									  data.totalSize(),
									  data.totalSize() + SIMDJSON_PADDING);
	ondemand::document document;
	internal::JsonObject root;
	if (jsonParser->parser.iterate(view).get(document) != SUCCESS ||
		document.get_object().get(root.value) != SUCCESS) [[unlikely]] {
		return Error::InvalidJson;
	}

	auto asset = parse(root, categories);
	if (!asset.has_error() && !document.at_end()) [[unlikely]] {
		// ondemand does not reject trailing content by itself.
		return Error::InvalidJson;
	}
	return asset;
}

fg::Expected<fg::Asset> fg::Parser::loadGltfBinary(GltfDataGetter& data, fs::path _directory, Options _options, Category categories) {
	using namespace simdjson;

	options = _options;
	directory = std::move(_directory);
	variantCount = 0;
	glbBuffer = std::monostate();

#if !defined(__ANDROID__)
	// If we never have to load the files ourselves, we're fine with the directory being invalid/blank.
	if (std::error_code ec; hasBit(options, Options::LoadExternalBuffers) && (!fs::is_directory(directory, ec) || ec)) {
		return Error::InvalidPath;
	}
#endif

	data.reset();

	auto header = internal::readBinaryHeader(data);
	if (header.magic != internal::binaryGltfHeaderMagic) {
		return Error::InvalidGLB;
	}
	if (header.version != 2) {
		return Error::UnsupportedVersion;
	}
	if (header.length > data.totalSize()) {
		return Error::InvalidGLB;
	}

	// The glTF 2 spec specifies that in GLB files the order of chunks is predefined. Specifically,
	//  1. JSON chunk
	//  2. BIN chunk (optional)
	auto jsonChunk = internal::readBinaryChunk(data);
	if (jsonChunk.chunkType != internal::binaryGltfJsonChunkMagic ||
		jsonChunk.chunkLength > data.totalSize() - sizeof(internal::BinaryGltfHeader) - sizeof(internal::BinaryGltfChunk)) {
		return Error::InvalidGLB;
	}

	// Create a string view of the JSON chunk in the GLB data buffer. The documentation of parse()
	// says the padding can be initialised to anything, apparently. Therefore, this should work.
	auto jsonSpan = data.read(jsonChunk.chunkLength, SIMDJSON_PADDING);
	simdjson::padded_string_view jsonChunkView(reinterpret_cast<const std::uint8_t*>(jsonSpan.data()),
											   jsonChunk.chunkLength,
											   jsonChunk.chunkLength + SIMDJSON_PADDING);

	// ondemand parses lazily, so the JSON chunk has to stay alive until parse() returns. GltfDataGetter
	// guarantees this, as the binary chunk below is read through the non-padded read overload.
	ondemand::document document;
	internal::JsonObject root;
	if (jsonParser->parser.iterate(jsonChunkView).get(document) != SUCCESS ||
		document.get_object().get(root.value) != SUCCESS) [[unlikely]] {
		return Error::InvalidJson;
	}

	// Is there enough room for another chunk header?
	if (header.length > (data.bytesRead() + sizeof(internal::BinaryGltfChunk))) {
		auto binaryChunk = internal::readBinaryChunk(data);

		if (binaryChunk.chunkType != internal::binaryGltfDataChunkMagic) {
			return Error::InvalidGLB;
		}

		// TODO: Somehow allow skipping the binary part in the future?
		if (binaryChunk.chunkLength != 0) {
			if (binaryChunk.chunkLength > data.totalSize() - data.bytesRead()) {
				return Error::InvalidGLB;
			}

			if (config.mapCallback != nullptr) {
				auto info = config.mapCallback(binaryChunk.chunkLength, config.userPointer);
				if (info.mappedMemory != nullptr) {
					data.read(info.mappedMemory, binaryChunk.chunkLength);
					if (config.unmapCallback != nullptr) {
						config.unmapCallback(&info, config.userPointer);
					}
					glbBuffer = sources::CustomBuffer{info.customId, MimeType::None};
				}
			} else {
				static_vector<std::byte> binaryData(for_overwrite, binaryChunk.chunkLength);
				data.read(binaryData.data(), binaryChunk.chunkLength);

				sources::Array vectorData {
					std::move(binaryData),
					MimeType::GltfBuffer,
				};
				glbBuffer = std::move(vectorData);
			}
		}
	}

	auto asset = parse(root, categories);
	if (!asset.has_error() && !document.at_end()) [[unlikely]] {
		return Error::InvalidJson;
	}
	return asset;
}

void fg::Parser::setBufferAllocationCallback(BufferMapCallback* mapCallback, BufferUnmapCallback* unmapCallback) noexcept {
	if (mapCallback == nullptr)
		unmapCallback = nullptr;
	config.mapCallback = mapCallback;
	config.unmapCallback = unmapCallback;
}

void fg::Parser::setExtrasParseCallback(ExtrasParseCallback *extrasCallback) noexcept {
	config.extrasCallback = extrasCallback;
}

void fg::Parser::setUserPointer(void* pointer) noexcept {
	config.userPointer = pointer;
}
#pragma endregion
