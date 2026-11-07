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

	template <typename T>
	[[nodiscard]] Error parseNumArray(simdjson::ondemand::array array, T& out) noexcept {
		std::size_t i = 0;
		for (auto element : array) {
			double value;
			if (i >= out.size() || element.get_double().get(value) != simdjson::SUCCESS) [[unlikely]]
				return Error::InvalidGltf;
			if (!toNum(value, out[i++])) [[unlikely]]
				return Error::InvalidGltf;
		}
		return i == out.size() ? Error::None : Error::InvalidGltf;
	}

	[[nodiscard]] inline Error parseExtras(const ParserInternalConfig& config, simdjson::ondemand::object& object,
			const std::size_t objectIndex, const Category category) {
		using namespace simdjson;

		if (config.extrasCallback == nullptr)
			return Error::None;

		ondemand::object extrasObject;
		if (const auto error = object["extras"].get_object().get(extrasObject); error == NO_SUCH_FIELD) {
			return Error::None;
		} else if (error != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::string_view extras;
		if (extrasObject.raw_json().get(extras) != SUCCESS) [[unlikely]]
			return Error::InvalidJson;
		config.extrasCallback(extras, objectIndex, category, config.userPointer);
		return Error::None;
	}

	template <typename T>
	Error parseAttributes(simdjson::ondemand::object& object, T& attributes) {
		using namespace simdjson;

		// We iterate through the JSON object and write each key/pair value into the
		// attribute map. The keys are only validated in the validate() method.
		std::size_t count;
		if (object.count_fields().get(count) != SUCCESS) [[unlikely]]
			return Error::InvalidJson;
		attributes.reserve(count);
		for (auto field : object) {
			std::string_view key;
			if (field.unescaped_key().get(key) != SUCCESS) [[unlikely]]
				return Error::InvalidJson;

			std::uint64_t accessorIndex;
			if (field.value().get_uint64().get(accessorIndex) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			if (!isValidIndex(accessorIndex)) [[unlikely]]
				return Error::ValueOutOfRange;
			attributes.emplace_back(Attribute {
				.name = std::string(key),
				.accessorIndex = static_cast<std::size_t>(accessorIndex),
			});
		}
		return Error::None;
	}

	[[nodiscard]] inline Error getImageIndexForExtension(simdjson::simdjson_result<simdjson::ondemand::value> element, optional<std::size_t>& imageIndexOut) {
		using namespace simdjson;

		ondemand::object source;
		if (element.get_object().get(source) != simdjson::SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		std::uint64_t imageIndex;
		if (source["source"].get_uint64().get(imageIndex) != simdjson::SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (!isValidIndex(imageIndex)) [[unlikely]]
			return Error::ValueOutOfRange;

		imageIndexOut = static_cast<std::size_t>(imageIndex);
		return Error::None;
	}

	[[nodiscard]] inline Error parseTextureExtensions(Texture& texture, simdjson::ondemand::object& extensions, const Extensions extensionFlags) {
		for (auto extensionResult : extensions) {
			std::string_view key;
			if (extensionResult.escaped_key().get(key) != simdjson::SUCCESS) [[unlikely]]
				return Error::InvalidJson;

			switch (crc32c(key)) {
				case force_consteval<crc32c(extensions::KHR_texture_basisu)>: {
					if (!hasBit(extensionFlags, Extensions::KHR_texture_basisu))
						break;
					if (auto error = getImageIndexForExtension(extensionResult.value(), texture.basisuImageIndex); error != Error::None)
						return error;
					break;
				}
				case force_consteval<crc32c(extensions::MSFT_texture_dds)>: {
					if (!hasBit(extensionFlags, Extensions::MSFT_texture_dds))
						break;
					if (auto error = getImageIndexForExtension(extensionResult.value(), texture.ddsImageIndex); error != Error::None)
						return error;
					break;
				}
				case force_consteval<crc32c(extensions::EXT_texture_webp)>: {
					if (!hasBit(extensionFlags, Extensions::EXT_texture_webp))
						break;
					if (auto error = getImageIndexForExtension(extensionResult.value(), texture.webpImageIndex); error != Error::None)
						return error;
					break;
				}
				case force_consteval<crc32c(extensions::EXT_texture_avif)>: {
					if (!hasBit(extensionFlags, Extensions::EXT_texture_avif))
						break;
					if (auto error = getImageIndexForExtension(extensionResult.value(), texture.avifImageIndex); error != Error::None)
						return error;
					break;
				}
				default:
					break;
			}
		}

		return Error::None;
	}

	[[nodiscard]] inline Error getJsonArray(simdjson::ondemand::object& parent, std::string_view arrayName, simdjson::ondemand::array* array) noexcept {
		using namespace simdjson;

		const auto error = parent[arrayName].get_array().get(*array);
		if (error == NO_SUCH_FIELD) {
			return Error::MissingField;
		}
		if (error == SUCCESS) [[likely]] {
			return Error::None;
		}
		return Error::InvalidJson;
	}

	template <internal::TextureInfoType type = internal::TextureInfoType::Standard>
	Error parseTextureInfo(simdjson::ondemand::object& object, const std::string_view key,
		TextureInfo* info, const Extensions extensions) noexcept {
		using namespace simdjson;

		ondemand::object child;
		if (auto childErr = object[key].get_object().get(child); childErr == NO_SUCH_FIELD) {
			return Error::MissingField;
		} else if (childErr != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t index;
		if (child["index"].get_uint64().get(index) == SUCCESS) [[likely]] {
			if (!isValidIndex(index)) [[unlikely]]
				return Error::ValueOutOfRange;
			info->textureIndex = static_cast<std::size_t>(index);
		} else {
			return Error::InvalidGltf;
		}

		if (const auto error = child["texCoord"].get_uint64().get(index); error == SUCCESS) [[likely]] {
			if (!isValidIndex(index)) [[unlikely]]
				return Error::ValueOutOfRange;
			info->texCoordIndex = static_cast<std::size_t>(index);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidJson;
		}

		if constexpr (type == internal::TextureInfoType::NormalTexture) {
			double scale;
			if (const auto error = child["scale"].get_double().get(scale); error == SUCCESS) [[likely]] {
				if (!toNum(scale, reinterpret_cast<NormalTextureInfo*>(info)->scale)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		} else if constexpr (type == internal::TextureInfoType::OcclusionTexture) {
			double strength;
			if (const auto error = child["strength"].get_double().get(strength); error == SUCCESS) [[likely]] {
				if (!toNum(strength, reinterpret_cast<OcclusionTextureInfo*>(info)->strength)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		}

		ondemand::object extensionsObject;
		if (child["extensions"].get_object().get(extensionsObject) == SUCCESS) [[likely]] {
			ondemand::object textureTransform;
			if (hasBit(extensions, Extensions::KHR_texture_transform) && extensionsObject[extensions::KHR_texture_transform].get_object().get(textureTransform) == SUCCESS) [[likely]] {
				auto& transform = info->transform.emplace();
				transform.rotation = 0.0F;

				if (textureTransform["texCoord"].get_uint64().get(index) == SUCCESS) [[likely]] {
					if (!isValidIndex(index)) [[unlikely]]
						return Error::ValueOutOfRange;
					transform.texCoordIndex = index;
				}

				double rotation = 0.0F;
				if (textureTransform["rotation"].get_double().get(rotation) == SUCCESS) [[likely]] {
					if (!toNum(rotation, transform.rotation)) [[unlikely]]
						return Error::InvalidGltf;
				}

				ondemand::array array;
				if (textureTransform["offset"].get_array().get(array) == SUCCESS) [[likely]] {
					if (auto error = parseNumArray(array, transform.uvOffset); error != Error::None) [[unlikely]]
						return error;
				}

				if (textureTransform["scale"].get_array().get(array) == SUCCESS) [[likely]] {
					if (auto error = parseNumArray(array, transform.uvScale); error != Error::None) [[unlikely]]
						return error;
				}
			}
		}

		return Error::None;
	}
} // namespace fastgltf

#pragma region enum tag_invoke
namespace simdjson {
	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::AccessorType& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (auto error = val.get_string().get(str); error != simdjson::SUCCESS)
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
		} else {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::ComponentType& out) noexcept {
		using namespace fastgltf;

		std::uint64_t value;
		if (auto error = val.get_uint64().get(value); error != simdjson::SUCCESS)
			return error;

		if (value > std::numeric_limits<std::underlying_type_t<ComponentType>>::max()) {
			return simdjson::INCORRECT_TYPE;
		}
		out = fastgltf::internal::getComponentType(static_cast<std::underlying_type_t<ComponentType>>(value));
		if (out == ComponentType::Invalid) {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::PrimitiveType& out) noexcept {
		using namespace fastgltf;

		std::uint64_t value;
		if (auto error = val.get_uint64().get(value); error != simdjson::SUCCESS)
			return error;

		if (!toEnum(value, out)) {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::BufferTarget& out) noexcept {
		using namespace fastgltf;

		std::uint64_t value;
		if (auto error = val.get_uint64().get(value); error != simdjson::SUCCESS)
			return error;

		if (!toEnum(value, out)) {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::MeshoptCompressionMode& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (auto error = val.get_string().get(str); error != simdjson::SUCCESS)
			return error;

		if (str == "ATTRIBUTES") {
			out = MeshoptCompressionMode::Attributes;
        } else if (str == "TRIANGLES") {
        	out = MeshoptCompressionMode::Triangles;
		} else if (str == "INDICES") {
			out = MeshoptCompressionMode::Indices;
		} else {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::MeshoptCompressionFilter& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (auto error = val.get_string().get(str); error != simdjson::SUCCESS)
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
		} else {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::Filter& out) noexcept {
		using namespace fastgltf;

		std::uint64_t value;
		if (auto error = val.get_uint64().get(value); error != simdjson::SUCCESS)
			return error;

		if (!toEnum(value, out)) {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::Wrap& out) noexcept {
		using namespace fastgltf;

		std::uint64_t value;
		if (auto error = val.get_uint64().get(value); error != simdjson::SUCCESS)
			return error;

		if (!toEnum(value, out)) {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::AlphaMode& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (auto error = val.get_string().get(str); error != simdjson::SUCCESS)
			return error;

		if (str == "OPAQUE") {
			out = AlphaMode::Opaque;
		} else if (str == "MASK") {
			out = AlphaMode::Mask;
		} else if (str == "BLEND") {
			out = AlphaMode::Blend;
		} else {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::AnimationInterpolation& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (auto error = val.get_string().get(str); error != simdjson::SUCCESS)
			return error;

		if (str == "LINEAR") {
			out = AnimationInterpolation::Linear;
		} else if (str == "STEP") {
			out = AnimationInterpolation::Step;
		} else if (str == "CUBICSPLINE") {
			out = AnimationInterpolation::CubicSpline;
		} else {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::LightType& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (auto error = val.get_string().get(str); error != simdjson::SUCCESS)
			return error;

		if (str == "directional") {
			out = LightType::Directional;
		} else if (str == "spot") {
			out = LightType::Spot;
		} else if (str == "point") {
			out = LightType::Point;
		} else {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::CombineMode& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (auto error = val.get_string().get(str); error != simdjson::SUCCESS)
			return error;

		if (str == "average") {
			out = CombineMode::Average;
		} else if (str == "minimum") {
			out = CombineMode::Minimum;
		} else if (str == "maximum") {
			out = CombineMode::Maximum;
		} else if (str == "multiply") {
			out = CombineMode::Multiply;
		} else {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::DriveType& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (auto error = val.get_string().get(str); error != simdjson::SUCCESS)
			return error;

		if (str == "linear") {
			out = DriveType::Linear;
		} else if (str == "angular") {
			out = DriveType::Angular;
		} else {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}

	template <typename T>
	static simdjson::error_code tag_invoke(simdjson::deserialize_tag, T& val, fastgltf::DriveMode& out) noexcept {
		using namespace fastgltf;

		std::string_view str;
		if (auto error = val.get_string().get(str); error != simdjson::SUCCESS)
			return error;

		if (str == "force") {
			out = DriveMode::Force;
		} else if (str == "acceleration") {
			out = DriveMode::Acceleration;
		} else {
			return simdjson::INCORRECT_TYPE;
		}
		return simdjson::SUCCESS;
	}
}
#pragma endregion

#pragma region glTF parsing
fg::Expected<fg::DataSource> fg::Parser::decodeDataUri(const URIView& uri) const noexcept {
	auto path = uri.path();
	const auto mimeEnd = path.find(';');
	const auto mime = path.substr(0, mimeEnd);

	const auto encodingEnd = path.find(',');
	const auto encoding = path.substr(mimeEnd + 1, encodingEnd - mimeEnd - 1);
	if (encoding != "base64") {
		return Error::InvalidURI;
	}

	const auto encodedData = path.substr(encodingEnd + 1);
	if (encodedData.size() < 4 || encodedData.size() % 4 != 0) {
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
		if (majorError != std::errc {} || dot == end || *dot != '.')
			return Error::InvalidVersion;

		const auto majorDigits = dot - version.data();
		if ((majorDigits > 1 && version[0] == '0') || ret.major > max_version_num)
			return Error::InvalidVersion;

		auto [last, minorError] = std::from_chars(std::next(dot), end, ret.minor);
		if (minorError != std::errc {} || last != end)
			return Error::InvalidVersion;

		const auto minorDigits = last - std::next(dot);
		if ((minorDigits > 1 && *std::next(dot) == '0') || ret.minor > max_version_num)
			return Error::InvalidVersion;
		return ret;
	}
}

fg::Error fg::Parser::parseAssetInfo(internal::JsonObject& rootJson, AssetInfo& info) {
	using namespace simdjson;
	auto& root = rootJson.value;

	ondemand::object assetInfo;
	if (const auto error = root["asset"].get_object().get(assetInfo); error == NO_SUCH_FIELD) [[unlikely]] {
		return Error::MissingField;
	} else if (error != SUCCESS) [[unlikely]] {
		return Error::InvalidJson;
	}

	std::string_view versionString;
	if (const auto error = assetInfo["version"].get_string().get(versionString); error != SUCCESS) [[unlikely]] {
		return error == NO_SUCH_FIELD ? Error::MissingField : Error::InvalidVersion;
	}

	auto version = detail::parseVersionString(versionString);
	if (version.has_error())
		return version.error();
	if (version->major != 2) [[unlikely]]
		return Error::UnsupportedVersion;
	info.version = *version;

	std::string_view copyright;
	if (assetInfo["copyright"].get_string().get(copyright) == SUCCESS) [[likely]] {
		info.copyright = copyright;
	}

	std::string_view generator;
	if (assetInfo["generator"].get_string().get(generator) == SUCCESS) [[likely]] {
		info.generator = generator;
	}

	std::string_view minVersionString;
	if (const auto strError = assetInfo["minVersion"].get_string().get(minVersionString);
		strError == SUCCESS) {
		if (auto minVersion = detail::parseVersionString(minVersionString); minVersion.has_error()) {
			return minVersion.error();
		} else {
			if (*minVersion > Version { 2, 0 }) [[unlikely]]
				return Error::UnsupportedVersion;
			info.minVersion = *minVersion;
		}
	} else if (strError != NO_SUCH_FIELD) [[unlikely]] {
		return Error::InvalidVersion;
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

	if (ondemand::array extensionsRequired; root["extensionsRequired"].get_array().get(extensionsRequired) == SUCCESS) [[likely]] {
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
			if (!known) {
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
		if (root["extensions"].get_object().get(extensionsObject) == SUCCESS &&
			extensionsObject[extensions::KHR_materials_variants].get_object().get(variantsObject) == SUCCESS &&
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

			if (auto error = parseExtensions(extensionsObject, asset); error != Error::None)
				return error;
			continue;
		}

		if (hashedKey == force_consteval<crc32c("asset")> || hashedKey == force_consteval<crc32c("extras")>) {
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
					if (auto eError = usedValue.get_string().get(usedString); eError == SUCCESS) [[likely]] {
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

		if (error != Error::None)
			return error;

#undef KEY_SWITCH_CASE
	}

	asset.availableCategories = readCategories;

	if (hasBit(options, Options::GenerateMeshIndices)) {
		if (auto error = generateMeshIndices(asset); error != Error::None) {
			return error;
		}
	}

	// Resize primitive mappings to match the global variant count
	if (hasBit(config.extensions, Extensions::KHR_materials_variants) && !asset.materialVariants.empty()) {
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
	template <typename T>
	requires AccessorBoundsArray::is_valid_type_v<T>
	static Error parseMinMaxArray(simdjson::ondemand::array elements, AccessorBoundsArray& array) {
		using namespace simdjson;

		std::size_t size;
		if (elements.count_elements().get(size) != SUCCESS) [[unlikely]]
			return Error::InvalidJson;
		if (size != array.size()) [[unlikely]]
			return Error::InvalidGltf;

		auto* data = array.data<T>();

		std::size_t idx = 0;
		for (auto element : elements) {
			if constexpr (std::is_same_v<T, double>) {
				double value;
				if (element.get_double().get(value) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				data[idx++] = value;
			} else if constexpr (std::is_same_v<T, std::int64_t>) {
				ondemand::number_type type;
				if (element.get_number_type().get(type) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				if (type == ondemand::number_type::floating_point_number) {
					double value;
					if (element.get_double().get(value) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
					// Reject any values outside of the 2^63 range of std::int64_t
					if (!(value >= -0x1p63 && value < 0x1p63)) [[unlikely]] {
						return Error::InvalidGltf;
					}
					data[idx++] = static_cast<std::int64_t>(value);
				} else {
					// Note that the glTF spec doesn't care about any integer larger than 32-bits, so
					// truncating uint64 to int64 wouldn't make any difference, as those large values
					// aren't allowed anyway.
					std::int64_t value;
					if (element.get_int64().get(value) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
					data[idx++] = value;
				}
			}
		}

		return Error::None;
	}

	static Error parseMinMax(Accessor& accessor, simdjson::ondemand::object& accessorObject, const std::string_view key, optional<AccessorBoundsArray>& ref) {
		using namespace simdjson;

		ondemand::array elements;
		if (const auto error = accessorObject[key].get_array().get(elements);
			error == SUCCESS) [[likely]] {
			const auto num = getNumComponents(accessor.type);
			if (num == 0) [[unlikely]]
				return Error::InvalidGltf;

			if (isFloatingPoint(accessor.componentType)) {
				ref = AccessorBoundsArray(num, AccessorBoundsArray::BoundsType::float64);
				return parseMinMaxArray<double>(elements, ref.value());
			} else {
				ref = AccessorBoundsArray(num, AccessorBoundsArray::BoundsType::int64);
				return parseMinMaxArray<std::int64_t>(elements, ref.value());
			}
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}
		return Error::None;
	}
}

fg::Error fg::Parser::parseAccessors(internal::JsonArray& accessorsJson, Asset& asset) {
	using namespace simdjson;
	auto& accessors = accessorsJson.value;

	if (auto reserveError = reserveElements(accessors, asset.accessors); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto accessorValue : accessors) {
		// Required fields: "componentType", "count"
		Accessor accessor {};
		ondemand::object accessorObject;
		if (accessorValue.get_object().get(accessorObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t componentType;
		if (accessorObject["componentType"].get_uint64().get(componentType) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		accessor.componentType = internal::getComponentType(static_cast<std::underlying_type_t<ComponentType>>(componentType));
		if (accessor.componentType == ComponentType::Double && !(hasBit(options, Options::AllowDouble) || hasBit(config.extensions, Extensions::KHR_accessor_float64))) {
			return Error::InvalidGltf;
		}

		if (accessorObject["type"].get(accessor.type) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t accessorCount;
		if (accessorObject["count"].get_uint64().get(accessorCount) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		accessor.count = static_cast<std::size_t>(accessorCount);


		std::uint64_t bufferView;
		if (accessorObject["bufferView"].get_uint64().get(bufferView) == SUCCESS) [[likely]] {
			if (!isValidIndex(bufferView)) [[unlikely]]
				return Error::ValueOutOfRange;
			accessor.bufferViewIndex = static_cast<std::size_t>(bufferView);
		}

		// byteOffset is optional, but defaults to 0
		std::uint64_t byteOffset;
		if (auto error = accessorObject["byteOffset"].get_uint64().get(byteOffset); error == SUCCESS) [[likely]] {
			accessor.byteOffset = static_cast<std::size_t>(byteOffset);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (auto error = parseMinMax(accessor, accessorObject, "max", accessor.max); error != Error::None) {
			return error;
		}
		if (auto error = parseMinMax(accessor, accessorObject, "min", accessor.min); error != Error::None) {
			return error;
		}

		if (auto error = accessorObject["normalized"].get_bool().get(accessor.normalized); error != SUCCESS && error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		// This property MUST NOT be set to true for accessors with FLOAT or UNSIGNED_INT component type.
		if (accessor.normalized && (accessor.componentType == ComponentType::UnsignedInt || isFloatingPoint(accessor.componentType))) {
			return Error::InvalidGltf;
		}

		ondemand::object sparseAccessorObject;
		if (accessorObject["sparse"].get_object().get(sparseAccessorObject) == SUCCESS) [[likely]] {
			SparseAccessor sparse {};
			std::uint64_t value;
			ondemand::object child;
			if (sparseAccessorObject["count"].get_uint64().get(value) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			sparse.count = static_cast<std::size_t>(value);

			// Accessor Sparce Indices
			if (sparseAccessorObject["indices"].get_object().get(child) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			if (child["bufferView"].get_uint64().get(value) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			if (!isValidIndex(value)) [[unlikely]]
				return Error::ValueOutOfRange;
			sparse.indicesBufferView = static_cast<std::size_t>(value);

			if (auto error = child["byteOffset"].get_uint64().get(value); error == SUCCESS) [[likely]] {
				sparse.indicesByteOffset = static_cast<std::size_t>(value);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			if (child["componentType"].get_uint64().get(value) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			if (const auto componentType = internal::getComponentType(static_cast<std::underlying_type_t<ComponentType>>(value));
				componentType != ComponentType::Invalid) {
				sparse.indexComponentType = componentType;
			} else [[unlikely]] {
				return Error::InvalidGltf;
			}

			// Accessor Sparse Values
			if (sparseAccessorObject["values"].get_object().get(child) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			if (child["bufferView"].get_uint64().get(value) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			if (!isValidIndex(value)) [[unlikely]]
				return Error::ValueOutOfRange;
			sparse.valuesBufferView = static_cast<std::size_t>(value);

			if (auto error = child["byteOffset"].get_uint64().get(value); error == SUCCESS) [[likely]] {
				sparse.valuesByteOffset = static_cast<std::size_t>(value);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			accessor.sparse = sparse;
		}

		if (auto extrasError = parseExtras(config, accessorObject, asset.accessors.size(), Category::Accessors); extrasError != Error::None) [[unlikely]]
			return extrasError;

		std::string_view name;
		if (accessorObject["name"].get_string().get(name) == SUCCESS) {
			accessor.name.assign(name);
		}

		asset.accessors.emplace_back(std::move(accessor));
	}

	return Error::None;
}

fg::Error fg::Parser::parseAnimations(internal::JsonArray& animationsJson, Asset& asset) {
	using namespace simdjson;
	auto& animations = animationsJson.value;

	if (auto reserveError = reserveElements(animations, asset.animations); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto animationValue : animations) {
		ondemand::object animationObject;
		Animation animation {};
		if (animationValue.get_object().get(animationObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::array channels;
		auto channelError = getJsonArray(animationObject, "channels", &channels);
		if (channelError != Error::None) {
			return Error::InvalidGltf;
		}

		if (auto reserveError = reserveElements(channels, animation.channels); reserveError != Error::None) [[unlikely]]
			return reserveError;
		for (auto channelValue : channels) {
			ondemand::object channelObject;
			AnimationChannel channel {};
			if (channelValue.get_object().get(channelObject) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			std::uint64_t sampler;
			if (channelObject["sampler"].get_uint64().get(sampler) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			if (!isValidIndex(sampler)) [[unlikely]]
				return Error::ValueOutOfRange;
			channel.samplerIndex = static_cast<std::size_t>(sampler);

			ondemand::object targetObject;
			if (channelObject["target"].get_object().get(targetObject) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			} else {
				std::uint64_t node;
				if (auto error = targetObject["node"].get_uint64().get(node); error == SUCCESS) [[likely]] {
					if (!isValidIndex(node)) [[unlikely]]
						return Error::ValueOutOfRange;
					channel.nodeIndex = static_cast<std::size_t>(node);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					// the glTF spec allows the node index of an animation channel to be absent, and requires
					// implementations to just ignore the animation if no extension exists to override it.
					// > When node isn't defined, channel SHOULD be ignored.
					return Error::InvalidGltf;
				}

				std::string_view path;
				if (targetObject["path"].get_string().get(path) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
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

		ondemand::array samplers;
		auto samplerError = getJsonArray(animationObject, "samplers", &samplers);
		if (samplerError != Error::None) {
			return Error::InvalidGltf;
		}

		if (auto reserveError = reserveElements(samplers, animation.samplers); reserveError != Error::None) [[unlikely]]
			return reserveError;
		for (auto samplerValue : samplers) {
			ondemand::object samplerObject;
			AnimationSampler sampler {};
			if (samplerValue.get_object().get(samplerObject) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			std::uint64_t input;
			if (samplerObject["input"].get_uint64().get(input) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			if (!isValidIndex(input)) [[unlikely]]
				return Error::ValueOutOfRange;
			sampler.inputAccessor = static_cast<std::size_t>(input);

			std::uint64_t output;
			if (samplerObject["output"].get_uint64().get(output) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			if (!isValidIndex(output)) [[unlikely]]
				return Error::ValueOutOfRange;
			sampler.outputAccessor = static_cast<std::size_t>(output);

			std::string_view interpolation;
			if (samplerObject["interpolation"].get_string().get(interpolation) != SUCCESS) [[unlikely]] {
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

		if (auto extrasError = parseExtras(config, animationObject, asset.animations.size(), Category::Animations); extrasError != Error::None) [[unlikely]]
			return extrasError;


		std::string_view name;
		if (animationObject["name"].get_string().get(name) == SUCCESS) {
			animation.name.assign(name);
		}

		asset.animations.emplace_back(std::move(animation));
	}

	return Error::None;
}

fg::Error fg::Parser::parseBuffers(internal::JsonArray& buffersJson, Asset& asset) {
	using namespace simdjson;
	auto& buffers = buffersJson.value;

	if (auto reserveError = reserveElements(buffers, asset.buffers); reserveError != Error::None) [[unlikely]]
		return reserveError;
	std::size_t bufferIndex = 0;
	for (auto bufferValue : buffers) {
		// Required fields: "byteLength"
		Buffer buffer {};
		ondemand::object bufferObject;
		if (bufferValue.get_object().get(bufferObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t byteLength;
		if (bufferObject["byteLength"].get_uint64().get(byteLength) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		buffer.byteLength = static_cast<std::size_t>(byteLength);

		// The spec for EXT_meshopt_compression allows so-called 'fallback buffers' which only exist to
		// act as a valid fallback for compressed buffer views, but actually do not contain any data.
		// In these cases, there is either simply no URI, or a fallback boolean is added to the extensions'
		// extension field.
		// In these cases, fastgltf could just leave the std::monostate in the DataSource.
		// However, to make the actual use of these buffers clear, we'll use an empty fallback type.
		bool meshoptCompressionRequired = false;
		for (const auto& extension : asset.extensionsRequired) {
			if (extension == extensions::EXT_meshopt_compression || extension == extensions::KHR_meshopt_compression) {
				meshoptCompressionRequired = true;
			}
		}

		// When parsing GLB, there's a buffer object that will point to the BUF chunk in the
		// file. Otherwise, data must be specified in the "uri" field.
		std::string_view uriString;
		if (bufferObject["uri"].get_string().get(uriString) == SUCCESS) [[likely]] {
			URIView uriView(uriString);

			if (!uriView.valid()) {
				return Error::InvalidURI;
			}

			if (uriView.isDataUri()) {
				auto decoded = decodeDataUri(uriView);
				if (decoded.has_error()) {
					return decoded.error();
				}

				buffer.data = std::move(decoded.get());
			} else if (uriView.isLocalPath() && hasBit(options, Options::LoadExternalBuffers)) {
				auto file = loadFileFromUri(uriView);
				if (file.has_error()) {
					return file.error();
				}

				buffer.data = std::move(file.get());
			} else {
				sources::URI filePath;
				filePath.fileByteOffset = 0;
				filePath.uri = uriView;
				buffer.data = std::move(filePath);
			}
		} else if (bufferIndex == 0 && !std::holds_alternative<std::monostate>(glbBuffer)) {
			buffer.data = std::move(glbBuffer);
		} else if (meshoptCompressionRequired) {
			// This buffer is not a GLB buffer and has no URI source and is therefore a fallback.
			buffer.data = sources::Fallback();
		} else {
			// All other buffers have to contain an uri field.
			return Error::InvalidGltf;
		}

		if (std::holds_alternative<std::monostate>(buffer.data)) {
			return Error::InvalidGltf;
		}

		if (auto extrasError = parseExtras(config, bufferObject, asset.buffers.size(), Category::Buffers); extrasError != Error::None) [[unlikely]]
			return extrasError;

		std::string_view name;
		if (bufferObject["name"].get_string().get(name) == SUCCESS) {
			buffer.name.assign(name);
		}

		++bufferIndex;
		asset.buffers.emplace_back(std::move(buffer));
	}

	return Error::None;
}

namespace fastgltf::internal {
	inline auto parseMeshoptCompression(
			CompressedBufferView& compression, simdjson::ondemand::object& object) {
		using namespace simdjson;

		std::optional<std::uint64_t> buffer, byteLength, byteStride, count;
		std::uint64_t byteOffset = 0;
		std::optional<MeshoptCompressionMode> mode;
		MeshoptCompressionFilter filter = MeshoptCompressionFilter::None;

		auto result = object.for_each<"buffer", "byteOffset", "byteLength", "byteStride", "count", "mode", "filter">(
			buffer, byteOffset, byteLength, byteStride, count, mode, filter
		);
		if (result.error != SUCCESS) [[unlikely]] {
			return result.error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
		}

		if (!buffer || !byteLength || !byteStride || !count || !mode) [[unlikely]]
			return Error::InvalidGltf;

		if (!isValidIndex(*buffer)) [[unlikely]]
			return Error::ValueOutOfRange;

		compression.bufferIndex = static_cast<std::size_t>(*buffer);
		compression.byteOffset = static_cast<std::size_t>(byteOffset);
		compression.byteLength = static_cast<std::size_t>(*byteLength);
		compression.byteStride = static_cast<std::size_t>(*byteStride);
		compression.count = static_cast<std::size_t>(*count);
		compression.mode = *mode;
		compression.filter = filter;
		return Error::None;
	}
}

fg::Error fg::Parser::parseBufferViews(internal::JsonArray& bufferViewsJson, Asset& asset) {
	using namespace simdjson;
	auto& bufferViews = bufferViewsJson.value;

	if (auto reserveError = reserveElements(bufferViews, asset.bufferViews); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto bufferViewValue : bufferViews) {
		ondemand::object bufferViewObject;
		if (bufferViewValue.get_object().get(bufferViewObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::optional<std::uint64_t> buffer, byteLength, byteStride, target;
		std::uint64_t byteOffset = 0;
		std::string_view name;
		optional<CompressedBufferView> meshoptCompression;

		auto result = bufferViewObject.for_each<"buffer", "byteOffset", "byteLength", "byteStride", "target", "name", "extensions", "extras">(
			buffer, byteOffset, byteLength, byteStride, target, name,
			[&](ondemand::value v) -> simdjson::error_code {
				ondemand::object extensionObject;
				if (const auto error = v.get_object().get(extensionObject);
					error != simdjson::SUCCESS) [[unlikely]] {
					return error;
				}

				ondemand::object meshoptCompressionObject;
				if (hasBit(config.extensions, Extensions::KHR_meshopt_compression) &&
					extensionObject[extensions::KHR_meshopt_compression].get_object().get(meshoptCompressionObject) == SUCCESS) {

					auto& compression = meshoptCompression.emplace();
					if (const auto error = internal::parseMeshoptCompression(compression, meshoptCompressionObject);
						error != Error::None) [[unlikely]] {
						return simdjson::UNEXPECTED_ERROR;
					}
				} else if (hasBit(config.extensions, Extensions::EXT_meshopt_compression) &&
					extensionObject[extensions::EXT_meshopt_compression].get_object().get(meshoptCompressionObject) == SUCCESS) {

					auto& compression = meshoptCompression.emplace();
					if (const auto error = internal::parseMeshoptCompression(compression, meshoptCompressionObject);
						error != Error::None) [[unlikely]] {
						return simdjson::UNEXPECTED_ERROR;
					}
				}
				return simdjson::SUCCESS;
			},
			[&](ondemand::value v) -> simdjson::error_code {
				std::string_view extras;
				if (const auto error = v.raw_json().get(extras);
					error != SUCCESS) [[unlikely]]
					return error;
				config.extrasCallback(extras, asset.bufferViews.size(), Category::BufferViews, config.userPointer);
				return simdjson::SUCCESS;
			}
		);
		if (result.error != SUCCESS) [[unlikely]] {
			return result.error == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
		}

		if (!buffer || !byteLength) [[unlikely]]
			return Error::InvalidGltf;
		if (!isValidIndex(*buffer)) [[unlikely]]
			return Error::ValueOutOfRange;

		auto& view = asset.bufferViews.emplace_back(BufferView {
			.bufferIndex = static_cast<std::size_t>(*buffer),
			.byteOffset = static_cast<std::size_t>(byteOffset),
			.byteLength = static_cast<std::size_t>(*byteLength),
			.byteStride = std::nullopt,
			.target = std::nullopt,
			.meshoptCompression = std::nullopt,
			.name = std::string(name),
		});

		if (byteStride.has_value()) {
			if (!isValidOptionalValue<std::size_t>(*byteStride)) [[unlikely]]
				return Error::ValueOutOfRange;
			view.byteStride = static_cast<std::size_t>(*byteStride);
		}

		if (target.has_value()) {
			if (!toEnum<BufferTarget>(*target, view.target)) [[unlikely]]
				return Error::InvalidGltf;
		}

		if (meshoptCompression.has_value()) {
			view.meshoptCompression = std::move(*meshoptCompression);
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parseCameras(internal::JsonArray& camerasJson, Asset& asset) {
	using namespace simdjson;
	auto& cameras = camerasJson.value;

	if (auto reserveError = reserveElements(cameras, asset.cameras); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto cameraValue : cameras) {
		Camera camera {};
		ondemand::object cameraObject;
		if (cameraValue.get_object().get(cameraObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::string_view name;
		if (cameraObject["name"].get_string().get(name) == SUCCESS) {
			camera.name.assign(name);
		}

		std::string_view type;
		if (cameraObject["type"].get_string().get(type) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (type == "perspective") {
			ondemand::object perspectiveCamera;
			if (cameraObject["perspective"].get_object().get(perspectiveCamera) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			Camera::Perspective perspective {};
			double value;
			if (auto error = perspectiveCamera["aspectRatio"].get_double().get(value); error == SUCCESS) [[likely]] {
				if (!toNum(value, perspective.aspectRatio)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}

			if (auto error = perspectiveCamera["zfar"].get_double().get(value); error == SUCCESS) [[likely]] {
				if (!toNum(value, perspective.zfar)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}

			if (perspectiveCamera["yfov"].get_double().get(value) == SUCCESS) [[likely]] {
				if (!toNum(value, perspective.yfov)) [[unlikely]]
					return Error::InvalidGltf;
			} else {
				return Error::InvalidGltf;
			}

			if (perspectiveCamera["znear"].get_double().get(value) == SUCCESS) [[likely]] {
				if (!toNum(value, perspective.znear)) [[unlikely]]
					return Error::InvalidGltf;
			} else {
				return Error::InvalidGltf;
			}

			camera.camera = perspective;
		} else if (type == "orthographic") {
			ondemand::object orthographicCamera;
			if (cameraObject["orthographic"].get_object().get(orthographicCamera) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			Camera::Orthographic orthographic {};
			double value;
			if (orthographicCamera["xmag"].get_double().get(value) == SUCCESS) [[likely]] {
				if (!toNum(value, orthographic.xmag)) [[unlikely]]
					return Error::InvalidGltf;
			} else {
				return Error::InvalidGltf;
			}

			if (orthographicCamera["ymag"].get_double().get(value) == SUCCESS) [[likely]] {
				if (!toNum(value, orthographic.ymag)) [[unlikely]]
					return Error::InvalidGltf;
			} else {
				return Error::InvalidGltf;
			}

			if (orthographicCamera["zfar"].get_double().get(value) == SUCCESS) [[likely]] {
				if (!toNum(value, orthographic.zfar)) [[unlikely]]
					return Error::InvalidGltf;
			} else {
				return Error::InvalidGltf;
			}

			if (orthographicCamera["znear"].get_double().get(value) == SUCCESS) [[likely]] {
				if (!toNum(value, orthographic.znear)) [[unlikely]]
					return Error::InvalidGltf;
			} else {
				return Error::InvalidGltf;
			}

			camera.camera = orthographic;
		} else {
			return Error::InvalidGltf;
		}

		if (auto extrasError = parseExtras(config, cameraObject, asset.cameras.size(), Category::Cameras); extrasError != Error::None) [[unlikely]]
			return extrasError;

		asset.cameras.emplace_back(std::move(camera));
	}

	return Error::None;
}

fg::Error fg::Parser::parseExtensions(internal::JsonObject& extensionsObjectJson, Asset& asset) {
	using namespace simdjson;
	auto& extensionsObject = extensionsObjectJson.value;

	for (auto extensionValue : extensionsObject) {
		std::string_view key;
		if (extensionValue.escaped_key().get(key) != SUCCESS) [[unlikely]]
				return Error::InvalidJson;
		auto hashedKey = crc32c(key);

		ondemand::object extensionObject;
		if (auto error = extensionValue.value().get_object().get(extensionObject); error != SUCCESS) [[unlikely]] {
			if (error == INCORRECT_TYPE) {
				continue; // We want to ignore
			}
			return Error::InvalidGltf;
		}

		switch (hashedKey) {
			case force_consteval<crc32c(extensions::KHR_lights_punctual)>: {
				if (!hasBit(config.extensions, Extensions::KHR_lights_punctual))
					break;

				internal::JsonArray lightsArray;
				if (auto error = extensionObject["lights"].get_array().get(lightsArray.value); error == SUCCESS) [[likely]] {
					if (auto lightsError = parseLights(lightsArray, asset); lightsError != Error::None)
						return lightsError;
				} else if (error != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_variants)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_variants))
					break;

				ondemand::array variantsArray;
				if (auto arrayError = extensionObject["variants"].get_array().get(variantsArray); arrayError == SUCCESS) [[likely]] {
					if (auto reserveError = reserveElements(variantsArray, asset.materialVariants); reserveError != Error::None) [[unlikely]]
						return reserveError;
					for (auto variant : variantsArray) {
						ondemand::object variantObject;
						if (auto error = variant.get_object().get(variantObject); error == SUCCESS) {
							std::string_view name;
							if (variantObject["name"].get_string().get(name) != SUCCESS) {
								return Error::InvalidGltf;
							}
							asset.materialVariants.emplace_back(name);
						} else {
							return Error::InvalidGltf;
						}
					}
				} else {
					return Error::InvalidGltf;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_implicit_shapes)>: {
				if (!hasBit(config.extensions, Extensions::KHR_implicit_shapes)) {
					break;
				}

				internal::JsonArray shapesArray;
				if (auto arrayError = extensionObject["shapes"].get_array().get(shapesArray.value); arrayError == SUCCESS) [[likely]] {
					if (auto shapesError = parseShapes(shapesArray, asset); shapesError != Error::None) [[unlikely]] {
						return shapesError;
					}
				} else {
					return Error::InvalidGltf;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_physics_rigid_bodies)>: {
				if (!hasBit(config.extensions, Extensions::KHR_physics_rigid_bodies)) {
					break;
				}

				internal::JsonArray materialsArray;
				if (auto arrayError = extensionObject["physicsMaterials"].get_array().get(materialsArray.value); arrayError == SUCCESS) {
					if (auto materialsError = parsePhysicsMaterials(materialsArray, asset); materialsError != Error::None) [[unlikely]] {
						return materialsError;
					}
				} else if (arrayError != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				internal::JsonArray filtersArray;
				if (auto arrayError = extensionObject["collisionFilters"].get_array().get(filtersArray.value); arrayError == SUCCESS) {
					if (auto filtersError = parseCollisionFilters(filtersArray, asset); filtersError != Error::None) [[unlikely]] {
						return filtersError;
					}
				} else if (arrayError != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				internal::JsonArray jointsArray;
				if (auto arrayError = extensionObject["physicsJoints"].get_array().get(jointsArray.value); arrayError == SUCCESS) {
					if (auto jointsError = parsePhysicsJoints(jointsArray, asset); jointsError != Error::None) [[unlikely]] {
						return jointsError;
					}
				} else if (arrayError != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				break;
			}
			default:
				continue;
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parseImages(internal::JsonArray& imagesJson, Asset& asset) {
	using namespace simdjson;
	auto& images = imagesJson.value;

	if (auto reserveError = reserveElements(images, asset.images); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto imageValue : images) {
		Image image {};
		ondemand::object imageObject;
		if (imageValue.get_object().get(imageObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::string_view uriString;
		if (imageObject["uri"].get_string().get(uriString) == SUCCESS) [[likely]] {
			if (imageObject["bufferView"].error() == SUCCESS) [[likely]] {
				// If uri is declared, bufferView cannot be declared.
				return Error::InvalidGltf;
			}

			URIView uriView(uriString);
			if (!uriView.valid()) {
				return Error::InvalidURI;
			}

			if (uriView.isDataUri()) {
				auto decoded = decodeDataUri(uriView);
				if (decoded.has_error()) {
					return decoded.error();
				}

				image.data = std::move(decoded.get());
			} else if (uriView.isLocalPath() && hasBit(options, Options::LoadExternalImages)) {
				auto file = loadFileFromUri(uriView);
				if (file.has_error()) {
					return file.error();
				}

				image.data = std::move(file.get());
			} else {
				sources::URI filePath;
				filePath.fileByteOffset = 0;
				filePath.uri = uriView;
				image.data = std::move(filePath);
			}

			std::string_view mimeType;
			if (imageObject["mimeType"].get_string().get(mimeType) == SUCCESS) [[likely]] {
				std::visit([&]<typename T>(T& arg) {
					// This is kinda cursed
					if constexpr (is_any_of_v<T, sources::CustomBuffer, sources::BufferView, sources::URI, sources::Array, sources::Vector>) {
						arg.mimeType = internal::getMimeTypeFromString(mimeType);
					}
				}, image.data);
			}
		}

		std::uint64_t bufferViewIndex;
		if (imageObject["bufferView"].get_uint64().get(bufferViewIndex) == SUCCESS) [[likely]] {
			std::string_view mimeType;
			if (imageObject["mimeType"].get_string().get(mimeType) != SUCCESS) [[unlikely]] {
				// If bufferView is defined, mimeType needs to also be defined.
				return Error::InvalidGltf;
			}

			if (!isValidIndex(bufferViewIndex)) [[unlikely]]
				return Error::ValueOutOfRange;
			image.data = sources::BufferView {
				.bufferViewIndex = static_cast<std::size_t>(bufferViewIndex),
				.mimeType = internal::getMimeTypeFromString(mimeType),
			};
		}

		if (std::holds_alternative<std::monostate>(image.data)) {
			return Error::InvalidGltf;
		}

		if (auto extrasError = parseExtras(config, imageObject, asset.images.size(), Category::Images); extrasError != Error::None) [[unlikely]]
			return extrasError;

		// name is optional.
		std::string_view name;
		if (imageObject["name"].get_string().get(name) == SUCCESS) {
			image.name.assign(name);
		}

		asset.images.emplace_back(std::move(image));
	}

	return Error::None;
}

fg::Error fg::Parser::parseLights(internal::JsonArray& lightsJson, Asset& asset) {
	using namespace simdjson;
	auto& lights = lightsJson.value;

	if (auto reserveError = reserveElements(lights, asset.lights); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto lightValue : lights) {
		ondemand::object lightObject;
		if (lightValue.get_object().get(lightObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		Light light {};

		std::string_view type;
		if (lightObject["type"].get_string().get(type) == SUCCESS) [[likely]] {
			switch (crc32c(type)) {
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
		} else {
			return Error::InvalidGltf;
		}

		if (light.type == LightType::Spot) {
			ondemand::object spotObject;
			if (lightObject["spot"].get_object().get(spotObject) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			double innerConeAngle;
			if (auto error = spotObject["innerConeAngle"].get_double().get(innerConeAngle); error == SUCCESS) [[likely]] {
				if (!toNum(innerConeAngle, light.innerConeAngle)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error == NO_SUCH_FIELD) {
				light.innerConeAngle = 0.0f;
			} else {
				return Error::InvalidGltf;
			}

			double outerConeAngle;
			if (auto error = spotObject["outerConeAngle"].get_double().get(outerConeAngle); error == SUCCESS) [[likely]] {
				if (!toNum(outerConeAngle, light.outerConeAngle)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error == NO_SUCH_FIELD) {
				light.outerConeAngle = static_cast<num>(std::numbers::pi / 4.0);
			} else {
				return Error::InvalidGltf;
			}
		}

		ondemand::array colorArray;
		if (auto error = lightObject["color"].get_array().get(colorArray); error == SUCCESS) [[likely]] {
			if (auto colorError = parseNumArray(colorArray, light.color); colorError != Error::None) [[unlikely]]
				return colorError;
		} else if (error == NO_SUCH_FIELD) {
			light.color = math::nvec3(1);
		} else {
			return Error::InvalidGltf;
		}

		double intensity;
		if (lightObject["intensity"].get_double().get(intensity) == SUCCESS) [[likely]] {
			if (!toNum(intensity, light.intensity)) [[unlikely]]
				return Error::InvalidGltf;
		} else {
			light.intensity = 1.0f;
		}

		double range;
		if (lightObject["range"].get_double().get(range) == SUCCESS) [[likely]] {
			if (!toNum(range, light.range)) [[unlikely]]
				return Error::InvalidGltf;
		}

		std::string_view name;
		if (lightObject["name"].get_string().get(name) == SUCCESS) {
			light.name.assign(name);
		}

		asset.lights.emplace_back(std::move(light));
	}

	return Error::None;
}

fg::Error fg::Parser::parseMaterialExtensions(internal::JsonObject& objectJson, Material &material) {
	using namespace simdjson;
	auto& object = objectJson.value;

	for (auto extensionField : object) {
		std::string_view key;
		if (extensionField.escaped_key().get(key) != SUCCESS) [[unlikely]]
				return Error::InvalidJson;
		auto hashedKey = crc32c(key);

		switch (hashedKey) {
			case force_consteval<crc32c(extensions::KHR_materials_anisotropy)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_anisotropy))
					break;

				ondemand::object anisotropyObject;
				auto anisotropyError = extensionField.value().get_object().get(anisotropyObject);
				if (anisotropyError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& anisotropy = material.anisotropy.emplace();

				double anisotropyStrength;
				if (auto error = anisotropyObject["anisotropyStrength"].get_double().get(anisotropyStrength);
					error == SUCCESS) [[likely]] {
					if (!toNum(anisotropyStrength, anisotropy.anisotropyStrength)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidJson;
				}

				double anisotropyRotation;
				if (auto error = anisotropyObject["anisotropyRotation"].get_double().get(anisotropyRotation);
					error == SUCCESS) [[likely]] {
					if (!toNum(anisotropyRotation, anisotropy.anisotropyRotation)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidJson;
				}

				TextureInfo anisotropyTexture;
				if (auto error = parseTextureInfo(anisotropyObject, "anisotropyTexture", &anisotropyTexture,
												  config.extensions); error == Error::None) [[likely]] {
					anisotropy.anisotropyTexture = std::move(anisotropyTexture);
				} else if (error != Error::MissingField) {
					return error;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_clearcoat)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_clearcoat))
					break;

				ondemand::object clearcoatObject;
				auto clearcoatError = extensionField.value().get_object().get(clearcoatObject);
				if (clearcoatError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& clearcoat = material.clearcoat.emplace();

				double clearcoatFactor;
				if (auto error = clearcoatObject["clearcoatFactor"].get_double().get(clearcoatFactor);
					error == SUCCESS) {
					if (!toNum(clearcoatFactor, clearcoat.clearcoatFactor)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidJson;
				}

				TextureInfo clearcoatTexture;
				if (auto error = parseTextureInfo(clearcoatObject, "clearcoatTexture", &clearcoatTexture,
												  config.extensions); error == Error::None) [[likely]] {
					clearcoat.clearcoatTexture = std::move(clearcoatTexture);
				} else if (error != Error::MissingField) {
					return error;
				}

				double clearcoatRoughnessFactor;
				if (auto error = clearcoatObject["clearcoatRoughnessFactor"].get_double().get(
							clearcoatRoughnessFactor); error == SUCCESS) [[likely]] {
					if (!toNum(clearcoatRoughnessFactor, clearcoat.clearcoatRoughnessFactor)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidJson;
				}

				TextureInfo clearcoatRoughnessTexture;
				if (auto error = parseTextureInfo(clearcoatObject, "clearcoatRoughnessTexture",
												  &clearcoatRoughnessTexture, config.extensions); error ==
																								  Error::None) {
					clearcoat.clearcoatRoughnessTexture = std::move(clearcoatRoughnessTexture);
				} else if (error != Error::MissingField) {
					return error;
				}

				NormalTextureInfo clearcoatNormalTexture;
				if (auto error = parseTextureInfo<internal::TextureInfoType::NormalTexture>(clearcoatObject, "clearcoatNormalTexture",
												  &clearcoatNormalTexture, config.extensions); error ==
																							   Error::None) {
					clearcoat.clearcoatNormalTexture = std::move(clearcoatNormalTexture);
				} else if (error != Error::MissingField) {
					return error;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_dispersion)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_dispersion))
					break;

				ondemand::object dispersionObject;
				auto dispersionError = extensionField.value().get_object().get(dispersionObject);
				if (dispersionError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double dispersionFactor;
				auto error = dispersionObject["dispersion"].get_double().get(dispersionFactor);
				if (error == SUCCESS) [[likely]] {
					if (!toNum(dispersionFactor, material.dispersion)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_emissive_strength)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_emissive_strength))
					break;

				ondemand::object emissiveObject;
				auto emissiveError = extensionField.value().get_object().get(emissiveObject);
				if (emissiveError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double emissiveStrength;
				auto error = emissiveObject["emissiveStrength"].get_double().get(emissiveStrength);
				if (error == SUCCESS) [[likely]] {
					if (!toNum(emissiveStrength, material.emissiveStrength)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_ior)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_ior))
					break;

				ondemand::object iorObject;
				auto iorError = extensionField.value().get_object().get(iorObject);
				if (iorError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double ior;
				auto error = iorObject["ior"].get_double().get(ior);
				if (error == SUCCESS) [[likely]] {
					if (!toNum(ior, material.ior)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidJson;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_iridescence)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_iridescence))
					break;

				ondemand::object iridescenceObject;
				auto iridescenceError = extensionField.value().get_object().get(iridescenceObject);
				if (iridescenceError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& iridescence = material.iridescence.emplace();

				double iridescenceFactor;
				if (auto error = iridescenceObject["iridescenceFactor"].get_double().get(iridescenceFactor);
						error == SUCCESS) [[likely]] {
					if (!toNum(iridescenceFactor, iridescence.iridescenceFactor)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				TextureInfo iridescenceTexture;
				if (auto error = parseTextureInfo(iridescenceObject, "iridescenceTexture", &iridescenceTexture,
												  config.extensions); error == Error::None) [[likely]] {
					iridescence.iridescenceTexture = std::move(iridescenceTexture);
				} else if (error != Error::MissingField) {
					return error;
				}

				double iridescenceIor;
				if (auto error = iridescenceObject["iridescenceIor"].get_double().get(iridescenceIor);
					error == SUCCESS) {
					if (!toNum(iridescenceIor, iridescence.iridescenceIor)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double iridescenceThicknessMinimum;
				if (auto error = iridescenceObject["iridescenceThicknessMinimum"].get_double().get(
							iridescenceThicknessMinimum); error == SUCCESS) [[likely]] {
					if (!toNum(iridescenceThicknessMinimum, iridescence.iridescenceThicknessMinimum)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double iridescenceThicknessMaximum;
				if (auto error = iridescenceObject["iridescenceThicknessMaximum"].get_double().get(
							iridescenceThicknessMaximum); error == SUCCESS) [[likely]] {
					if (!toNum(iridescenceThicknessMaximum, iridescence.iridescenceThicknessMaximum)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				TextureInfo iridescenceThicknessTexture;
				if (auto error = parseTextureInfo(iridescenceObject, "iridescenceThicknessTexture",
												  &iridescenceThicknessTexture, config.extensions); error ==
																									Error::None) {
					iridescence.iridescenceThicknessTexture = std::move(iridescenceThicknessTexture);
				} else if (error != Error::MissingField) {
					return error;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_diffuse_transmission)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_diffuse_transmission))
					break;

				ondemand::object diffuseTransmissionObject;
				auto diffuseTransmissionError = extensionField.value().get_object().get(diffuseTransmissionObject);
				if (diffuseTransmissionError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& diffuseTransmission = material.diffuseTransmission.emplace();

				double diffuseTransmissionFactor;
				if (auto error = diffuseTransmissionObject["diffuseTransmissionFactor"].get_double().get(diffuseTransmissionFactor);
					error == SUCCESS) [[likely]] {
					if (!toNum(diffuseTransmissionFactor, diffuseTransmission.diffuseTransmissionFactor)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				TextureInfo diffuseTransmissionTexture;
				if (auto error = parseTextureInfo(diffuseTransmissionObject, "diffuseTransmissionTexture", &diffuseTransmissionTexture, config.extensions); error == Error::None) [[likely]] {
					diffuseTransmission.diffuseTransmissionTexture = std::move(diffuseTransmissionTexture);
				} else if (error != Error::MissingField) {
					return error;
				}

				ondemand::array diffuseTransmissionColorFactor;
				if (auto error = diffuseTransmissionObject["diffuseTransmissionColorFactor"].get_array().get(diffuseTransmissionColorFactor); error == SUCCESS) [[likely]] {
					std::size_t i = 0;
					for (auto factor: diffuseTransmissionColorFactor) {
						if (i >= diffuseTransmission.diffuseTransmissionColorFactor.size()) {
							return Error::InvalidGltf;
						}
						double value;
						if (factor.get_double().get(value) != SUCCESS) [[unlikely]] {
							return Error::InvalidGltf;
						}
						if (!toNum(value, diffuseTransmission.diffuseTransmissionColorFactor[i++])) [[unlikely]]
							return Error::InvalidGltf;
					}
				}

				TextureInfo diffuseTransmissionColorTexture;
				if (auto error = parseTextureInfo(diffuseTransmissionObject, "diffuseTransmissionColorTexture", &diffuseTransmissionColorTexture, config.extensions); error == Error::None) [[likely]] {
					diffuseTransmission.diffuseTransmissionColorTexture = std::move(diffuseTransmissionColorTexture);
				} else if (error != Error::MissingField) {
					return error;
				}
				break;
			}

			case force_consteval<crc32c(extensions::KHR_materials_sheen)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_sheen))
					break;

				ondemand::object sheenObject;
				auto sheenError = extensionField.value().get_object().get(sheenObject);
				if (sheenError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& sheen = material.sheen.emplace();

				ondemand::array sheenColorFactor;
				if (auto error = sheenObject["sheenColorFactor"].get_array().get(sheenColorFactor); error ==
																									SUCCESS) {
					std::size_t i = 0;
					for (auto factor: sheenColorFactor) {
						if (i >= sheen.sheenColorFactor.size()) {
							return Error::InvalidGltf;
						}
						double value;
						if (factor.get_double().get(value) != SUCCESS) [[unlikely]] {
							return Error::InvalidGltf;
						}
						if (!toNum(value, sheen.sheenColorFactor[i++])) [[unlikely]]
							return Error::InvalidGltf;
					}
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				TextureInfo sheenColorTexture;
				if (auto error = parseTextureInfo(sheenObject, "sheenColorTexture", &sheenColorTexture,
												  config.extensions); error == Error::None) [[likely]] {
					sheen.sheenColorTexture = std::move(sheenColorTexture);
				} else if (error != Error::MissingField) {
					return error;
				}

				double sheenRoughnessFactor;
				if (auto error = sheenObject["sheenRoughnessFactor"].get_double().get(sheenRoughnessFactor);
						error == SUCCESS) [[likely]] {
					if (!toNum(sheenRoughnessFactor, sheen.sheenRoughnessFactor)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				TextureInfo sheenRoughnessTexture;
				if (auto error = parseTextureInfo(sheenObject, "sheenRoughnessTexture", &sheenRoughnessTexture,
												  config.extensions); error == Error::None) [[likely]] {
					sheen.sheenRoughnessTexture = std::move(sheenRoughnessTexture);
				} else if (error != Error::MissingField) {
					return error;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_specular)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_specular))
					break;

				ondemand::object specularObject;
				auto specularError = extensionField.value().get_object().get(specularObject);
				if (specularError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& specular = material.specular.emplace();

				double specularFactor;
				if (auto error = specularObject["specularFactor"].get_double().get(specularFactor); error ==
																									SUCCESS) {
					if (!toNum(specularFactor, specular.specularFactor)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				TextureInfo specularTexture;
				if (auto error = parseTextureInfo(specularObject, "specularTexture", &specularTexture,
												  config.extensions); error == Error::None) [[likely]] {
					specular.specularTexture = std::move(specularTexture);
				} else if (error != Error::MissingField) {
					return error;
				}

				ondemand::array specularColorFactor;
				if (auto error = specularObject["specularColorFactor"].get_array().get(specularColorFactor);
						error == SUCCESS) [[likely]] {
					std::size_t i = 0;
					for (auto factor: specularColorFactor) {
						if (i >= specular.specularColorFactor.size()) {
							return Error::InvalidGltf;
						}
						double value;
						if (factor.get_double().get(value) != SUCCESS) [[unlikely]] {
							return Error::InvalidGltf;
						}
						if (!toNum(value, specular.specularColorFactor[i++])) [[unlikely]]
							return Error::InvalidGltf;
					}
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				TextureInfo specularColorTexture;
				if (auto error = parseTextureInfo(specularObject, "specularColorTexture", &specularColorTexture,
												  config.extensions); error == Error::None) [[likely]] {
					specular.specularColorTexture = std::move(specularColorTexture);
				} else if (error != Error::MissingField) {
					return error;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_transmission)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_transmission))
					break;

				ondemand::object transmissionObject;
				auto transmissionError = extensionField.value().get_object().get(transmissionObject);
				if (transmissionError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& transmission = material.transmission.emplace();

				double transmissionFactor;
				if (auto error = transmissionObject["transmissionFactor"].get_double().get(transmissionFactor);
						error == SUCCESS) [[likely]] {
					if (!toNum(transmissionFactor, transmission.transmissionFactor)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				TextureInfo transmissionTexture;
				if (auto error = parseTextureInfo(transmissionObject, "transmissionTexture", &transmissionTexture,
												  config.extensions); error == Error::None) [[likely]] {
					transmission.transmissionTexture = std::move(transmissionTexture);
				} else if (error != Error::MissingField) {
					return error;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_unlit)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_unlit))
					break;

				ondemand::object unlitObject;
				auto unlitError = extensionField.value().get_object().get(unlitObject);
				if (unlitError == SUCCESS) [[likely]] {
					material.unlit = true;
				} else {
					return Error::InvalidGltf;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_volume)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_volume))
					break;

				ondemand::object volumeObject;
				auto volumeError = extensionField.value().get_object().get(volumeObject);
				if (volumeError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& volume = material.volume.emplace();

				double thicknessFactor;
				if (auto error = volumeObject["thicknessFactor"].get_double().get(thicknessFactor); error == SUCCESS) [[likely]] {
					if (!toNum(thicknessFactor, volume.thicknessFactor)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				TextureInfo thicknessTexture;
				if (auto error = parseTextureInfo(volumeObject, "thicknessTexture", &thicknessTexture, config.extensions); error == Error::None) [[likely]] {
					volume.thicknessTexture = std::move(thicknessTexture);
				} else if (error != Error::MissingField) {
					return error;
				}

				double attenuationDistance;
				if (auto error = volumeObject["attenuationDistance"].get_double().get(attenuationDistance); error == SUCCESS) [[likely]] {
					if (!toNum(attenuationDistance, volume.attenuationDistance)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				ondemand::array attenuationColor;
				if (auto error = volumeObject["attenuationColor"].get_array().get(attenuationColor); error == SUCCESS) [[likely]] {
					std::size_t i = 0;
					for (auto factor : attenuationColor) {
						if (i >= volume.attenuationColor.size()) {
							return Error::InvalidGltf;
						}
						double value;
						if (factor.get_double().get(value) != SUCCESS) [[unlikely]] {
							return Error::InvalidGltf;
						}
						if (!toNum(value, (volume.attenuationColor)[i++])) [[unlikely]]
							return Error::InvalidGltf;
					}
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}
				break;
			}
			case force_consteval<crc32c(extensions::MSFT_packing_normalRoughnessMetallic)>: {
				if (!hasBit(config.extensions, Extensions::MSFT_packing_normalRoughnessMetallic))
					break;

				ondemand::object normalRoughnessMetallic;
				if (extensionField.value().get_object().get(normalRoughnessMetallic) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				TextureInfo textureInfo {};
				if (auto error = parseTextureInfo(normalRoughnessMetallic, "normalRoughnessMetallicTexture", &textureInfo, config.extensions); error == Error::None) [[likely]] {
					material.packedNormalMetallicRoughnessTexture = std::move(textureInfo);
				} else if (error != Error::MissingField) {
					return error;
				}
				break;
			}
			case force_consteval<crc32c(extensions::MSFT_packing_occlusionRoughnessMetallic)>: {
				if (!hasBit(config.extensions, Extensions::MSFT_packing_occlusionRoughnessMetallic))
					break;

				ondemand::object occlusionRoughnessMetallic;
				if (extensionField.value().get_object().get(occlusionRoughnessMetallic) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				auto& packedTextures = material.packedOcclusionRoughnessMetallicTextures.emplace();
				TextureInfo textureInfo {};
				if (auto error = parseTextureInfo(occlusionRoughnessMetallic, "occlusionRoughnessMetallicTexture", &textureInfo, config.extensions); error == Error::None) [[likely]] {
					packedTextures.occlusionRoughnessMetallicTexture = std::move(textureInfo);
				} else if (error != Error::MissingField) {
					return error;
				}

				if (auto error = parseTextureInfo(occlusionRoughnessMetallic, "roughnessMetallicOcclusionTexture", &textureInfo, config.extensions); error == Error::None) [[likely]] {
					packedTextures.roughnessMetallicOcclusionTexture = std::move(textureInfo);
				} else if (error != Error::MissingField) {
					return error;
				}

				if (auto error = parseTextureInfo(occlusionRoughnessMetallic, "normalTexture", &textureInfo, config.extensions); error == Error::None) [[likely]] {
					packedTextures.normalTexture = std::move(textureInfo);
				} else if (error != Error::MissingField) {
					return error;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_pbrSpecularGlossiness)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_pbrSpecularGlossiness))
					break;

				ondemand::object specularGlossinessObject;
				auto specularGlossinessError = extensionField.value().get_object().get(specularGlossinessObject);
				if (specularGlossinessError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				auto& specularGlossiness = material.specularGlossiness.emplace();

				ondemand::array diffuseFactor;
				if (auto error = specularGlossinessObject["diffuseFactor"].get_array().get(diffuseFactor); error == SUCCESS) [[likely]] {
					std::size_t i = 0;
					for (auto factor : diffuseFactor) {
						if (i >= specularGlossiness.diffuseFactor.size()) {
							return Error::InvalidGltf;
						}
						double value;
						if (factor.get_double().get(value) != SUCCESS) [[unlikely]] {
							return Error::InvalidGltf;
						}
						if (!toNum(value, specularGlossiness.diffuseFactor[i++])) [[unlikely]]
							return Error::InvalidGltf;
					}
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				TextureInfo diffuseTexture;
				if (auto error = parseTextureInfo(specularGlossinessObject, "diffuseTexture", &diffuseTexture, config.extensions); error == Error::None) [[likely]] {
					specularGlossiness.diffuseTexture = std::move(diffuseTexture);
				} else if (error != Error::MissingField) {
					return error;
				}

				ondemand::array specularFactor;
				if (auto error = specularGlossinessObject["specularFactor"].get_array().get(specularFactor); error == SUCCESS) [[likely]] {
					std::size_t i = 0;
					for (auto factor : specularFactor) {
						if (i >= specularGlossiness.specularFactor.size()) {
							return Error::InvalidGltf;
						}
						double value;
						if (factor.get_double().get(value) != SUCCESS) [[unlikely]] {
							return Error::InvalidGltf;
						}
						if (!toNum(value, specularGlossiness.specularFactor[i++])) [[unlikely]]
							return Error::InvalidGltf;
					}
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double glossinessFactor;
				if (auto error = specularGlossinessObject["glossinessFactor"].get_double().get(glossinessFactor); error == SUCCESS) [[likely]] {
					if (!toNum(glossinessFactor, specularGlossiness.glossinessFactor)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				TextureInfo specularGlossinessTexture;
				if (auto error = parseTextureInfo(specularGlossinessObject, "specularGlossinessTexture", &specularGlossinessTexture, config.extensions); error == Error::None) [[likely]] {
					specularGlossiness.specularGlossinessTexture = std::move(specularGlossinessTexture);
				} else if (error != Error::MissingField) {
					return error;
				}
				break;
			}
			default:
				// Should we error on unknown extensions?
				break;
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parseMaterials(internal::JsonArray& materialsJson, Asset& asset) {
	using namespace simdjson;
	auto& materials = materialsJson.value;

	if (auto reserveError = reserveElements(materials, asset.materials); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto materialValue : materials) {
		ondemand::object materialObject;
		if (materialValue.get_object().get(materialObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		Material material {};

		ondemand::array emissiveFactor;
		if (auto error = materialObject["emissiveFactor"].get_array().get(emissiveFactor); error == SUCCESS) [[likely]] {
			if (auto factorError = parseNumArray(emissiveFactor, material.emissiveFactor); factorError != Error::None) [[unlikely]]
				return factorError;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		{
			NormalTextureInfo normalTextureInfo {};
			if (auto error = parseTextureInfo<internal::TextureInfoType::NormalTexture>(
					materialObject, "normalTexture", &normalTextureInfo, config.extensions);
				error == Error::None) [[likely]] {
				material.normalTexture = std::move(normalTextureInfo);
			} else if (error != Error::MissingField) {
				return error;
			}
		}

		{
			OcclusionTextureInfo occlusionTextureInfo {};
			if (auto error = parseTextureInfo<internal::TextureInfoType::OcclusionTexture>(
					materialObject, "occlusionTexture", &occlusionTextureInfo, config.extensions);
				error == Error::None) [[likely]] {
				material.occlusionTexture = std::move(occlusionTextureInfo);
			} else if (error != Error::MissingField) {
				return error;
			}
		}

		{
			TextureInfo textureInfo {};
			if (auto error = parseTextureInfo(materialObject, "emissiveTexture", &textureInfo, config.extensions); error == Error::None) [[likely]] {
				material.emissiveTexture = std::move(textureInfo);
			} else if (error != Error::MissingField) {
				return error;
			}
		}

		ondemand::object pbrMetallicRoughness;
		if (materialObject["pbrMetallicRoughness"].get_object().get(pbrMetallicRoughness) == SUCCESS) [[likely]] {
			PBRData pbr {};

			ondemand::array baseColorFactor;
			if (pbrMetallicRoughness["baseColorFactor"].get_array().get(baseColorFactor) == SUCCESS) [[likely]] {
				if (auto factorError = parseNumArray(baseColorFactor, pbr.baseColorFactor); factorError != Error::None) [[unlikely]]
					return factorError;
			}

			double factor;
			if (auto error = pbrMetallicRoughness["metallicFactor"].get_double().get(factor); error == SUCCESS) [[likely]] {
				if (!toNum(factor, pbr.metallicFactor)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
			if (auto error = pbrMetallicRoughness["roughnessFactor"].get_double().get(factor); error == SUCCESS) [[likely]] {
				if (!toNum(factor, pbr.roughnessFactor)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}

			TextureInfo textureInfo;
			if (auto error = parseTextureInfo(pbrMetallicRoughness, "baseColorTexture", &textureInfo, config.extensions); error == Error::None) [[likely]] {
				pbr.baseColorTexture = std::move(textureInfo);
			} else if (error != Error::MissingField) {
				return error;
			}

			if (auto error = parseTextureInfo(pbrMetallicRoughness, "metallicRoughnessTexture", &textureInfo, config.extensions); error == Error::None) [[likely]] {
				pbr.metallicRoughnessTexture = std::move(textureInfo);
			} else if (error != Error::MissingField) {
				return error;
			}

			material.pbrData = std::move(pbr);
		}

		std::string_view alphaMode;
		if (auto error = materialObject["alphaMode"].get_string().get(alphaMode); error == SUCCESS) [[likely]] {
			if (alphaMode == "OPAQUE") {
				material.alphaMode = AlphaMode::Opaque;
			} else if (alphaMode == "MASK") {
				material.alphaMode = AlphaMode::Mask;
			} else if (alphaMode == "BLEND") {
				material.alphaMode = AlphaMode::Blend;
			} else {
				return Error::InvalidGltf;
			}
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		double alphaCutoff;
		if (auto error = materialObject["alphaCutoff"].get_double().get(alphaCutoff); error == SUCCESS) [[likely]] {
			if (!toNum(alphaCutoff, material.alphaCutoff)) [[unlikely]]
				return Error::InvalidGltf;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		bool doubleSided;
		if (auto error = materialObject["doubleSided"].get_bool().get(doubleSided); error == SUCCESS) [[likely]] {
			material.doubleSided = doubleSided;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::string_view name;
		if (materialObject["name"].get_string().get(name) == SUCCESS) {
			material.name.assign(name);
		}

		internal::JsonObject extensionsObject;
		if (auto extensionError = materialObject["extensions"].get_object().get(extensionsObject.value); extensionError == SUCCESS) {
			if (const auto err = parseMaterialExtensions(extensionsObject, material); err != Error::None)
				return err;
		} else if (extensionError != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidJson;
		}

		if (auto extrasError = parseExtras(config, materialObject, asset.materials.size(), Category::Materials); extrasError != Error::None) [[unlikely]]
			return extrasError;

		asset.materials.emplace_back(std::move(material));
	}

	return Error::None;
}

fastgltf::Error fg::Parser::parsePrimitiveExtensions(internal::JsonObject& objectJson, Primitive& primitive) {
	using namespace simdjson;
	auto& object = objectJson.value;

	for (auto extension : object) {
		std::string_view key;
		if (extension.escaped_key().get(key) != SUCCESS) [[unlikely]]
				return Error::InvalidJson;
		auto hashedKey = crc32c(key);

		switch (hashedKey) {
			case force_consteval<crc32c(extensions::KHR_materials_variants)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_variants))
					break;

				ondemand::object variantObject;
				if (auto variantsError = extension.value().get_object().get(variantObject); variantsError != SUCCESS) {
					return Error::InvalidGltf;
				}

				ondemand::array mappingsArray;
				if (variantObject["mappings"].get_array().get(mappingsArray) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				for (auto mapping : mappingsArray) {
					ondemand::object mappingObject;
					if (mapping.get_object().get(mappingObject) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}

					std::uint64_t materialIndex;
					if (mappingObject["material"].get_uint64().get(materialIndex) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}

					if (!isValidIndex(materialIndex)) [[unlikely]]
						return Error::ValueOutOfRange;

					ondemand::array variantsArray;
					if (mappingObject["variants"].get_array().get(variantsArray) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
					for (auto variant : variantsArray) {
						std::uint64_t variantIndex;
						if (variant.get_uint64().get(variantIndex) != SUCCESS) [[unlikely]] {
							return Error::InvalidGltf;
						}

						if (variantIndex >= variantCount) [[unlikely]]
							return Error::InvalidGltf;

						primitive.mappings.resize(max(primitive.mappings.size(),
													  static_cast<std::size_t>(variantIndex + 1)));
						primitive.mappings[static_cast<std::size_t>(variantIndex)] = materialIndex;
					}
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_draco_mesh_compression)>: {
				if (!hasBit(config.extensions, Extensions::KHR_draco_mesh_compression))
					break;

				ondemand::object dracoObject;
				if (auto dracoError = extension.value().get_object().get(dracoObject); dracoError != SUCCESS) {
					return Error::InvalidGltf;
				}

				auto& dracoCompression = primitive.dracoCompression.emplace();

				std::uint64_t value;
				if (auto error = dracoObject["bufferView"].get_uint64().get(value); error != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				if (!isValidIndex(value)) [[unlikely]]
					return Error::ValueOutOfRange;
				dracoCompression.bufferView = static_cast<std::size_t>(value);

				ondemand::object attributesObject;
				if (dracoObject["attributes"].get_object().get(attributesObject) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				if (auto attributesError = parseAttributes(attributesObject, dracoCompression.attributes); attributesError != Error::None) {
					return attributesError;
				}
				break;
			}
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parseMeshes(internal::JsonArray& meshesJson, Asset& asset) {
	using namespace simdjson;
	auto& meshes = meshesJson.value;

	if (auto reserveError = reserveElements(meshes, asset.meshes); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto meshValue : meshes) {
		// Required fields: "primitives"
		ondemand::object meshObject;
		if (meshValue.get_object().get(meshObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		Mesh mesh {};

		ondemand::array array;
		auto meshError = getJsonArray(meshObject, "primitives", &array);
		if (meshError != Error::None) {
			return Error::InvalidGltf;
		}

		std::size_t primitiveCount;
		if (array.count_elements().get(primitiveCount) != SUCCESS) [[unlikely]] {
			return Error::InvalidJson;
		}
		if (primitiveCount == 0) [[unlikely]] {
			return Error::InvalidGltf;
		}
		mesh.primitives.reserve(primitiveCount);
		for (auto primitiveValue : array) {
			// Required fields: "attributes"
			Primitive primitive {};
			ondemand::object primitiveObject;
			if (primitiveValue.get_object().get(primitiveObject) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			ondemand::object attributesObject;
			if (primitiveObject["attributes"].get_object().get(attributesObject) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			if (auto attributesError = parseAttributes(attributesObject, primitive.attributes); attributesError != Error::None) {
				return attributesError;
			}

			ondemand::array targets;
			if (primitiveObject["targets"].get_array().get(targets) == SUCCESS) [[likely]] {
				if (auto reserveError = reserveElements(targets, primitive.targets); reserveError != Error::None) [[unlikely]]
					return reserveError;
				for (auto targetValue : targets) {
					if (targetValue.get_object().get(attributesObject) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
					auto& map = primitive.targets.emplace_back();
					if (auto attributesError = parseAttributes(attributesObject, map); attributesError != Error::None) {
						return attributesError;
					}
				}
			}

			std::uint64_t value;
			if (auto error = primitiveObject["mode"].get_uint64().get(value); error == SUCCESS) [[likely]] {
				if (!toEnum(value, primitive.type)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			if (auto error = primitiveObject["indices"].get_uint64().get(value); error == SUCCESS) [[likely]] {
				if (!isValidIndex(value)) [[unlikely]]
					return Error::ValueOutOfRange;
				primitive.indicesAccessor = static_cast<std::size_t>(value);
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}

			if (auto error = primitiveObject["material"].get_uint64().get(value); error == SUCCESS) [[likely]] {
				if (!isValidIndex(value)) [[unlikely]]
					return Error::ValueOutOfRange;
				primitive.materialIndex = static_cast<std::size_t>(value);
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}

			if (hasBit(config.extensions, Extensions::KHR_materials_variants) || hasBit(config.extensions, Extensions::KHR_draco_mesh_compression)) {
				internal::JsonObject extensionsObject;
				if (auto error = primitiveObject["extensions"].get_object().get(extensionsObject.value); error == SUCCESS) {
					if (auto extensionError = parsePrimitiveExtensions(extensionsObject, primitive); extensionError != Error::None)
						return extensionError;
				} else if (error != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
			}

			mesh.primitives.emplace_back(std::move(primitive));
		}

		if (meshError = getJsonArray(meshObject, "weights", &array); meshError == Error::None) [[likely]] {
			if (auto reserveError = reserveElements(array, mesh.weights); reserveError != Error::None) [[unlikely]]
				return reserveError;
			for (auto weightValue : array) {
				double val;
				if (weightValue.get_double().get(val) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				if (!toNum(val, mesh.weights.emplace_back())) [[unlikely]]
					return Error::InvalidGltf;
			}
		} else if (meshError != Error::MissingField && meshError != Error::None) {
			return Error::InvalidGltf;
		}

		if (auto extrasError = parseExtras(config, meshObject, asset.meshes.size(), Category::Meshes); extrasError != Error::None) [[unlikely]]
			return extrasError;

		std::string_view name;
		if (meshObject["name"].get_string().get(name) == SUCCESS) {
			mesh.name.assign(name);
		}

		asset.meshes.emplace_back(std::move(mesh));
	}

	return Error::None;
}

fg::Error fg::Parser::parseNodes(internal::JsonArray& nodesJson, Asset& asset) {
	using namespace simdjson;
	auto& nodes = nodesJson.value;

	if (auto reserveError = reserveElements(nodes, asset.nodes); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto nodeValue : nodes) {
		Node node {};
		ondemand::object nodeObject;
		if (nodeValue.get_object().get(nodeObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t index;
		if (auto error = nodeObject["mesh"].get_uint64().get(index); error == SUCCESS) {
			if (!isValidIndex(index)) [[unlikely]]
				return Error::ValueOutOfRange;
			node.meshIndex = static_cast<std::size_t>(index);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}
		if (auto error = nodeObject["skin"].get_uint64().get(index); error == SUCCESS) {
			if (!isValidIndex(index)) [[unlikely]]
				return Error::ValueOutOfRange;
			node.skinIndex = static_cast<std::size_t>(index);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}
		if (auto error = nodeObject["camera"].get_uint64().get(index); error == SUCCESS) {
			if (!isValidIndex(index)) [[unlikely]]
				return Error::ValueOutOfRange;
			node.cameraIndex = static_cast<std::size_t>(index);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::array array;
		auto childError = getJsonArray(nodeObject, "children", &array);
		if (childError == Error::None) [[likely]] {
			if (auto reserveError = reserveElements(array, node.children); reserveError != Error::None) [[unlikely]]
				return reserveError;
			for (auto childValue : array) {
				if (childValue.get_uint64().get(index) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				if (!isValidIndex(index)) [[unlikely]]
					return Error::ValueOutOfRange;
				node.children.emplace_back(static_cast<std::size_t>(index));
			}
		} else if (childError != Error::MissingField) {
			return childError;
		}

		auto weightsError = getJsonArray(nodeObject, "weights", &array);
		if (weightsError != Error::MissingField) {
			if (weightsError == Error::None) {
				if (auto reserveError = reserveElements(array, node.weights); reserveError != Error::None) [[unlikely]]
					return reserveError;
				for (auto weightValue : array) {
					double val;
					if (weightValue.get_double().get(val) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
					if (!toNum(val, node.weights.emplace_back())) [[unlikely]]
						return Error::InvalidGltf;
				}
			} else {
				return Error::InvalidGltf;
			}
		}

		auto error = nodeObject["matrix"].get_array().get(array);
		if (error == SUCCESS) [[likely]] {
			math::nmat4x4 transformMatrix;
			std::size_t i = 0, j = 0;
			for (auto num : array) {
				double val;
				if (i == 4 || num.get_double().get(val) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				if (!toNum(val, transformMatrix.col(i)[j++])) [[unlikely]]
					return Error::InvalidGltf;
				if (j == 4) {
					j = 0;
					++i;
				}
			}
			if (i != 4) [[unlikely]] {
				return Error::InvalidGltf;
			}

			if (hasBit(options, Options::DecomposeNodeMatrices)) {
				TRS trs {};
				math::decomposeTransformMatrix(transformMatrix, trs.scale, trs.rotation, trs.translation);
				node.transform = trs;
			} else {
				node.transform = transformMatrix;
			}
		} else if (error == NO_SUCH_FIELD) {
			TRS trs {};

			// There's no matrix, let's see if there's scale, rotation, or rotation fields.
			if (auto scaleError = nodeObject["scale"].get_array().get(array); scaleError == SUCCESS) [[likely]] {
				if (auto arrayError = parseNumArray(array, trs.scale); arrayError != Error::None) [[unlikely]]
					return arrayError;
			} else if (scaleError != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidJson;
			}

			if (auto translationError = nodeObject["translation"].get_array().get(array); translationError == SUCCESS) [[likely]] {
				if (auto arrayError = parseNumArray(array, trs.translation); arrayError != Error::None) [[unlikely]]
					return arrayError;
			} else if (translationError != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			if (auto rotationError = nodeObject["rotation"].get_array().get(array); rotationError == SUCCESS) [[likely]] {
				if (auto arrayError = parseNumArray(array, trs.rotation); arrayError != Error::None) [[unlikely]]
					return arrayError;
			} else if (rotationError != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			node.transform = trs;
		}

		ondemand::object extensionsObject;
		if (nodeObject["extensions"].get_object().get(extensionsObject) == SUCCESS) [[likely]] {
			if (hasBit(config.extensions, Extensions::KHR_lights_punctual)) {
				ondemand::object lightsObject;
				if (const auto lightsError = extensionsObject[extensions::KHR_lights_punctual].get_object().get(lightsObject); lightsError == SUCCESS) [[likely]] {
					std::uint64_t light;
					if (auto lightError = lightsObject["light"].get_uint64().get(light); lightError == SUCCESS) [[likely]] {
						if (!isValidIndex(light)) [[unlikely]]
							return Error::ValueOutOfRange;
						node.lightIndex = static_cast<std::size_t>(light);
					} else {
						return lightError == NO_SUCH_FIELD || lightError == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
					}
				} else if (lightsError != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
			}

			if (hasBit(config.extensions, Extensions::EXT_mesh_gpu_instancing)) {
				ondemand::object gpuInstancingObject;
				if (auto instancingError = extensionsObject[extensions::EXT_mesh_gpu_instancing].get_object().get(gpuInstancingObject); instancingError == SUCCESS) [[likely]] {
					ondemand::object attributesObject;

					if (gpuInstancingObject["attributes"].get_object().get(attributesObject) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}

					if (auto attributesError = parseAttributes(attributesObject, node.instancingAttributes); attributesError != Error::None) {
						return attributesError;
					}
				} else if (instancingError != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
			}

			if (hasBit(config.extensions, Extensions::KHR_physics_rigid_bodies)) {
				internal::JsonObject physicsRigidBodiesObject;
				if (auto rigidBodiesError = extensionsObject[extensions::KHR_physics_rigid_bodies].get_object().get(physicsRigidBodiesObject.value); rigidBodiesError == SUCCESS) [[likely]] {
					const auto rigidBodyError = parsePhysicsRigidBody(physicsRigidBodiesObject, node);
					if (rigidBodyError != Error::None) {
						return rigidBodyError;
					}
				} else if (rigidBodiesError != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
			}

			if (hasBit(config.extensions, Extensions::KHR_node_visibility)) {
				ondemand::object nodeVisibilityObject;
				if (auto nodeVisibilityError = extensionsObject[extensions::KHR_node_visibility].get_object().get(nodeVisibilityObject); nodeVisibilityError == SUCCESS) {
					if (const auto visibleError = nodeVisibilityObject["visible"].get_bool().get(node.visible);
						visibleError != NO_SUCH_FIELD && visibleError != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
				} else if (nodeVisibilityError != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
			}

			if (hasBit(config.extensions, Extensions::KHR_node_selectability)) {
				ondemand::object nodeSelectabilityObject;
				if (auto nodeSelectabilityError = extensionsObject[extensions::KHR_node_selectability].get_object().get(nodeSelectabilityObject); nodeSelectabilityError == SUCCESS) {
					if (const auto selectableError = nodeSelectabilityObject["selectable"].get_bool().get(node.selectable);
						selectableError != NO_SUCH_FIELD && selectableError != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
				} else if (nodeSelectabilityError != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
			}

			if (hasBit(config.extensions, Extensions::KHR_node_hoverability)) {
				ondemand::object nodeHoverabilityObject;
				if (auto nodeHoverabilityError = extensionsObject[extensions::KHR_node_hoverability].get_object().get(nodeHoverabilityObject); nodeHoverabilityError == SUCCESS) {
					if (const auto hoverableError = nodeHoverabilityObject["hoverable"].get_bool().get(node.hoverable);
						hoverableError != NO_SUCH_FIELD && hoverableError != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
				} else if (nodeHoverabilityError != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
			}
		}

		if (auto extrasError = parseExtras(config, nodeObject, asset.nodes.size(), Category::Nodes); extrasError != Error::None) [[unlikely]]
			return extrasError;

		std::string_view name;
		if (nodeObject["name"].get_string().get(name) == SUCCESS) {
			node.name.assign(name);
		}

		asset.nodes.emplace_back(std::move(node));
	}

	return Error::None;
}

fg::Error fg::Parser::parseSamplers(internal::JsonArray& samplersJson, Asset& asset) {
	using namespace simdjson;
	auto& samplers = samplersJson.value;

	std::uint64_t number;
	if (auto reserveError = reserveElements(samplers, asset.samplers); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto samplerValue : samplers) {
		Sampler sampler {};
		ondemand::object samplerObject;
		if (samplerValue.get_object().get(samplerObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (auto error = samplerObject["magFilter"].get_uint64().get(number); error == SUCCESS) [[likely]] {
			if (!toEnum<Filter>(number, sampler.magFilter)) [[unlikely]]
				return Error::InvalidGltf;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}
		if (auto error = samplerObject["minFilter"].get_uint64().get(number); error == SUCCESS) [[likely]] {
			if (!toEnum<Filter>(number, sampler.minFilter)) [[unlikely]]
				return Error::InvalidGltf;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (auto error = samplerObject["wrapS"].get_uint64().get(number); error == SUCCESS) [[likely]] {
			if (!toEnum<Wrap>(number, sampler.wrapS)) [[unlikely]]
				return Error::InvalidGltf;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}
		if (auto error = samplerObject["wrapT"].get_uint64().get(number); error == SUCCESS) [[likely]] {
			if (!toEnum<Wrap>(number, sampler.wrapT)) [[unlikely]]
				return Error::InvalidGltf;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (auto extrasError = parseExtras(config, samplerObject, asset.samplers.size(), Category::Samplers); extrasError != Error::None) [[unlikely]]
			return extrasError;

		std::string_view name;
		if (samplerObject["name"].get_string().get(name) == SUCCESS) {
			sampler.name.assign(name);
		}

		asset.samplers.emplace_back(std::move(sampler));
	}

	return Error::None;
}

fg::Error fg::Parser::parseScenes(internal::JsonArray& scenesJson, Asset& asset) {
	using namespace simdjson;
	auto& scenes = scenesJson.value;

	if (auto reserveError = reserveElements(scenes, asset.scenes); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto sceneValue : scenes) {
		// The scene object can be completely empty
		Scene scene {};
		ondemand::object sceneObject;
		if (sceneValue.get_object().get(sceneObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::string_view name;
		if (sceneObject["name"].get_string().get(name) == SUCCESS) {
			scene.name.assign(name);
		}

		if (auto extrasError = parseExtras(config, sceneObject, asset.scenes.size(), Category::Scenes); extrasError != Error::None) [[unlikely]]
			return extrasError;

		// Parse the array of nodes.
		ondemand::array nodes;
		auto nodeError = getJsonArray(sceneObject, "nodes", &nodes);
		if (nodeError == Error::None) [[likely]] {
			if (auto reserveError = reserveElements(nodes, scene.nodeIndices); reserveError != Error::None) [[unlikely]]
				return reserveError;
			for (auto nodeValue : nodes) {
				std::uint64_t index;
				if (nodeValue.get_uint64().get(index) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				if (!isValidIndex(index)) [[unlikely]]
					return Error::ValueOutOfRange;
				scene.nodeIndices.emplace_back(static_cast<std::size_t>(index));
			}
		} else if (nodeError != Error::MissingField) {
			return nodeError;
		}

		asset.scenes.emplace_back(std::move(scene));
	}

	return Error::None;
}

fg::Error fg::Parser::parseSkins(internal::JsonArray& skinsJson, Asset& asset) {
	using namespace simdjson;
	auto& skins = skinsJson.value;

	if (auto reserveError = reserveElements(skins, asset.skins); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto skinValue : skins) {
		Skin skin {};
		ondemand::object skinObject;
		if (skinValue.get_object().get(skinObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t index;
		if (auto error = skinObject["inverseBindMatrices"].get_uint64().get(index); error == SUCCESS) [[likely]] {
			if (!isValidIndex(index)) [[unlikely]]
				return Error::ValueOutOfRange;
			skin.inverseBindMatrices = static_cast<std::size_t>(index);
		} else if (error != NO_SUCH_FIELD) {
			return Error::InvalidGltf;
		}
		if (auto error = skinObject["skeleton"].get_uint64().get(index); error == SUCCESS) [[likely]] {
			if (!isValidIndex(index)) [[unlikely]]
				return Error::ValueOutOfRange;
			skin.skeleton = static_cast<std::size_t>(index);
		} else if (error != NO_SUCH_FIELD) {
			return Error::InvalidGltf;
		}

		ondemand::array jointsArray;
		if (skinObject["joints"].get_array().get(jointsArray) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		if (auto reserveError = reserveElements(jointsArray, skin.joints); reserveError != Error::None) [[unlikely]]
			return reserveError;
		for (auto jointValue : jointsArray) {
			if (jointValue.get_uint64().get(index) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			if (!isValidIndex(index)) [[unlikely]]
				return Error::ValueOutOfRange;
			skin.joints.emplace_back(index);
		}

		if (auto extrasError = parseExtras(config, skinObject, asset.skins.size(), Category::Skins); extrasError != Error::None) [[unlikely]]
			return extrasError;

		std::string_view name;
		if (skinObject["name"].get_string().get(name) == SUCCESS) {
			skin.name.assign(name);
		}
		asset.skins.emplace_back(std::move(skin));
	}

	return Error::None;
}

fg::Error fg::Parser::parseTextures(internal::JsonArray& texturesJson, Asset& asset) {
	using namespace simdjson;
	auto& textures = texturesJson.value;

	if (auto reserveError = reserveElements(textures, asset.textures); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto textureValue : textures) {
		Texture texture {};
		ondemand::object textureObject;
		if (textureValue.get_object().get(textureObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t sourceIndex;
		if (auto error = textureObject["source"].get_uint64().get(sourceIndex); error == SUCCESS) [[likely]] {
			if (!isValidIndex(sourceIndex)) [[unlikely]]
				return Error::ValueOutOfRange;
			texture.imageIndex = static_cast<std::size_t>(sourceIndex);
		} else if (error != NO_SUCH_FIELD) {
			return Error::InvalidGltf;
		}

		ondemand::object extensionsObject;
		if (auto error = textureObject["extensions"].get_object().get(extensionsObject); error == SUCCESS) [[likely]] {
			if (auto extensionError = parseTextureExtensions(texture, extensionsObject, config.extensions); extensionError != Error::None) {
				return extensionError;
			}
		} else if (error != NO_SUCH_FIELD) {
			return Error::InvalidGltf;
		}

		// The index of the sampler used by this texture. When undefined, a sampler with
		// repeat wrapping and auto filtering SHOULD be used.
		std::uint64_t samplerIndex;
		if (auto error = textureObject["sampler"].get_uint64().get(samplerIndex); error == SUCCESS) [[likely]] {
			if (!isValidIndex(samplerIndex)) [[unlikely]]
				return Error::ValueOutOfRange;
			texture.samplerIndex = static_cast<std::size_t>(samplerIndex);
		} else if (error != NO_SUCH_FIELD) {
			return Error::InvalidGltf;
		}

		if (auto extrasError = parseExtras(config, textureObject, asset.textures.size(), Category::Textures); extrasError != Error::None) [[unlikely]]
			return extrasError;

		std::string_view name;
		if (textureObject["name"].get_string().get(name) == SUCCESS) {
			texture.name.assign(name);
		}

		asset.textures.emplace_back(std::move(texture));
	}

	return Error::None;
}

fg::Error fg::Parser::parseShapes(internal::JsonArray& shapesJson, Asset& asset) {
	using namespace simdjson;
	auto& shapes = shapesJson.value;

	if (auto reserveError = reserveElements(shapes, asset.shapes); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto shapeValue : shapes) {
		auto& shape = asset.shapes.emplace_back();

		ondemand::object shapeObject;
		if (shapeValue.get_object().get(shapeObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::string_view shapeTypeName;
		if (shapeObject["type"].get_string().get(shapeTypeName) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::object sphereObject;
		if (auto error = shapeObject["sphere"].get_object().get(sphereObject); error == SUCCESS) {
			if (shapeTypeName != "sphere") [[unlikely]] {
				return Error::InvalidGltf;
			}

			auto& sphere = shape.data.emplace<SphereShape>();

			double radius;
			if (error = sphereObject["radius"].get_double().get(radius); error == SUCCESS) {
				if (!toNum(radius, sphere.radius)) [[unlikely]]
					return Error::InvalidGltf;
			} else if(error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		} else if(error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::object boxObject;
		if (auto error = shapeObject["box"].get_object().get(boxObject); error == SUCCESS) {
			if (shapeTypeName != "box") [[unlikely]] {
				return Error::InvalidGltf;
			}

			auto& box = shape.data.emplace<BoxShape>();

			ondemand::array sizeArray;
			if (error = boxObject["size"].get_array().get(sizeArray); error == SUCCESS) {
				if (auto arrayError = parseNumArray(sizeArray, box.size); arrayError != Error::None) [[unlikely]]
					return arrayError;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::object capsuleObject;
		if (auto error = shapeObject["capsule"].get_object().get(capsuleObject); error == SUCCESS) {
			if (shapeTypeName != "capsule") [[unlikely]] {
				return Error::InvalidGltf;
			}

			auto& capsule = shape.data.emplace<CapsuleShape>();

			double height;
			if (error = capsuleObject["height"].get_double().get(height); error == SUCCESS) {
				if (!toNum(height, capsule.height)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			double radiusBottom;
			if (error = capsuleObject["radiusBottom"].get_double().get(radiusBottom); error == SUCCESS) {
				if (!toNum(radiusBottom, capsule.radiusBottom)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			double radiusTop;
			if (error = capsuleObject["radiusTop"].get_double().get(radiusTop); error == SUCCESS) {
				if (!toNum(radiusTop, capsule.radiusTop)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::object cylinderObject;
		if (auto error = shapeObject["cylinder"].get_object().get(cylinderObject); error == SUCCESS) {
			if (shapeTypeName != "cylinder") [[unlikely]] {
				return Error::InvalidGltf;
			}

			auto& cylinder = shape.data.emplace<CylinderShape>();

			double height;
			if (error = cylinderObject["height"].get_double().get(height); error == SUCCESS) {
				if (!toNum(height, cylinder.height)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			double radiusBottom;
			if (error = cylinderObject["radiusBottom"].get_double().get(radiusBottom); error == SUCCESS) {
				if (!toNum(radiusBottom, cylinder.radiusBottom)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			double radiusTop;
			if (error = cylinderObject["radiusTop"].get_double().get(radiusTop); error == SUCCESS) {
				if (!toNum(radiusTop, cylinder.radiusTop)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::object planeObject;
		if (auto error = shapeObject["plane"].get_object().get(planeObject); error == SUCCESS) {
			if (shapeTypeName != "plane") [[unlikely]] {
				return Error::InvalidGltf;
			}

			auto& plane = shape.data.emplace<PlaneShape>();

			double sizeX;
			if (error = planeObject["sizeX"].get_double().get(sizeX); error == SUCCESS) {
				if (!toNum(sizeX, plane.sizeX)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			double sizeZ;
			if (error = planeObject["sizeZ"].get_double().get(sizeZ); error == SUCCESS) {
				if (!toNum(sizeZ, plane.sizeZ)) [[unlikely]]
					return Error::InvalidGltf;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			bool doubleSided;
			if (error = planeObject["doubleSided"].get_bool().get(doubleSided); error == SUCCESS) {
				plane.doubleSided = doubleSided;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (auto extrasError = parseExtras(config, shapeObject, asset.shapes.size() - 1, Category::Shapes); extrasError != Error::None) [[unlikely]]
			return extrasError;

		std::string_view name;
		if (shapeObject["name"].get_string().get(name) == SUCCESS) {
			shape.name.assign(name);
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parsePhysicsMaterials(internal::JsonArray& physicsMaterialsJson, Asset& asset) {
	using namespace simdjson;
	auto& physicsMaterials = physicsMaterialsJson.value;

	if (auto reserveError = reserveElements(physicsMaterials, asset.physicsMaterials); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto materialValue : physicsMaterials) {
		PhysicsMaterial& material = asset.physicsMaterials.emplace_back();
		ondemand::object materialObject;
		if (materialValue.get_object().get(materialObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		double staticFriction;
		if (auto error = materialObject["staticFriction"].get_double().get(staticFriction); error == SUCCESS) {
			if (!toNum(staticFriction, material.staticFriction)) [[unlikely]]
				return Error::InvalidGltf;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		double dynamicFriction;
		if (auto error = materialObject["dynamicFriction"].get_double().get(dynamicFriction); error == SUCCESS) {
			if (!toNum(dynamicFriction, material.dynamicFriction)) [[unlikely]]
				return Error::InvalidGltf;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		double restitution;
		if (auto error = materialObject["restitution"].get_double().get(restitution); error == SUCCESS) {
			if (!toNum(restitution, material.restitution)) [[unlikely]]
				return Error::InvalidGltf;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (auto error = materialObject["frictionCombine"].get(material.frictionCombine); error == SUCCESS) {
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (auto error = materialObject["restitutionCombine"].get(material.restitutionCombine); error == SUCCESS) {
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (auto extrasError = parseExtras(config, materialObject, asset.physicsMaterials.size() - 1, Category::PhysicsMaterials); extrasError != Error::None) [[unlikely]]
			return extrasError;
	}

	return Error::None;
}

fg::Error fg::Parser::parseCollisionFilters(internal::JsonArray& collisionFiltersJson, Asset& asset) {
	using namespace simdjson;
	auto& collisionFilters = collisionFiltersJson.value;

	if (auto reserveError = reserveElements(collisionFilters, asset.collisionFilters); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto collisionFilterValue : collisionFilters) {
		auto& collisionFilter = asset.collisionFilters.emplace_back();

		ondemand::object collisionFilterObject;
		if (collisionFilterValue.get_object().get(collisionFilterObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::array collisionSystems;
		if (auto error = collisionFilterObject["collisionSystems"].get_array().get(collisionSystems); error == SUCCESS) {
			if (auto reserveError = reserveElements(collisionSystems, collisionFilter.collisionSystems); reserveError != Error::None) [[unlikely]]
				return reserveError;
			for (auto systemNameValue : collisionSystems) {
				std::string_view systemName;
				if (systemNameValue.get_string().get(systemName) == SUCCESS) [[likely]] {
					collisionFilter.collisionSystems.emplace_back(systemName);
				} else {
					return Error::InvalidGltf;
				}
			}
		} else if (error != NO_SUCH_FIELD) {
			return Error::InvalidGltf;
		}

		// A glTF isn't allowed to have both
		if (collisionFilterObject["collideWithSystems"].error() == SUCCESS && collisionFilterObject["notCollideWithSystems"].error() == SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::array collideWithSystems;
		if (auto error = collisionFilterObject["collideWithSystems"].get_array().get(collideWithSystems); error == SUCCESS) {
			if (auto reserveError = reserveElements(collideWithSystems, collisionFilter.collideWithSystems); reserveError != Error::None) [[unlikely]]
				return reserveError;
			for (auto systemNameValue : collideWithSystems) {
				std::string_view systemName;
				if (systemNameValue.get_string().get(systemName) == SUCCESS) [[likely]] {
					collisionFilter.collideWithSystems.emplace_back(systemName);
				} else {
					return Error::InvalidGltf;
				}
			}
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::array notCollideWithSystems;
		if (auto error = collisionFilterObject["notCollideWithSystems"].get_array().get(notCollideWithSystems); error == SUCCESS) {
			if (auto reserveError = reserveElements(notCollideWithSystems, collisionFilter.notCollideWithSystems); reserveError != Error::None) [[unlikely]]
				return reserveError;
			for (auto systemNameValue : notCollideWithSystems) {
				std::string_view systemName;
				if (systemNameValue.get_string().get(systemName) == SUCCESS) [[likely]] {
					collisionFilter.notCollideWithSystems.emplace_back(systemName);
				} else {
					return Error::InvalidGltf;
				}
			}
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (auto extrasError = parseExtras(config, collisionFilterObject, asset.collisionFilters.size() - 1, Category::CollisionFilters); extrasError != Error::None) [[unlikely]]
			return extrasError;
	}

	return Error::None;
}

fg::Error fg::Parser::parsePhysicsJoints(internal::JsonArray& physicsJointsJson, Asset& asset) {
	using namespace simdjson;
	auto& physicsJoints = physicsJointsJson.value;

	if (auto reserveError = reserveElements(physicsJoints, asset.physicsJoints); reserveError != Error::None) [[unlikely]]
		return reserveError;
	for (auto physicsJointValue : physicsJoints) {
		auto& physicsJoint = asset.physicsJoints.emplace_back();

		ondemand::object physicsJointObject;
		if (physicsJointValue.get_object().get(physicsJointObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::array limitsArray;
		if (physicsJointObject["limits"].get_array().get(limitsArray) == SUCCESS) {
			if (auto reserveError = reserveElements(limitsArray, physicsJoint.limits); reserveError != Error::None) [[unlikely]]
				return reserveError;
			for (auto limitValue : limitsArray) {
				auto& limit = physicsJoint.limits.emplace_back();
				ondemand::object limitObject;
				if (limitValue.get_object().get(limitObject) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				if (limitObject["linearAxes"].error() == SUCCESS && limitObject["angularAxes"].error() == SUCCESS) [[unlikely]] {
					// A limit may only have one of these
					return Error::InvalidGltf;
				}

				if (limitObject["linearAxes"].error() != SUCCESS && limitObject["angularAxes"].error() != SUCCESS) [[unlikely]] {
					// A limit MUST have one of these
					return Error::InvalidGltf;
				}

				double limitMin;
				if (auto error = limitObject["min"].get_double().get(limitMin); error == SUCCESS) {
					if (!toNum(limitMin, limit.min)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double limitMax;
				if (auto error = limitObject["max"].get_double().get(limitMax); error == SUCCESS) {
					if (!toNum(limitMax, limit.max)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double stiffness;
				if (auto error = limitObject["stiffness"].get_double().get(stiffness); error == SUCCESS) {
					if (!toNum(stiffness, limit.stiffness)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double damping;
				if (auto error = limitObject["damping"].get_double().get(damping); error == SUCCESS) {
					if (!toNum(damping, limit.damping)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				ondemand::array linearAxesArray;
				if (auto error = limitObject["linearAxes"].get_array().get(linearAxesArray); error == SUCCESS) {
					std::size_t axisCount;
					if (linearAxesArray.count_elements().get(axisCount) != SUCCESS) [[unlikely]]
						return Error::InvalidJson;
					if (axisCount == 0 || axisCount > 3)
						return Error::InvalidGltf;

					for (auto axisValue : linearAxesArray) {
						if (limit.linearAxes.size() == limit.linearAxes.max_size()) [[unlikely]]
							return Error::InvalidGltf;

						std::uint64_t axis;
						if (axisValue.get_uint64().get(axis) == SUCCESS && axis <= 2) [[likely]] {
							limit.linearAxes.emplace_back(static_cast<uint8_t>(axis));
						} else {
							return Error::InvalidGltf;
						}
					}
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				ondemand::array angularAxesArray;
				if (auto error = limitObject["angularAxes"].get_array().get(angularAxesArray); error == SUCCESS) {
					std::size_t axisCount;
					if (angularAxesArray.count_elements().get(axisCount) != SUCCESS) [[unlikely]]
						return Error::InvalidJson;
					if (axisCount == 0 || axisCount > 3)
						return Error::InvalidGltf;

					for (auto axisValue : angularAxesArray) {
						if (limit.angularAxes.size() == limit.angularAxes.max_size()) [[unlikely]]
							return Error::InvalidGltf;

						std::uint64_t axis;
						if (axisValue.get_uint64().get(axis) == SUCCESS && axis <= 2) [[likely]] {
							limit.angularAxes.emplace_back(static_cast<uint8_t>(axis));
						} else {
							return Error::InvalidGltf;
						}
					}
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}
			}
		}

		ondemand::array driveArray;
		if (auto error = physicsJointObject["drives"].get_array().get(driveArray); error == SUCCESS) {
			if (auto reserveError = reserveElements(driveArray, physicsJoint.drives); reserveError != Error::None) [[unlikely]]
				return reserveError;
			for (auto driveValue : driveArray) {
				auto& drive = physicsJoint.drives.emplace_back();
				ondemand::object driveObject;
				if (driveValue.get_object().get(driveObject) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				// Type, mode, and axis are required

				if (driveObject["type"].get(drive.type) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				if (driveObject["mode"].get(drive.mode) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				uint64_t axis;
				if (driveObject["axis"].get_uint64().get(axis) == SUCCESS) [[likely]] {
					if (axis < 3) {
						drive.axis = static_cast<uint8_t>(axis);
					} else [[unlikely]] {
						return Error::InvalidGltf;
					}
				} else {
					return Error::InvalidGltf;
				}

				// Other fields are not required

				double maxForce;
				if (error = driveObject["maxForce"].get_double().get(maxForce); error == SUCCESS) {
					if (!toNum(maxForce, drive.maxForce)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				// If the drive has positionTarget, it must also have stiffness
				const auto hasPositionTarget = driveObject["positionTarget"].error() == SUCCESS;
				const auto hasStiffness = driveObject["stiffness"].error() == SUCCESS;
				if (hasPositionTarget != hasStiffness) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double positionTarget;
				if (error = driveObject["positionTarget"].get_double().get(positionTarget); error == SUCCESS) {
					if (!toNum(positionTarget, drive.positionTarget)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double stiffness;
				if (error = driveObject["stiffness"].get_double().get(stiffness); error == SUCCESS) {
					if (!toNum(stiffness, drive.stiffness)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				// If the drive has velocityTarget, it must also have damping
				const auto hasVelocityTarget = driveObject["velocityTarget"].error() == SUCCESS;
				const auto hasDamping = driveObject["damping"].error() == SUCCESS;
				if (hasVelocityTarget != hasDamping) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double velocityTarget;
				if (error = driveObject["velocityTarget"].get_double().get(velocityTarget); error == SUCCESS) {
					if (!toNum(velocityTarget, drive.velocityTarget)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double damping;
				if (error = driveObject["damping"].get_double().get(damping); error == SUCCESS) {
					if (!toNum(damping, drive.damping)) [[unlikely]]
						return Error::InvalidGltf;
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}
			}
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (auto extrasError = parseExtras(config, physicsJointObject, asset.physicsJoints.size() - 1, Category::PhysicsJoints); extrasError != Error::None) [[unlikely]]
			return extrasError;
	}

	return Error::None;
}

fg::Error fg::Parser::parsePhysicsRigidBody(internal::JsonObject& khr_physics_rigid_bodiesJson, Node& node) {
	using namespace simdjson;
	auto& khr_physics_rigid_bodies = khr_physics_rigid_bodiesJson.value;

	auto& rigidBody = node.physicsRigidBody.emplace();

	ondemand::object motionObject;
	if (auto error = khr_physics_rigid_bodies["motion"].get_object().get(motionObject); error == SUCCESS) {
		auto& motion = rigidBody.motion.emplace();

		bool isKinematic;
		if (error = motionObject["isKinematic"].get_bool().get(isKinematic); error == SUCCESS) {
			motion.isKinematic = isKinematic;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		double mass;
		if (error = motionObject["mass"].get_double().get(mass); error == SUCCESS) {
			if (!toNum(mass, motion.mass)) [[unlikely]]
				return Error::InvalidGltf;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::array centerOfMassArray;
		if (error = motionObject["centerOfMass"].get_array().get(centerOfMassArray); error == SUCCESS) {
			if (auto arrayError = parseNumArray(centerOfMassArray, motion.centerOfMass); arrayError != Error::None) [[unlikely]]
				return arrayError;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::array inertiaDiagonalArray;
		if (error = motionObject["inertiaDiagonal"].get_array().get(inertiaDiagonalArray); error == SUCCESS) {
			math::nvec3 inertialDiagonal;
			if (auto arrayError = parseNumArray(inertiaDiagonalArray, inertialDiagonal); arrayError != Error::None) [[unlikely]]
				return arrayError;

			motion.inertialDiagonal = inertialDiagonal;

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::array inertialOrientationArray;
		if (error = motionObject["inertiaOrientation"].get_array().get(inertialOrientationArray); error == SUCCESS) {
			math::nvec4 inertialOrientation;
			if (auto arrayError = parseNumArray(inertialOrientationArray, inertialOrientation); arrayError != Error::None) [[unlikely]]
				return arrayError;

			motion.inertialOrientation = inertialOrientation;

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::array linearVelocityArray;
		if (error = motionObject["linearVelocity"].get_array().get(linearVelocityArray); error == SUCCESS) {
			if (auto arrayError = parseNumArray(linearVelocityArray, motion.linearVelocity); arrayError != Error::None) [[unlikely]]
				return arrayError;

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::array angularVelocityArray;
		if (error = motionObject["angularVelocity"].get_array().get(angularVelocityArray); error == SUCCESS) {
			if (auto arrayError = parseNumArray(angularVelocityArray, motion.angularVelocity); arrayError != Error::None) [[unlikely]]
				return arrayError;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		double gravityFactor;
		if (error = motionObject["gravityFactor"].get_double().get(gravityFactor); error == SUCCESS) {
			if (!toNum(gravityFactor, motion.gravityFactor)) [[unlikely]]
				return Error::InvalidGltf;
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

	} else if (error != NO_SUCH_FIELD) [[unlikely]] {
		return Error::InvalidGltf;
	}

	ondemand::object colliderObject;
	if (auto error = khr_physics_rigid_bodies["collider"].get_object().get(colliderObject); error == SUCCESS) {
		auto& collider = rigidBody.collider.emplace();

		ondemand::object geometryObject;
		if (colliderObject["geometry"].get_object().get(geometryObject) == SUCCESS) [[likely]] {
			std::uint64_t shape;
			if (error = geometryObject["shape"].get_uint64().get(shape); error == SUCCESS) [[likely]] {
				if (!isValidIndex(shape)) [[unlikely]]
					return Error::ValueOutOfRange;
				collider.geometry.shape = static_cast<std::size_t>(shape);
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
			std::uint64_t geometryMesh;
			if (error = geometryObject["mesh"].get_uint64().get(geometryMesh); error == SUCCESS) {
				if (!isValidIndex(geometryMesh)) [[unlikely]]
					return Error::ValueOutOfRange;
				collider.geometry.mesh = static_cast<std::size_t>(geometryMesh);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
			bool convexHull;
			if (error = geometryObject["convexHull"].get_bool().get(convexHull); error == SUCCESS) {
				collider.geometry.convexHull = convexHull;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		} else {
			return Error::InvalidGltf;
		}

		std::uint64_t physicsMaterial;
		if (error = colliderObject["physicsMaterial"].get_uint64().get(physicsMaterial); error == SUCCESS) {
			if (!isValidIndex(physicsMaterial)) [[unlikely]]
				return Error::ValueOutOfRange;
			collider.physicsMaterial = static_cast<std::size_t>(physicsMaterial);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t collisionFilter;
		if (error = colliderObject["collisionFilter"].get_uint64().get(collisionFilter); error == SUCCESS) {
			if (!isValidIndex(collisionFilter)) [[unlikely]]
				return Error::ValueOutOfRange;
			collider.collisionFilter = static_cast<std::size_t>(collisionFilter);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

	} else if (error != NO_SUCH_FIELD) [[unlikely]] {
		return Error::InvalidGltf;
	}

	ondemand::object triggerObject;
	if (auto error = khr_physics_rigid_bodies["trigger"].get_object().get(triggerObject); error == SUCCESS) {
		ondemand::object geometryObject;
		if (error = triggerObject["geometry"].get_object().get(geometryObject); error == SUCCESS) {
			auto& geometryTrigger = rigidBody.trigger.emplace().emplace<GeometryTrigger>();

			std::uint64_t shape;
			if (error = geometryObject["shape"].get_uint64().get(shape); error == SUCCESS) [[likely]] {
				if (!isValidIndex(shape)) [[unlikely]]
					return Error::ValueOutOfRange;
				geometryTrigger.geometry.shape = static_cast<std::size_t>(shape);
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
			std::uint64_t meshIndex;
			if (error = geometryObject["mesh"].get_uint64().get(meshIndex); error == SUCCESS) {
				if (!isValidIndex(meshIndex)) [[unlikely]]
					return Error::ValueOutOfRange;
				geometryTrigger.geometry.mesh = static_cast<std::size_t>(meshIndex);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
			bool convexHull;
			if (error = geometryObject["convexHull"].get_bool().get(convexHull); error == SUCCESS) {
				geometryTrigger.geometry.convexHull = convexHull;
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			// The trigger can only have a collision filter if it has geometry, otherwise it defers to the nodes
			std::uint64_t collisionFilter;
			if (error = triggerObject["collisionFilter"].get_uint64().get(collisionFilter); error == SUCCESS) {
				if (!isValidIndex(collisionFilter)) [[unlikely]]
					return Error::ValueOutOfRange;
				geometryTrigger.collisionFilter = static_cast<std::size_t>(collisionFilter);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		ondemand::array nodes;
		if (error = triggerObject["nodes"].get_array().get(nodes); error == SUCCESS) {
			auto& nodeTrigger = rigidBody.trigger.emplace().emplace<NodeTrigger>();

			std::size_t nodeCount;
			if (nodes.count_elements().get(nodeCount) != SUCCESS) [[unlikely]]
				return Error::InvalidJson;
			nodeTrigger.nodes.resize(nodeCount);
			auto i = 0U;
			for (auto num : nodes) {
				std::uint64_t val;
				if (num.get_uint64().get(val) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				if (!isValidIndex(val)) [[unlikely]]
					return Error::ValueOutOfRange;
				nodeTrigger.nodes[i] = static_cast<std::size_t>(val);
				++i;
			}

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}
	} else if (error != NO_SUCH_FIELD) [[unlikely]] {
		return Error::InvalidGltf;
	}

	ondemand::object jointObject;
	if (auto error = khr_physics_rigid_bodies["joint"].get_object().get(jointObject); error == SUCCESS) {
		auto& joint = rigidBody.joint.emplace();
		std::uint64_t connectedNodeId;
		if (jointObject["connectedNode"].get_uint64().get(connectedNodeId) == SUCCESS) [[likely]] {
			if (!isValidIndex(connectedNodeId)) [[unlikely]]
				return Error::ValueOutOfRange;
			joint.connectedNode = connectedNodeId;
		} else {
			return Error::InvalidGltf;
		}

		std::uint64_t jointId;
		if (jointObject["joint"].get_uint64().get(jointId) == SUCCESS) [[likely]] {
			if (!isValidIndex(jointId)) [[unlikely]]
				return Error::ValueOutOfRange;
			joint.joint = static_cast<std::size_t>(jointId);
		} else {
			return Error::InvalidGltf;
		}

		if (error = jointObject["enableCollision"].get_bool().get(joint.enableCollision); error != SUCCESS && error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}
	} else if (error != NO_SUCH_FIELD) [[unlikely]] {
		return Error::InvalidGltf;
	}

	return Error::None;
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
	if (jsonParser->parser.iterate(view).get(document) != SUCCESS || document.get_object().get(root.value) != SUCCESS) [[unlikely]] {
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
	if (jsonParser->parser.iterate(jsonChunkView).get(document) != SUCCESS || document.get_object().get(root.value) != SUCCESS) [[unlikely]] {
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
