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

#include <fstream>
#include <functional>
#include <mutex>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5030) // attribute 'x' is not recognized
#pragma warning(disable : 4514) // unreferenced inline function has been removed
#pragma warning(disable : 4710) // function not inlined
#endif

#include <simdjson.h>
#include <simdutf.h>

#include <fastgltf/core.hpp>
#include <fastgltf/base64.hpp>
#include <fastgltf/crc32.hpp>
#include <fastgltf/types.hpp>

namespace fg = fastgltf;
namespace fs = std::filesystem;

namespace fastgltf {
	[[nodiscard, gnu::always_inline]] inline bool getImageIndexForExtension(const simdjson::dom::element& element, optional<std::size_t>& imageIndexOut) {
		using namespace simdjson;

		dom::object source;
		if (element.get(source) != simdjson::SUCCESS) [[unlikely]] {
			return false;
		}
		std::uint64_t imageIndex;
		if (source["source"].get_uint64().get(imageIndex) != simdjson::SUCCESS) [[unlikely]] {
			return false;
		}

		imageIndexOut = static_cast<std::size_t>(imageIndex);
		return true;
	}

	[[nodiscard, gnu::always_inline]] inline bool parseTextureExtensions(Texture& texture, const simdjson::dom::object& extensions, const Extensions extensionFlags) {
		for (auto extension : extensions) {
			switch (crc32c(extension.key)) {
				case force_consteval<crc32c(extensions::KHR_texture_basisu)>: {
					if (!hasBit(extensionFlags, Extensions::KHR_texture_basisu))
						break;
					if (!getImageIndexForExtension(extension.value, texture.basisuImageIndex))
						return false;
					break;
				}
				case force_consteval<crc32c(extensions::MSFT_texture_dds)>: {
					if (!hasBit(extensionFlags, Extensions::MSFT_texture_dds))
						break;
					if (!getImageIndexForExtension(extension.value, texture.ddsImageIndex))
						return false;
					break;
				}
				case force_consteval<crc32c(extensions::EXT_texture_webp)>: {
					if (!hasBit(extensionFlags, Extensions::EXT_texture_webp))
						break;
					if (!getImageIndexForExtension(extension.value, texture.webpImageIndex))
						return false;
					break;
				}
				default:
					break;
			}
		}

		return true;
	}

	[[nodiscard, gnu::always_inline]] inline Error getJsonArray(const simdjson::dom::object& parent, std::string_view arrayName, simdjson::dom::array* array) noexcept {
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
	Error parseTextureInfo(const simdjson::dom::object& object, const std::string_view key,
		TextureInfo* info, const Extensions extensions) noexcept {
		using namespace simdjson;

		dom::object child;
		if (auto childErr = object[key].get_object().get(child); childErr == NO_SUCH_FIELD) {
			return Error::MissingField;
		} else if (childErr != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t index;
		if (child["index"].get_uint64().get(index) == SUCCESS) [[likely]] {
			info->textureIndex = static_cast<std::size_t>(index);
		} else {
			return Error::InvalidGltf;
		}

		if (const auto error = child["texCoord"].get_uint64().get(index); error == SUCCESS) [[likely]] {
			info->texCoordIndex = static_cast<std::size_t>(index);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidJson;
		}

		if constexpr (type == internal::TextureInfoType::NormalTexture) {
			double scale;
			if (const auto error = child["scale"].get_double().get(scale); error == SUCCESS) [[likely]] {
				reinterpret_cast<NormalTextureInfo*>(info)->scale = static_cast<num>(scale);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		} else if constexpr (type == internal::TextureInfoType::OcclusionTexture) {
			double strength;
			if (const auto error = child["strength"].get_double().get(strength); error == SUCCESS) [[likely]] {
				reinterpret_cast<OcclusionTextureInfo*>(info)->strength = static_cast<num>(strength);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		}

		dom::object extensionsObject;
		if (child["extensions"].get_object().get(extensionsObject) == SUCCESS) [[likely]] {
			dom::object textureTransform;
			if (hasBit(extensions, Extensions::KHR_texture_transform) && extensionsObject[extensions::KHR_texture_transform].get_object().get(textureTransform) == SUCCESS) [[likely]] {
				auto& transform = info->transform.emplace();
				transform.rotation = 0.0F;

				if (textureTransform["texCoord"].get_uint64().get(index) == SUCCESS) [[likely]] {
					transform.texCoordIndex = index;
				}

				double rotation = 0.0F;
				if (textureTransform["rotation"].get_double().get(rotation) == SUCCESS) [[likely]] {
					transform.rotation = static_cast<num>(rotation);
				}

				dom::array array;
				if (textureTransform["offset"].get_array().get(array) == SUCCESS) [[likely]] {
					for (auto i = 0U; i < 2; ++i) {
						double val;
						if (array.at(i).get_double().get(val) != SUCCESS) [[unlikely]] {
							return Error::InvalidGltf;
						}
						transform.uvOffset[i] = static_cast<num>(val);
					}
				}

				if (textureTransform["scale"].get_array().get(array) == SUCCESS) [[likely]] {
					for (auto i = 0U; i < 2; ++i) {
						double val;
						if (array.at(i).get_double().get(val) != SUCCESS) [[unlikely]] {
							return Error::InvalidGltf;
						}
						transform.uvScale[i] = static_cast<num>(val);
					}
				}
			}
		}

		return Error::None;
	}
} // namespace fastgltf

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

template <typename T> fg::Error fg::Parser::parseAttributes(simdjson::dom::object& object, T& attributes) {
	using namespace simdjson;

	// We iterate through the JSON object and write each key/pair value into the
	// attribute map. The keys are only validated in the validate() method.
	attributes.reserve(object.size());
	for (const auto field : object) {
		const auto key = field.key;

		std::uint64_t accessorIndex;
		if (field.value.get_uint64().get(accessorIndex) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		attributes.emplace_back(Attribute {
			.name = std::string(key),
			.accessorIndex = static_cast<std::size_t>(accessorIndex),
		});
	}
	return Error::None;
}

namespace fastgltf {
template<typename T>
static void writeIndices(static_vector<std::byte>& generatedIndices,
	const PrimitiveType type, const std::size_t primitiveCount) {

	const std::span indices(reinterpret_cast<T*>(generatedIndices.data()),
		generatedIndices.size() / sizeof(T));

	// Generate the correct indices for every primitive topology
	switch (type) {
		case PrimitiveType::Points: {
			for (std::size_t i = 0; i < primitiveCount; ++i)
				indices[i] = static_cast<T>(i);
			break;
		}
		case PrimitiveType::Lines:
		case PrimitiveType::LineLoop:
		case PrimitiveType::LineStrip: {
			for (std::size_t i = 0; i < primitiveCount; ++i) {
				indices[i * 2 + 0] = static_cast<T>(i * 2 + 0);
				indices[i * 2 + 1] = static_cast<T>(i * 2 + 1);
			}
			break;
		}
		case PrimitiveType::Triangles:
		case PrimitiveType::TriangleStrip:
		case PrimitiveType::TriangleFan: {
			for (std::size_t i = 0; i < primitiveCount; ++i) {
				indices[i * 3 + 0] = static_cast<T>(i * 3 + 0);
				indices[i * 3 + 1] = static_cast<T>(i * 3 + 1);
				indices[i * 3 + 2] = static_cast<T>(i * 3 + 2);
			}
			break;
		}
		default: FASTGLTF_UNREACHABLE
	}
}

static std::pair<static_vector<std::byte>, ComponentType> writeIndices(
	const PrimitiveType type, const std::size_t indexCount, const std::size_t primitiveCount) {

	if (indexCount < 255) {
		static_vector<std::byte> generatedIndices(for_overwrite, indexCount * sizeof(std::uint8_t));
		writeIndices<std::uint8_t>(generatedIndices, type, primitiveCount);
		return std::make_pair(std::move(generatedIndices), ComponentType::UnsignedByte);
	}

	if (indexCount < 65535) {
		static_vector<std::byte> generatedIndices(for_overwrite, indexCount * sizeof(std::uint16_t));
		writeIndices<std::uint16_t>(generatedIndices, type, primitiveCount);
		return std::make_pair(std::move(generatedIndices), ComponentType::UnsignedShort);
	}

	static_vector<std::byte> generatedIndices(for_overwrite, indexCount * sizeof(std::uint32_t));
	writeIndices<std::uint32_t>(generatedIndices, type, primitiveCount);
	return std::make_pair(std::move(generatedIndices), ComponentType::UnsignedInt);
}
}

fg::Error fg::Parser::generateMeshIndices(fastgltf::Asset& asset) const {
	for (auto& mesh : asset.meshes) {
		for (auto& primitive : mesh.primitives) {
			if (primitive.indicesAccessor.has_value())
				continue;

			auto positionAttribute = primitive.findAttribute("POSITION");
			if (positionAttribute == primitive.attributes.end()) {
				return Error::InvalidGltf;
			}
			if (positionAttribute->accessorIndex >= asset.accessors.size()) {
				return Error::InvalidGltf;
			}
			auto positionCount = asset.accessors[positionAttribute->accessorIndex].count;

			auto primitiveCount = [&]() -> std::size_t {
				switch (primitive.type) {
					case PrimitiveType::Points: return positionCount;
					case PrimitiveType::Lines: return positionCount / 2;
					case PrimitiveType::LineLoop:
					case PrimitiveType::LineStrip: return max<std::size_t>(0, positionCount - 1);
					case PrimitiveType::Triangles: return positionCount / 3;
					case PrimitiveType::TriangleStrip: return max<std::size_t>(0U, positionCount - 2);
					case PrimitiveType::TriangleFan: return max<std::size_t>(0U, positionCount - 2);
					default: FASTGLTF_UNREACHABLE
				}
			}();
			auto indexCount = [&]() -> std::size_t {
				switch (primitive.type) {
					case PrimitiveType::Points: return primitiveCount;
					case PrimitiveType::Lines:
					case PrimitiveType::LineLoop:
					case PrimitiveType::LineStrip: return primitiveCount * 2;
					case PrimitiveType::Triangles:
					case PrimitiveType::TriangleStrip:
					case PrimitiveType::TriangleFan: return primitiveCount * 3;
					default: FASTGLTF_UNREACHABLE
				}
			}();

			auto [generatedIndices, componentType] =
				writeIndices(primitive.type, indexCount, primitiveCount);

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

fg::Error fg::validate(const Asset& asset) {
	auto isExtensionUsed = [&used = asset.extensionsUsed](const std::string_view extension) {
		return std::ranges::any_of(used, [&](auto& arg) {
			return arg == extension;
		});
	};

	// From the spec: extensionsRequired is a subset of extensionsUsed. All values in extensionsRequired MUST also exist in extensionsUsed.
	if (asset.extensionsRequired.size() > asset.extensionsUsed.size()) {
		return Error::InvalidGltf;
	}
	for (const auto& required : asset.extensionsRequired) {
		bool found = false;
		for (const auto& used : asset.extensionsUsed) {
			if (required == used)
				found = true;
		}
		if (!found)
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
			auto componentByteSize = getComponentByteSize(accessor.componentType);
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

		if (accessor.bufferViewIndex.has_value()) {
			const auto& bufferView = asset.bufferViews[accessor.bufferViewIndex.value()];
			const auto stride = bufferView.byteStride.value_or(getElementByteSize(accessor.type, accessor.componentType));
			if (accessor.byteOffset + stride * (accessor.count - 1) +
				getComponentByteSize(accessor.componentType) * getNumComponents(accessor.type) > bufferView.byteLength)
				return Error::InvalidGltf;
		}

		if (accessor.max.has_value()) {
			if ((accessor.componentType == ComponentType::Float || accessor.componentType == ComponentType::Double)
				&& !accessor.max->isType<double>())
				return Error::InvalidGltf;

			if (accessor.max->size() != getNumComponents(accessor.type))
				return Error::InvalidGltf;
		}

		if (accessor.min.has_value()) {
			if ((accessor.componentType == ComponentType::Float || accessor.componentType == ComponentType::Double)
				&& !accessor.min->isType<double>())
				return Error::InvalidGltf;

			if (accessor.min->size() != getNumComponents(accessor.type))
				return Error::InvalidGltf;
		}

		if (accessor.sparse) {
			const auto& indicesView = asset.bufferViews[accessor.sparse->indicesBufferView];
			if (indicesView.byteStride || indicesView.target)
				return Error::InvalidGltf;

			const auto& valueView = asset.bufferViews[accessor.sparse->valuesBufferView];
			if (valueView.byteStride || valueView.target)
				return Error::InvalidGltf;
		}
	}

	for (const auto& animation : asset.animations) {
		if (animation.channels.empty())
			return Error::InvalidGltf;
		for (const auto& channel1 : animation.channels) {
			if (!channel1.nodeIndex.has_value())
				continue;
			for (const auto& channel2 : animation.channels) {
				if (&channel1 == &channel2)
					continue;
				if (channel1.nodeIndex == channel2.nodeIndex && channel1.path == channel2.path)
					return Error::InvalidGltf;
			}
		}

		if (animation.samplers.empty())
			return Error::InvalidGltf;
		for (const auto& channel : animation.channels) {
			const auto& sampler = animation.samplers[channel.samplerIndex];

			const auto& inputAccessor = asset.accessors[sampler.inputAccessor];
			// The accessor MUST be of scalar type with floating-point components
			if (inputAccessor.type != AccessorType::Scalar)
				return Error::InvalidGltf;
			if (inputAccessor.componentType != ComponentType::Float && inputAccessor.componentType != ComponentType::Double)
				return Error::InvalidGltf;
			if (inputAccessor.bufferViewIndex && asset.bufferViews[*inputAccessor.bufferViewIndex].meshoptCompression)
				continue;

			if (inputAccessor.count == 0)
				continue;

			if (channel.path == AnimationPath::Weights)
				continue; // TODO: For weights, the input count needs to be multiplied by the morph target count.

			const auto& outputAccessor = asset.accessors[sampler.outputAccessor];
			if (outputAccessor.bufferViewIndex && asset.bufferViews[*outputAccessor.bufferViewIndex].meshoptCompression)
				continue;

			switch (sampler.interpolation) {
				case AnimationInterpolation::Linear:
				case AnimationInterpolation::Step:
					if (inputAccessor.count != outputAccessor.count)
						return Error::InvalidGltf;
					break;
				case AnimationInterpolation::CubicSpline:
					if (inputAccessor.count < 2)
						return Error::InvalidGltf;
					if (inputAccessor.count * 3 != outputAccessor.count)
						return Error::InvalidGltf;
					break;
			}
		}
	}

	for (const auto& buffer : asset.buffers) {
		if (buffer.byteLength < 1)
			return Error::InvalidGltf;
	}

	for (const auto& bufferView : asset.bufferViews) {
		if (bufferView.byteLength < 1)
			return Error::InvalidGltf;
		if (bufferView.byteStride.has_value() && (*bufferView.byteStride < 4U || *bufferView.byteStride > 252U || *bufferView.byteStride % 4 != 0))
			return Error::InvalidGltf;
		if (bufferView.bufferIndex >= asset.buffers.size())
			return Error::InvalidGltf;

		if (const auto& buffer = asset.buffers[bufferView.bufferIndex];
			bufferView.byteOffset + bufferView.byteLength > buffer.byteLength)
			return Error::InvalidGltf;

		if (bufferView.meshoptCompression.has_value() &&
			!(isExtensionUsed(extensions::EXT_meshopt_compression) || isExtensionUsed(extensions::KHR_meshopt_compression)))
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
			if (light.outerConeAngle.value() > std::numbers::pi / 2)
				return Error::InvalidGltf;
		}
	}

	for (const auto& material : asset.materials) {
		auto isInvalidTexture = [&textures = asset.textures](const std::size_t textureIndex) {
			return textureIndex >= textures.size();
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
		if (material.specularGlossiness && !isExtensionUsed(extensions::KHR_materials_pbrSpecularGlossiness))
			return Error::InvalidGltf;
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
		if (material.diffuseTransmission && !isExtensionUsed(extensions::KHR_materials_diffuse_transmission))
			return Error::InvalidGltf;
	}

	for (const auto& mesh : asset.meshes) {
		for (const auto& primitive : mesh.primitives) {
			if (primitive.materialIndex.has_value() && *primitive.materialIndex >= asset.materials.size())
				return Error::InvalidGltf;

			if (!primitive.mappings.empty()) {
				if (!isExtensionUsed(fastgltf::extensions::KHR_materials_variants))
					return Error::InvalidGltf;
				if (primitive.mappings.size() != asset.materialVariants.size())
					return Error::InvalidGltf;
				for (const auto& mapping : primitive.mappings) {
					if (!mapping.has_value())
						continue;
					if (mapping.value() >= asset.materials.size())
						return Error::InvalidGltf;
				}
			}

			if (primitive.indicesAccessor.has_value()) {
				if (*primitive.indicesAccessor >= asset.accessors.size())
					return Error::InvalidGltf;
				const auto& accessor = asset.accessors[*primitive.indicesAccessor];
				if (accessor.bufferViewIndex.has_value()) {
					const auto& bufferView = asset.bufferViews[*accessor.bufferViewIndex];
					// The byteStride property must not be set on anything but vertex attributes.
					if (bufferView.byteStride.has_value())
						return Error::InvalidGltf;
				}
			}

			for (const auto& [name, index] : primitive.attributes) {
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
					if (!accessor.max.has_value() || !accessor.min.has_value())
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
			if (pTRS->rotation.x() > 1.0 || pTRS->rotation.x() < -1.0)
				return Error::InvalidGltf;
			if (pTRS->rotation.y() > 1.0 || pTRS->rotation.y() < -1.0)
				return Error::InvalidGltf;
			if (pTRS->rotation.z() > 1.0 || pTRS->rotation.z() < -1.0)
				return Error::InvalidGltf;
			if (pTRS->rotation.w() > 1.0 || pTRS->rotation.w() < -1.0)
				return Error::InvalidGltf;
		}

		if ((node.skinIndex.has_value() || !node.weights.empty()) && !node.meshIndex.has_value()) {
			return Error::InvalidGltf;
		}

		if (node.skinIndex.has_value()) {
			// "When the node contains skin, all mesh.primitives MUST contain JOINTS_0 and WEIGHTS_0 attributes."
			const auto& mesh = asset.meshes[node.meshIndex.value()];
			for (const auto& primitive : mesh.primitives) {
				const auto joints0 = primitive.findAttribute("JOINTS_0");
				const auto weights0 = primitive.findAttribute("WEIGHTS_0");
				if (joints0 == primitive.attributes.end() || weights0 == primitive.attributes.end())
					return Error::InvalidGltf;
			}
		}

		if (!node.visible && !isExtensionUsed(extensions::KHR_node_visibility)) {
			return Error::InvalidGltf;
		}
		if (!node.selectable && !isExtensionUsed(extensions::KHR_node_selectability)) {
			return Error::InvalidGltf;
		}
		if (!node.hoverable && !isExtensionUsed(extensions::KHR_node_hoverability)) {
			return Error::InvalidGltf;
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

	if (isExtensionUsed(extensions::GODOT_single_root)) {
		// The document "scenes" array MUST have exactly one scene.
		if (asset.scenes.size() != 1) {
			return Error::InvalidGltf;
		}
		// The document "scene" index MUST be set to 0, the index of the only scene in the "scenes" array.
		if (asset.defaultScene != 0U) {
			return Error::InvalidGltf;
		}
		// The scene MUST have exactly one node, the single root node.
		if (asset.scenes[0].nodeIndices.size() != 1) {
			return Error::InvalidGltf;
		}
		// The scene's node index MUST be set to 0, the index of the first node in the "nodes" array.
		if (asset.scenes[0].nodeIndices[0] != 0) {
			return Error::InvalidGltf;
		}
		if (asset.nodes.empty()) {
			return Error::InvalidGltf;
		}
		auto &transform = asset.nodes[0].transform;
		TRS trs;
		// The "translation", "rotation", "scale", and "matrix" properties of the single root node MUST either be not defined or have default values.
		if (!std::holds_alternative<TRS>(transform)){
			math::decomposeTransformMatrix(std::get<math::fmat4x4>(transform), trs.scale, trs.rotation, trs.translation);
		} else {
			trs = std::get<TRS>(transform);
		}
		auto defaultTRS = TRS{};
		if (trs.rotation != defaultTRS.rotation) {
			return Error::InvalidGltf;
		}
		if (trs.scale != defaultTRS.scale) {
			return Error::InvalidGltf;
		}
		if (trs.translation != defaultTRS.translation) {
			return Error::InvalidGltf;
		}
	}

	return Error::None;
}

fg::Expected<fg::Asset> fg::Parser::parse(simdjson::dom::object root, Category categories) {
	using namespace simdjson;
	fillCategories(categories);

	Asset asset {};

	if (!hasBit(options, Options::DontRequireValidAssetMember)) {
		dom::object assetInfo;
		AssetInfo info {};
		if (const auto error = root["asset"].get_object().get(assetInfo); error == NO_SUCH_FIELD) {
			return Error::MissingField;
		} else if (error != SUCCESS) [[unlikely]] {
			return Error::InvalidJson;
		}

		std::string_view version;
		if (const auto error = assetInfo["version"].get_string().get(version); error != SUCCESS) [[unlikely]] {
			return error == NO_SUCH_FIELD ? Error::MissingField : Error::InvalidGltf;
		}

		const auto major = static_cast<std::uint32_t>(version.substr(0, 1)[0] - '0');
		// std::uint32_t minor = version.substr(2, 3)[0] - '0';
		if (major != 2U) {
			return Error::UnsupportedVersion;
		}
		info.gltfVersion = version;

		std::string_view copyright;
		if (assetInfo["copyright"].get_string().get(copyright) == SUCCESS) [[likely]] {
			info.copyright = copyright;
		}

		std::string_view generator;
		if (assetInfo["generator"].get_string().get(generator) == SUCCESS) [[likely]] {
			info.generator = generator;
		}

		std::string_view minVersion;
		if (assetInfo["minVersion"].get_string().get(minVersion) == SUCCESS) [[likely]] {
			info.minVersion = minVersion;
		}

		asset.assetInfo = std::move(info);
	}

	if (dom::array extensionsRequired; root["extensionsRequired"].get_array().get(extensionsRequired) == SUCCESS) [[likely]] {
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

	auto readCategories = Category::None;
	for (const auto object : root) {
		auto hashedKey = crc32c(object.key);
		if (hashedKey == force_consteval<crc32c("scene")>) {
			std::uint64_t defaultScene;
			if (object.value.get_uint64().get(defaultScene) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			asset.defaultScene = static_cast<std::size_t>(defaultScene);
			continue;
		}

		if (hashedKey == force_consteval<crc32c("extensions")>) {
			dom::object extensionsObject;
			if (object.value.get_object().get(extensionsObject) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			if (auto error = parseExtensions(extensionsObject, asset); error != Error::None)
				return error;
			continue;
		}

		if (hashedKey == force_consteval<crc32c("asset")> || hashedKey == force_consteval<crc32c("extras")>) {
			continue;
		}

		dom::array array;
		if (object.value.get_array().get(array) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

#define KEY_SWITCH_CASE(name, id) case force_consteval<crc32c(FASTGLTF_QUOTE(id))>:       \
				if (hasBit(categories, Category::name))   \
					error = parse##name(array, asset);                     \
				readCategories |= Category::name;         \
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
				for (auto usedValue : array) {
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

fg::Error fg::Parser::parseAccessors(const simdjson::dom::array& accessors, Asset& asset) {
	using namespace simdjson;

	asset.accessors.reserve(accessors.size());
	for (auto accessorValue : accessors) {
		// Required fields: "componentType", "count"
		Accessor accessor {};
		dom::object accessorObject;
		if (accessorValue.get_object().get(accessorObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t componentType;
		if (accessorObject["componentType"].get_uint64().get(componentType) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		accessor.componentType = internal::getComponentType(static_cast<std::underlying_type_t<ComponentType>>(componentType));
		if (accessor.componentType == ComponentType::Double && (!hasBit(options, Options::AllowDouble) || !hasBit(config.extensions, Extensions::KHR_accessor_float64))) {
			return Error::InvalidGltf;
		}

		std::string_view accessorType;
		if (accessorObject["type"].get_string().get(accessorType) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		accessor.type = internal::getAccessorType(accessorType);

		std::uint64_t accessorCount;
		if (accessorObject["count"].get_uint64().get(accessorCount) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		accessor.count = static_cast<std::size_t>(accessorCount);


		std::uint64_t bufferView;
		if (accessorObject["bufferView"].get_uint64().get(bufferView) == SUCCESS) [[likely]] {
			accessor.bufferViewIndex = static_cast<std::size_t>(bufferView);
		}

		// byteOffset is optional, but defaults to 0
		std::uint64_t byteOffset;
		if (auto error = accessorObject["byteOffset"].get_uint64().get(byteOffset); error == SUCCESS) [[likely]] {
			accessor.byteOffset = static_cast<std::size_t>(byteOffset);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		// Type of min and max should always be the same.
		auto parseMinMax = [&](const std::string_view key, optional<AccessorBoundsArray>& ref) -> Error {
			dom::array elements;
			if (accessorObject[key].get_array().get(elements) == SUCCESS) [[likely]] {
				const auto num = getNumComponents(accessor.type);

				if (accessor.componentType == ComponentType::Float || accessor.componentType == ComponentType::Double) {
					ref = AccessorBoundsArray(num, AccessorBoundsArray::BoundsType::float64);
				} else {
					ref = AccessorBoundsArray(num, AccessorBoundsArray::BoundsType::int64);
				}

				auto& array = ref.value();
				std::size_t idx = 0;
				for (auto element : elements) {
					if (idx == num) [[unlikely]] {
						return Error::InvalidGltf;
					}

					switch (element.type()) {
						case dom::element_type::DOUBLE: {
							double value;
							if (element.get_double().get(value) != SUCCESS) [[unlikely]] {
								return Error::InvalidGltf;
							}

							if (array.isType<double>()) {
								array.set(idx++, value);
							} else if (array.isType<std::int64_t>()) {
								array.set(idx++, static_cast<std::int64_t>(value));
							} else {
								FASTGLTF_UNREACHABLE
							}
							break;
						}
						case dom::element_type::INT64: {
							std::int64_t value;
							if (element.get_int64().get(value) != SUCCESS) [[unlikely]] {
								return Error::InvalidGltf;
							}

							if (array.isType<double>()) {
								array.set(idx++, static_cast<double>(value));
							} else if (array.isType<std::int64_t>()) {
								array.set(idx++, value);
							} else {
								FASTGLTF_UNREACHABLE
							}
							break;
						}
						case dom::element_type::UINT64: {
							// Note that the glTF spec doesn't care about any integer larger than 32-bits, so
							// truncating uint64 to int64 wouldn't make any difference, as those large values
							// aren't allowed anyway.
							std::uint64_t value;
							if (element.get_uint64().get(value) != SUCCESS) [[unlikely]] {
								return Error::InvalidGltf;
							}

							if (array.isType<double>()) {
								array.set(idx++, static_cast<double>(value));
							} else if (array.isType<std::int64_t>()) {
								array.set(idx++, static_cast<std::int64_t>(value));
							} else {
								FASTGLTF_UNREACHABLE
							}
							break;
						}
						default: return Error::InvalidGltf;
					}
				}

				if (idx < num) [[unlikely]] {
					return Error::InvalidGltf;
				}
			}
			return Error::None;
		};

		if (auto error = parseMinMax("max", accessor.max); error != Error::None) {
			return error;
		}
		if (auto error = parseMinMax("min", accessor.min); error != Error::None) {
			return error;
		}

		if (auto error = accessorObject["normalized"].get_bool().get(accessor.normalized); error != SUCCESS && error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		// This property MUST NOT be set to true for accessors with FLOAT or UNSIGNED_INT component type.
		if (accessor.normalized && (accessor.componentType == ComponentType::UnsignedInt || accessor.componentType == ComponentType::Float)) {
			return Error::InvalidGltf;
		}

		dom::object sparseAccessorObject;
		if (accessorObject["sparse"].get_object().get(sparseAccessorObject) == SUCCESS) [[likely]] {
			SparseAccessor sparse {};
			std::uint64_t value;
			dom::object child;
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
			sparse.valuesBufferView = static_cast<std::size_t>(value);

			if (auto error = child["byteOffset"].get_uint64().get(value); error == SUCCESS) [[likely]] {
				sparse.valuesByteOffset = static_cast<std::size_t>(value);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			accessor.sparse = sparse;
		}

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = accessorObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.accessors.size(), Category::Accessors, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
		}

		std::string_view name;
		if (accessorObject["name"].get_string().get(name) == SUCCESS) {
			accessor.name.assign(name);
		}

		asset.accessors.emplace_back(std::move(accessor));
	}

	return Error::None;
}

fg::Error fg::Parser::parseAnimations(simdjson::dom::array& animations, Asset& asset) {
	using namespace simdjson;

	asset.animations.reserve(animations.size());
	for (auto animationValue : animations) {
		dom::object animationObject;
		Animation animation {};
		if (animationValue.get_object().get(animationObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::array channels;
		auto channelError = getJsonArray(animationObject, "channels", &channels);
		if (channelError != Error::None) {
			return Error::InvalidGltf;
		}

		animation.channels.reserve(channels.size());
		for (auto channelValue : channels) {
			dom::object channelObject;
			AnimationChannel channel {};
			if (channelValue.get_object().get(channelObject) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			std::uint64_t sampler;
			if (channelObject["sampler"].get_uint64().get(sampler) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			channel.samplerIndex = static_cast<std::size_t>(sampler);

			dom::object targetObject;
			if (channelObject["target"].get_object().get(targetObject) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			} else {
				std::uint64_t node;
				if (auto error = targetObject["node"].get_uint64().get(node); error == SUCCESS) [[likely]] {
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

		dom::array samplers;
		auto samplerError = getJsonArray(animationObject, "samplers", &samplers);
		if (samplerError != Error::None) {
			return Error::InvalidGltf;
		}

		animation.samplers.reserve(samplers.size());
		for (auto samplerValue : samplers) {
			dom::object samplerObject;
			AnimationSampler sampler {};
			if (samplerValue.get_object().get(samplerObject) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			std::uint64_t input;
			if (samplerObject["input"].get_uint64().get(input) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			sampler.inputAccessor = static_cast<std::size_t>(input);

			std::uint64_t output;
			if (samplerObject["output"].get_uint64().get(output) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
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

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = animationObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.animations.size(), Category::Animations, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
		}


		std::string_view name;
		if (animationObject["name"].get_string().get(name) == SUCCESS) {
			animation.name.assign(name);
		}

		asset.animations.emplace_back(std::move(animation));
	}

	return Error::None;
}

fg::Error fg::Parser::parseBuffers(simdjson::dom::array& buffers, Asset& asset) {
	using namespace simdjson;

	asset.buffers.reserve(buffers.size());
	std::size_t bufferIndex = 0;
	for (auto bufferValue : buffers) {
		// Required fields: "byteLength"
		Buffer buffer {};
		dom::object bufferObject;
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

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = bufferObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.buffers.size(), Category::Buffers, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
		}

		std::string_view name;
		if (bufferObject["name"].get_string().get(name) == SUCCESS) {
			buffer.name.assign(name);
		}

		++bufferIndex;
		asset.buffers.emplace_back(std::move(buffer));
	}

	return Error::None;
}

fg::Error fg::Parser::parseBufferViews(const simdjson::dom::array& bufferViews, Asset& asset) {
	using namespace simdjson;

	asset.bufferViews.reserve(bufferViews.size());
	for (auto bufferViewValue : bufferViews) {
		dom::object bufferViewObject;
		if (bufferViewValue.get_object().get(bufferViewObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t number;
		BufferView view {};
		if (auto error = bufferViewObject["buffer"].get_uint64().get(number); error != SUCCESS) [[unlikely]] {
			return error == NO_SUCH_FIELD ? Error::InvalidGltf : Error::InvalidJson;
		}
		view.bufferIndex = static_cast<std::size_t>(number);

		if (auto error = bufferViewObject["byteOffset"].get_uint64().get(number); error == SUCCESS) [[likely]] {
			view.byteOffset = static_cast<std::size_t>(number);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidJson;
		}

		if (auto error = bufferViewObject["byteLength"].get_uint64().get(number); error != SUCCESS) [[unlikely]] {
			return error == NO_SUCH_FIELD ? Error::InvalidGltf : Error::InvalidJson;
		}
		view.byteLength = static_cast<std::size_t>(number);

		if (auto error = bufferViewObject["byteStride"].get_uint64().get(number); error == SUCCESS) [[likely]] {
			view.byteStride = static_cast<std::size_t>(number);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidJson;
		}

		if (auto error = bufferViewObject["target"].get_uint64().get(number); error == SUCCESS) [[likely]] {
			view.target = static_cast<BufferTarget>(number);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidJson;
		}

		std::string_view string;
		if (auto error = bufferViewObject["name"].get_string().get(string); error == SUCCESS) {
			view.name.assign(string);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidJson;
		}

		dom::object extensionObject;
		if (bufferViewObject["extensions"].get_object().get(extensionObject) == SUCCESS) [[likely]] {
			constexpr auto parseMeshoptCompression = [](CompressedBufferView& compression, const dom::object& meshoptCompression) {
				std::uint64_t number;
				if (auto error = meshoptCompression["buffer"].get_uint64().get(number); error != SUCCESS) [[unlikely]] {
					return error == NO_SUCH_FIELD ? Error::InvalidGltf : Error::InvalidJson;
				}
				compression.bufferIndex = static_cast<std::size_t>(number);

				if (auto error = meshoptCompression["byteOffset"].get_uint64().get(number); error == SUCCESS) [[likely]] {
					compression.byteOffset = static_cast<std::size_t>(number);
				} else if (error == NO_SUCH_FIELD) {
					compression.byteOffset = 0;
				} else {
					return Error::InvalidJson;
				}

				if (auto error = meshoptCompression["byteLength"].get_uint64().get(number); error != SUCCESS) [[unlikely]] {
					return error == NO_SUCH_FIELD ? Error::InvalidGltf : Error::InvalidJson;
				}
				compression.byteLength = static_cast<std::size_t>(number);

				if (auto error = meshoptCompression["byteStride"].get_uint64().get(number); error != SUCCESS) [[unlikely]] {
					return error == NO_SUCH_FIELD ? Error::InvalidGltf : Error::InvalidJson;
				}
				compression.byteStride = static_cast<std::size_t>(number);

				if (auto error = meshoptCompression["count"].get_uint64().get(number); error != SUCCESS) [[unlikely]] {
					return error == NO_SUCH_FIELD ? Error::InvalidGltf : Error::InvalidJson;
				}
				compression.count = number;

				std::string_view string;
				if (auto error = meshoptCompression["mode"].get_string().get(string); error != SUCCESS) [[unlikely]] {
					return error == NO_SUCH_FIELD ? Error::InvalidGltf : Error::InvalidJson;
				}
				switch (crc32c(string)) {
					case force_consteval<crc32c("ATTRIBUTES")>: {
						compression.mode = MeshoptCompressionMode::Attributes;
						break;
					}
					case force_consteval<crc32c("TRIANGLES")>: {
						compression.mode = MeshoptCompressionMode::Triangles;
						break;
					}
					case force_consteval<crc32c("INDICES")>: {
						compression.mode = MeshoptCompressionMode::Indices;
						break;
					}
					default: {
						return Error::InvalidGltf;
					}
				}

				if (auto error = meshoptCompression["filter"].get_string().get(string); error == SUCCESS) [[likely]] {
					switch (crc32c(string)) {
						case force_consteval<crc32c("NONE")>: {
							compression.filter = MeshoptCompressionFilter::None;
							break;
						}
						case force_consteval<crc32c("OCTAHEDRAL")>: {
							compression.filter = MeshoptCompressionFilter::Octahedral;
							break;
						}
						case force_consteval<crc32c("QUATERNION")>: {
							compression.filter = MeshoptCompressionFilter::Quaternion;
							break;
						}
						case force_consteval<crc32c("EXPONENTIAL")>: {
							compression.filter = MeshoptCompressionFilter::Exponential;
							break;
						}
						case force_consteval<crc32c("COLOR")>: {
							compression.filter = MeshoptCompressionFilter::Color;
							break;
						}
						default: {
							return Error::InvalidGltf;
						}
					}
				} else if (error == NO_SUCH_FIELD) {
					compression.filter = MeshoptCompressionFilter::None;
				} else {
					return Error::InvalidJson;
				}

				return Error::None;
			};

			dom::object meshoptCompression;
			if (hasBit(config.extensions, Extensions::KHR_meshopt_compression) &&
				extensionObject[extensions::KHR_meshopt_compression].get_object().get(meshoptCompression) == SUCCESS) {

				auto& compression = view.meshoptCompression.emplace();
				if (const auto error = parseMeshoptCompression(compression, meshoptCompression); error != Error::None) [[unlikely]] {
					return error;
				}
			} else if (hasBit(config.extensions, Extensions::EXT_meshopt_compression) &&
				extensionObject[extensions::EXT_meshopt_compression].get_object().get(meshoptCompression) == SUCCESS) {

				auto& compression = view.meshoptCompression.emplace();
				if (const auto error = parseMeshoptCompression(compression, meshoptCompression); error != Error::None) [[unlikely]] {
					return error;
				}
			}
		}

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = bufferViewObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.bufferViews.size(), Category::BufferViews, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
		}

		asset.bufferViews.emplace_back(std::move(view));
	}

	return Error::None;
}

fg::Error fg::Parser::parseCameras(simdjson::dom::array& cameras, Asset& asset) {
	using namespace simdjson;

	asset.cameras.reserve(cameras.size());
	for (auto cameraValue : cameras) {
		Camera camera {};
		dom::object cameraObject;
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
			dom::object perspectiveCamera;
			if (cameraObject["perspective"].get_object().get(perspectiveCamera) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			Camera::Perspective perspective {};
			double value;
			if (auto error = perspectiveCamera["aspectRatio"].get_double().get(value); error == SUCCESS) [[likely]] {
				perspective.aspectRatio = static_cast<num>(value);
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}

			if (auto error = perspectiveCamera["zfar"].get_double().get(value); error == SUCCESS) [[likely]] {
				perspective.zfar = static_cast<num>(value);
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}

			if (perspectiveCamera["yfov"].get_double().get(value) == SUCCESS) [[likely]] {
				perspective.yfov = static_cast<num>(value);
			} else {
				return Error::InvalidGltf;
			}

			if (perspectiveCamera["znear"].get_double().get(value) == SUCCESS) [[likely]] {
				perspective.znear = static_cast<num>(value);
			} else {
				return Error::InvalidGltf;
			}

			camera.camera = perspective;
		} else if (type == "orthographic") {
			dom::object orthographicCamera;
			if (cameraObject["orthographic"].get_object().get(orthographicCamera) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			Camera::Orthographic orthographic {};
			double value;
			if (orthographicCamera["xmag"].get_double().get(value) == SUCCESS) [[likely]] {
				orthographic.xmag = static_cast<num>(value);
			} else {
				return Error::InvalidGltf;
			}

			if (orthographicCamera["ymag"].get_double().get(value) == SUCCESS) [[likely]] {
				orthographic.ymag = static_cast<num>(value);
			} else {
				return Error::InvalidGltf;
			}

			if (orthographicCamera["zfar"].get_double().get(value) == SUCCESS) [[likely]] {
				orthographic.zfar = static_cast<num>(value);
			} else {
				return Error::InvalidGltf;
			}

			if (orthographicCamera["znear"].get_double().get(value) == SUCCESS) [[likely]] {
				orthographic.znear = static_cast<num>(value);
			} else {
				return Error::InvalidGltf;
			}

			camera.camera = orthographic;
		} else {
			return Error::InvalidGltf;
		}

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = cameraObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.cameras.size(), Category::Cameras, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
		}

		asset.cameras.emplace_back(std::move(camera));
	}

	return Error::None;
}

fg::Error fg::Parser::parseExtensions(const simdjson::dom::object& extensionsObject, Asset& asset) {
	using namespace simdjson;

	for (auto extensionValue : extensionsObject) {
		dom::object extensionObject;
		if (auto error = extensionValue.value.get_object().get(extensionObject); error != SUCCESS) [[unlikely]] {
			if (error == INCORRECT_TYPE) {
				continue; // We want to ignore
			}
			return Error::InvalidGltf;
		}

		switch (crc32c(extensionValue.key)) {
			case force_consteval<crc32c(extensions::KHR_lights_punctual)>: {
				if (!hasBit(config.extensions, Extensions::KHR_lights_punctual))
					break;

				dom::array lightsArray;
				if (auto error = extensionObject["lights"].get_array().get(lightsArray); error == SUCCESS) [[likely]] {
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

				dom::array variantsArray;
				if (auto arrayError = extensionObject["variants"].get_array().get(variantsArray); arrayError == SUCCESS) [[likely]] {
					asset.materialVariants.reserve(variantsArray.size());
					for (auto variant : variantsArray) {
						dom::object variantObject;
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

#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
			case force_consteval<crc32c(extensions::KHR_implicit_shapes)>: {
				if (!hasBit(config.extensions, Extensions::KHR_implicit_shapes)) {
					break;
				}

				dom::array shapesArray;
				if (auto arrayError = extensionObject["shapes"].get_array().get(shapesArray); arrayError == SUCCESS) [[likely]] {
					if (auto shapesError = parseShapes(shapesArray, asset); shapesError != Error::None) [[unlikely]] {
						return shapesError;
					}
				} else {
					return Error::InvalidGltf;
				}
				break;
			}
#endif
#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
			case force_consteval<crc32c(extensions::KHR_physics_rigid_bodies)>: {
				if (!hasBit(config.extensions, Extensions::KHR_physics_rigid_bodies)) {
					break;
				}

				dom::array materialsArray;
				if (auto arrayError = extensionObject["physicsMaterials"].get_array().get(materialsArray); arrayError == SUCCESS) {
					if (auto materialsError = parsePhysicsMaterials(materialsArray, asset); materialsError != Error::None) [[unlikely]] {
						return materialsError;
					}
				} else if (arrayError != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				dom::array filtersArray;
				if (auto arrayError = extensionObject["collisionFilters"].get_array().get(filtersArray); arrayError == SUCCESS) {
					if (auto filtersError = parseCollisionFilters(filtersArray, asset); filtersError != Error::None) [[unlikely]] {
						return filtersError;
					}
				} else if (arrayError != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				dom::array jointsArray;
				if (auto arrayError = extensionObject["physicsJoints"].get_array().get(jointsArray); arrayError == SUCCESS) {
					if (auto jointsError = parsePhysicsJoints(jointsArray, asset); jointsError != Error::None) [[unlikely]] {
						return jointsError;
					}
				} else if (arrayError != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				break;
			}
#endif
			default:
				continue;
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parseImages(simdjson::dom::array& images, Asset& asset) {
	using namespace simdjson;

	asset.images.reserve(images.size());
	for (auto imageValue : images) {
		Image image {};
		dom::object imageObject;
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

			image.data = sources::BufferView {
				.bufferViewIndex = static_cast<std::size_t>(bufferViewIndex),
				.mimeType = internal::getMimeTypeFromString(mimeType),
			};
		}

		if (std::holds_alternative<std::monostate>(image.data)) {
			return Error::InvalidGltf;
		}

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = imageObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.images.size(), Category::Images, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
		}

		// name is optional.
		std::string_view name;
		if (imageObject["name"].get_string().get(name) == SUCCESS) {
			image.name.assign(name);
		}

		asset.images.emplace_back(std::move(image));
	}

	return Error::None;
}

fg::Error fg::Parser::parseLights(const simdjson::dom::array& lights, Asset& asset) {
	using namespace simdjson;

	asset.lights.reserve(lights.size());
	for (auto lightValue : lights) {
		dom::object lightObject;
		if (lightValue.get_object().get(lightObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		Light light {};

		std::string_view type;
		if (lightObject["type"].get_string().get(type) == SUCCESS) [[likely]] {
			switch (crc32c(type.data())) {
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
			dom::object spotObject;
			if (lightObject["spot"].get_object().get(spotObject) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			double innerConeAngle;
			if (auto error = spotObject["innerConeAngle"].get_double().get(innerConeAngle); error == SUCCESS) [[likely]] {
				light.innerConeAngle = static_cast<num>(innerConeAngle);
			} else if (error == NO_SUCH_FIELD) {
				light.innerConeAngle = 0.0f;
			} else {
				return Error::InvalidGltf;
			}

			double outerConeAngle;
			if (auto error = spotObject["outerConeAngle"].get_double().get(outerConeAngle); error == SUCCESS) [[likely]] {
				light.outerConeAngle = static_cast<num>(outerConeAngle);
			} else if (error == NO_SUCH_FIELD) {
				light.outerConeAngle = static_cast<num>(std::numbers::pi / 4.0);
			} else {
				return Error::InvalidGltf;
			}
		}

		dom::array colorArray;
		if (auto error = lightObject["color"].get_array().get(colorArray); error == SUCCESS) [[likely]] {
			if (colorArray.size() != 3U) {
				return Error::InvalidGltf;
			}
			for (std::size_t i = 0U; i < colorArray.size(); ++i) {
				double color;
				if (colorArray.at(i).get_double().get(color) == SUCCESS) [[likely]] {
					light.color[i] = static_cast<num>(color);
				} else {
					return Error::InvalidGltf;
				}
			}
		} else if (error == NO_SUCH_FIELD) {
			light.color = math::nvec3(1);
		} else {
			return Error::InvalidGltf;
		}

		double intensity;
		if (lightObject["intensity"].get_double().get(intensity) == SUCCESS) [[likely]] {
			light.intensity = static_cast<num>(intensity);
		} else {
			light.intensity = 1.0f;
		}

		double range;
		if (lightObject["range"].get_double().get(range) == SUCCESS) [[likely]] {
			light.range = static_cast<num>(range);
		}

		std::string_view name;
		if (lightObject["name"].get_string().get(name) == SUCCESS) {
			light.name.assign(name);
		}

		asset.lights.emplace_back(std::move(light));
	}

	return Error::None;
}

fg::Error fg::Parser::parseMaterialExtensions(simdjson::dom::object &object, Material &material) {
	using namespace simdjson;

	for (auto extensionField : object) {
		switch (crc32c(extensionField.key)) {
			case force_consteval<crc32c(extensions::KHR_materials_anisotropy)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_anisotropy))
					break;

				dom::object anisotropyObject;
				auto anisotropyError = extensionField.value.get_object().get(anisotropyObject);
				if (anisotropyError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& anisotropy = material.anisotropy.emplace();

				double anisotropyStrength;
				if (auto error = anisotropyObject["anisotropyStrength"].get_double().get(anisotropyStrength);
						error == SUCCESS) [[likely]] {
					anisotropy.anisotropyStrength = static_cast<num>(anisotropyStrength);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidJson;
				}

				double anisotropyRotation;
				if (auto error = anisotropyObject["anisotropyRotation"].get_double().get(anisotropyRotation);
						error == SUCCESS) [[likely]] {
					anisotropy.anisotropyRotation = static_cast<num>(anisotropyRotation);
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

				dom::object clearcoatObject;
				auto clearcoatError = extensionField.value.get_object().get(clearcoatObject);
				if (clearcoatError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& clearcoat = material.clearcoat.emplace();

				double clearcoatFactor;
				if (auto error = clearcoatObject["clearcoatFactor"].get_double().get(clearcoatFactor); error ==
																									   SUCCESS) {
					clearcoat.clearcoatFactor = static_cast<num>(clearcoatFactor);
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
					clearcoat.clearcoatRoughnessFactor = static_cast<num>(clearcoatRoughnessFactor);
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

				dom::object dispersionObject;
				auto dispersionError = extensionField.value.get_object().get(dispersionObject);
				if (dispersionError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double dispersionFactor;
				auto error = dispersionObject["dispersion"].get_double().get(dispersionFactor);
				if (error == SUCCESS) [[likely]] {
					material.dispersion = static_cast<num>(dispersionFactor);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_emissive_strength)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_emissive_strength))
					break;

				dom::object emissiveObject;
				auto emissiveError = extensionField.value.get_object().get(emissiveObject);
				if (emissiveError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double emissiveStrength;
				auto error = emissiveObject["emissiveStrength"].get_double().get(emissiveStrength);
				if (error == SUCCESS) [[likely]] {
					material.emissiveStrength = static_cast<num>(emissiveStrength);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_ior)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_ior))
					break;

				dom::object iorObject;
				auto iorError = extensionField.value.get_object().get(iorObject);
				if (iorError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double ior;
				auto error = iorObject["ior"].get_double().get(ior);
				if (error == SUCCESS) [[likely]] {
					material.ior = static_cast<num>(ior);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidJson;
				}
				break;
			}
			case force_consteval<crc32c(extensions::KHR_materials_iridescence)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_iridescence))
					break;

				dom::object iridescenceObject;
				auto iridescenceError = extensionField.value.get_object().get(iridescenceObject);
				if (iridescenceError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& iridescence = material.iridescence.emplace();

				double iridescenceFactor;
				if (auto error = iridescenceObject["iridescenceFactor"].get_double().get(iridescenceFactor);
						error == SUCCESS) [[likely]] {
					iridescence.iridescenceFactor = static_cast<num>(iridescenceFactor);
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
				if (auto error = iridescenceObject["iridescenceIor"].get_double().get(iridescenceIor); error ==
																									   SUCCESS) {
					iridescence.iridescenceIor = static_cast<num>(iridescenceIor);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double iridescenceThicknessMinimum;
				if (auto error = iridescenceObject["iridescenceThicknessMinimum"].get_double().get(
							iridescenceThicknessMinimum); error == SUCCESS) [[likely]] {
					iridescence.iridescenceThicknessMinimum = static_cast<num>(iridescenceThicknessMinimum);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double iridescenceThicknessMaximum;
				if (auto error = iridescenceObject["iridescenceThicknessMaximum"].get_double().get(
							iridescenceThicknessMaximum); error == SUCCESS) [[likely]] {
					iridescence.iridescenceThicknessMaximum = static_cast<num>(iridescenceThicknessMaximum);
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

				dom::object diffuseTransmissionObject;
				auto diffuseTransmissionError = extensionField.value.get_object().get(diffuseTransmissionObject);
				if (diffuseTransmissionError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& diffuseTransmission = material.diffuseTransmission.emplace();

				double diffuseTransmissionFactor;
				if (auto error = diffuseTransmissionObject["diffuseTransmissionFactor"].get_double().get(diffuseTransmissionFactor); error == SUCCESS) [[likely]] {
					diffuseTransmission.diffuseTransmissionFactor = static_cast<num>(diffuseTransmissionFactor);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				TextureInfo diffuseTransmissionTexture;
				if (auto error = parseTextureInfo(diffuseTransmissionObject, "diffuseTransmissionTexture", &diffuseTransmissionTexture, config.extensions); error == Error::None) [[likely]] {
					diffuseTransmission.diffuseTransmissionTexture = std::move(diffuseTransmissionTexture);
				} else if (error != Error::MissingField) {
					return error;
				}

				dom::array diffuseTransmissionColorFactor;
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
						diffuseTransmission.diffuseTransmissionColorFactor[i++] = static_cast<num>(value);
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

				dom::object sheenObject;
				auto sheenError = extensionField.value.get_object().get(sheenObject);
				if (sheenError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& sheen = material.sheen.emplace();

				dom::array sheenColorFactor;
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
						sheen.sheenColorFactor[i++] = static_cast<num>(value);
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
					sheen.sheenRoughnessFactor = static_cast<num>(sheenRoughnessFactor);
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

				dom::object specularObject;
				auto specularError = extensionField.value.get_object().get(specularObject);
				if (specularError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& specular = material.specular.emplace();

				double specularFactor;
				if (auto error = specularObject["specularFactor"].get_double().get(specularFactor); error ==
																									SUCCESS) {
					specular.specularFactor = static_cast<num>(specularFactor);
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

				dom::array specularColorFactor;
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
						specular.specularColorFactor[i++] = static_cast<num>(value);
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

				dom::object transmissionObject;
				auto transmissionError = extensionField.value.get_object().get(transmissionObject);
				if (transmissionError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& transmission = material.transmission.emplace();

				double transmissionFactor;
				if (auto error = transmissionObject["transmissionFactor"].get_double().get(transmissionFactor);
						error == SUCCESS) [[likely]] {
					transmission.transmissionFactor = static_cast<num>(transmissionFactor);
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

				dom::object unlitObject;
				auto unlitError = extensionField.value.get_object().get(unlitObject);
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

				dom::object volumeObject;
				auto volumeError = extensionField.value.get_object().get(volumeObject);
				if (volumeError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				auto& volume = material.volume.emplace();

				double thicknessFactor;
				if (auto error = volumeObject["thicknessFactor"].get_double().get(thicknessFactor); error == SUCCESS) [[likely]] {
					volume.thicknessFactor = static_cast<num>(thicknessFactor);
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
					volume.attenuationDistance = static_cast<num>(attenuationDistance);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				dom::array attenuationColor;
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
						(volume.attenuationColor)[i++] = static_cast<num>(value);
					}
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}
				break;
			}
			case force_consteval<crc32c(extensions::MSFT_packing_normalRoughnessMetallic)>: {
				if (!hasBit(config.extensions, Extensions::MSFT_packing_normalRoughnessMetallic))
					break;

				dom::object normalRoughnessMetallic;
				if (extensionField.value.get_object().get(normalRoughnessMetallic) != SUCCESS) [[unlikely]] {
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

				dom::object occlusionRoughnessMetallic;
				if (extensionField.value.get_object().get(occlusionRoughnessMetallic) != SUCCESS) [[unlikely]] {
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

				dom::object specularGlossinessObject;
				auto specularGlossinessError = extensionField.value.get_object().get(specularGlossinessObject);
				if (specularGlossinessError != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				auto& specularGlossiness = material.specularGlossiness.emplace();

				dom::array diffuseFactor;
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
						specularGlossiness.diffuseFactor[i++] = static_cast<num>(value);
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

				dom::array specularFactor;
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
						specularGlossiness.specularFactor[i++] = static_cast<num>(value);
					}
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double glossinessFactor;
				if (auto error = specularGlossinessObject["glossinessFactor"].get_double().get(glossinessFactor); error == SUCCESS) [[likely]] {
					specularGlossiness.glossinessFactor = static_cast<num>(glossinessFactor);
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

fg::Error fg::Parser::parseMaterials(simdjson::dom::array& materials, Asset& asset) {
	using namespace simdjson;

	asset.materials.reserve(materials.size());
	for (auto materialValue : materials) {
		dom::object materialObject;
		if (materialValue.get_object().get(materialObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		Material material {};

		dom::array emissiveFactor;
		if (auto error = materialObject["emissiveFactor"].get_array().get(emissiveFactor); error == SUCCESS) [[likely]] {
			if (emissiveFactor.size() != 3) {
				return Error::InvalidGltf;
			}
			for (auto i = 0U; i < 3; ++i) {
				double val;
				if (emissiveFactor.at(i).get_double().get(val) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				material.emissiveFactor[i] = static_cast<num>(val);
			}
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

		dom::object pbrMetallicRoughness;
		if (materialObject["pbrMetallicRoughness"].get_object().get(pbrMetallicRoughness) == SUCCESS) [[likely]] {
			PBRData pbr {};

			dom::array baseColorFactor;
			if (pbrMetallicRoughness["baseColorFactor"].get_array().get(baseColorFactor) == SUCCESS) [[likely]] {
				for (auto i = 0U; i < 4; ++i) {
					double val;
					if (baseColorFactor.at(i).get_double().get(val) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
					pbr.baseColorFactor[i] = static_cast<num>(val);
				}
			}

			double factor;
			if (auto error = pbrMetallicRoughness["metallicFactor"].get_double().get(factor); error == SUCCESS) [[likely]] {
				pbr.metallicFactor = static_cast<num>(factor);
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
			if (auto error = pbrMetallicRoughness["roughnessFactor"].get_double().get(factor); error == SUCCESS) [[likely]] {
				pbr.roughnessFactor = static_cast<num>(factor);
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
			material.alphaCutoff = static_cast<num>(alphaCutoff);
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

		dom::object extensionsObject;
		if (auto extensionError = materialObject["extensions"].get_object().get(extensionsObject); extensionError == SUCCESS) {
			if (const auto err = parseMaterialExtensions(extensionsObject, material); err != Error::None)
				return err;
		} else if (extensionError != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidJson;
		}

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = materialObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.materials.size(), Category::Materials, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
		}

		asset.materials.emplace_back(std::move(material));
	}

	return Error::None;
}

fastgltf::Error fg::Parser::parsePrimitiveExtensions(const simdjson::dom::object& object, Primitive& primitive) {
	using namespace simdjson;

	for (auto extension : object) {
		switch (crc32c(extension.key)) {
			case force_consteval<crc32c(extensions::KHR_materials_variants)>: {
				if (!hasBit(config.extensions, Extensions::KHR_materials_variants))
					break;

				dom::object variantObject;
				if (auto variantsError = extension.value.get_object().get(variantObject); variantsError != SUCCESS) {
					return Error::InvalidGltf;
				}

				dom::array mappingsArray;
				if (variantObject["mappings"].get_array().get(mappingsArray) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				for (auto mapping : mappingsArray) {
					dom::object mappingObject;
					if (mapping.get_object().get(mappingObject) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}

					std::uint64_t materialIndex;
					if (mappingObject["material"].get_uint64().get(materialIndex) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}

					dom::array variantsArray;
					if (mappingObject["variants"].get_array().get(variantsArray) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
					for (auto variant : variantsArray) {
						std::uint64_t variantIndex;
						if (variant.get_uint64().get(variantIndex) != SUCCESS) [[unlikely]] {
							return Error::InvalidGltf;
						}

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

				dom::object dracoObject;
				if (auto dracoError = extension.value.get_object().get(dracoObject); dracoError != SUCCESS) {
					return Error::InvalidGltf;
				}

				auto& dracoCompression = primitive.dracoCompression.emplace();

				std::uint64_t value;
				if (auto error = dracoObject["bufferView"].get_uint64().get(value); error != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				dracoCompression.bufferView = static_cast<std::size_t>(value);

				dom::object attributesObject;
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

fg::Error fg::Parser::parseMeshes(simdjson::dom::array& meshes, Asset& asset) {
	using namespace simdjson;

	asset.meshes.reserve(meshes.size());
	for (auto meshValue : meshes) {
		// Required fields: "primitives"
		dom::object meshObject;
		if (meshValue.get_object().get(meshObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		Mesh mesh {};

		dom::array array;
		auto meshError = getJsonArray(meshObject, "primitives", &array);
		if (meshError != Error::None) {
			return meshError == Error::MissingField ? Error::InvalidGltf : meshError;
		}

		mesh.primitives.reserve(array.size());
		for (auto primitiveValue : array) {
			// Required fields: "attributes"
			Primitive primitive {};
			dom::object primitiveObject;
			if (primitiveValue.get_object().get(primitiveObject) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}

			dom::object attributesObject;
			if (primitiveObject["attributes"].get_object().get(attributesObject) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			if (auto attributesError = parseAttributes(attributesObject, primitive.attributes); attributesError != Error::None) {
				return attributesError;
			}

			dom::array targets;
			if (primitiveObject["targets"].get_array().get(targets) == SUCCESS) [[likely]] {
				primitive.targets.reserve(targets.size());
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
				primitive.type = static_cast<PrimitiveType>(value);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			if (auto error = primitiveObject["indices"].get_uint64().get(value); error == SUCCESS) [[likely]] {
				primitive.indicesAccessor = static_cast<std::size_t>(value);
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}

			if (auto error = primitiveObject["material"].get_uint64().get(value); error == SUCCESS) [[likely]] {
				primitive.materialIndex = static_cast<std::size_t>(value);
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}

			if (hasBit(config.extensions, Extensions::KHR_materials_variants) || hasBit(config.extensions, Extensions::KHR_draco_mesh_compression)) {
				dom::object extensionsObject;
				if (auto error = primitiveObject["extensions"].get_object().get(extensionsObject); error == SUCCESS) {
					if (auto extensionError = parsePrimitiveExtensions(extensionsObject, primitive); extensionError != Error::None)
						return extensionError;
				} else if (error != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
			}

			mesh.primitives.emplace_back(std::move(primitive));
		}

		if (meshError = getJsonArray(meshObject, "weights", &array); meshError == Error::None) [[likely]] {
			mesh.weights.reserve(array.size());
			for (auto weightValue : array) {
				double val;
				if (weightValue.get_double().get(val) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				mesh.weights.emplace_back(static_cast<num>(val));
			}
		} else if (meshError != Error::MissingField && meshError != Error::None) {
			return Error::InvalidGltf;
		}

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = meshObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.meshes.size(), Category::Meshes, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
		}

		std::string_view name;
		if (meshObject["name"].get_string().get(name) == SUCCESS) {
			mesh.name.assign(name);
		}

		asset.meshes.emplace_back(std::move(mesh));
	}

	return Error::None;
}

fg::Error fg::Parser::parseNodes(simdjson::dom::array& nodes, Asset& asset) {
	using namespace simdjson;

	asset.nodes.reserve(nodes.size());
	for (auto nodeValue : nodes) {
		Node node {};
		dom::object nodeObject;
		if (nodeValue.get_object().get(nodeObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t index;
		if (auto error = nodeObject["mesh"].get_uint64().get(index); error == SUCCESS) {
			node.meshIndex = static_cast<std::size_t>(index);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}
		if (auto error = nodeObject["skin"].get_uint64().get(index); error == SUCCESS) {
			node.skinIndex = static_cast<std::size_t>(index);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}
		if (auto error = nodeObject["camera"].get_uint64().get(index); error == SUCCESS) {
			node.cameraIndex = static_cast<std::size_t>(index);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::array array;
		auto childError = getJsonArray(nodeObject, "children", &array);
		if (childError == Error::None) [[likely]] {
			node.children.reserve(array.size());
			for (auto childValue : array) {
				if (childValue.get_uint64().get(index) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				node.children.emplace_back(static_cast<std::size_t>(index));
			}
		} else if (childError != Error::MissingField) {
			return childError;
		}

		auto weightsError = getJsonArray(nodeObject, "weights", &array);
		if (weightsError != Error::MissingField) {
			if (weightsError == Error::None) {
				node.weights.reserve(array.size());
				for (auto weightValue : array) {
					double val;
					if (weightValue.get_double().get(val) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
					node.weights.emplace_back(static_cast<num>(val));
				}
			} else {
				return Error::InvalidGltf;
			}
		}

		auto error = nodeObject["matrix"].get_array().get(array);
		if (error == SUCCESS) [[likely]] {
			if (array.size() != 16) [[unlikely]] {
				return Error::InvalidGltf;
			}
			math::fmat4x4 transformMatrix;
			std::size_t i = 0, j = 0;
			for (auto num : array) {
				double val;
				if (num.get_double().get(val) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				transformMatrix.col(i)[j++] = static_cast<fastgltf::num>(val);
				if (j == 4) {
					j = 0;
					++i;
				}
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
				if (array.size() != 3) [[unlikely]] {
					return Error::InvalidGltf;
				}
				auto i = 0U;
				for (auto num : array) {
					double val;
					if (num.get_double().get(val) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
					trs.scale[i] = static_cast<fastgltf::num>(val);
					++i;
				}
			} else if (scaleError != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidJson;
			}

			if (auto translationError = nodeObject["translation"].get_array().get(array); translationError == SUCCESS) [[likely]] {
				if (array.size() != 3) [[unlikely]] {
					return Error::InvalidGltf;
				}
				auto i = 0U;
				for (auto num : array) {
					double val;
					if (num.get_double().get(val) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
					trs.translation[i] = static_cast<fastgltf::num>(val);
					++i;
				}
			} else if (translationError != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			if (auto rotationError = nodeObject["rotation"].get_array().get(array); rotationError == SUCCESS) [[likely]] {
				if (array.size() != 4) [[unlikely]] {
					return Error::InvalidGltf;
				}
				auto i = 0U;
				for (auto num : array) {
					double val;
					if (num.get_double().get(val) != SUCCESS) [[unlikely]] {
						return Error::InvalidGltf;
					}
					trs.rotation[i] = static_cast<fastgltf::num>(val);
					++i;
				}
			} else if (rotationError != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			node.transform = trs;
		}

		dom::object extensionsObject;
		if (nodeObject["extensions"].get_object().get(extensionsObject) == SUCCESS) [[likely]] {
			if (hasBit(config.extensions, Extensions::KHR_lights_punctual)) {
				dom::object lightsObject;
				if (extensionsObject[extensions::KHR_lights_punctual].get_object().get(lightsObject) == SUCCESS) [[likely]] {
					std::uint64_t light;
					if (auto lightError = lightsObject["light"].get_uint64().get(light); lightError == SUCCESS) [[likely]] {
						node.lightIndex = static_cast<std::size_t>(light);
					} else {
						return lightError == NO_SUCH_FIELD || lightError == INCORRECT_TYPE ? Error::InvalidGltf : Error::InvalidJson;
					}
				} else if (error != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
			}

			if (hasBit(config.extensions, Extensions::EXT_mesh_gpu_instancing)) {
				dom::object gpuInstancingObject;
				if (auto instancingError = extensionsObject[extensions::EXT_mesh_gpu_instancing].get_object().get(gpuInstancingObject); instancingError == SUCCESS) [[likely]] {
					dom::object attributesObject;

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

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
			if (hasBit(config.extensions, Extensions::KHR_physics_rigid_bodies)) {
				dom::object physicsRigidBodiesObject;
				if (auto rigidBodiesError = extensionsObject[extensions::KHR_physics_rigid_bodies].get_object().get(physicsRigidBodiesObject); rigidBodiesError == SUCCESS) [[likely]] {
					const auto rigidBodyError = parsePhysicsRigidBody(physicsRigidBodiesObject, node);
					if (rigidBodyError != Error::None) {
						return rigidBodyError;
					}
				} else if (rigidBodiesError != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
			}
#endif

			if (hasBit(config.extensions, Extensions::KHR_node_visibility)) {
				dom::object nodeVisibilityObject;
				if (auto nodeVisibilityError = extensionsObject[extensions::KHR_node_visibility].get_object().get(nodeVisibilityObject); nodeVisibilityError == SUCCESS) {
					if (nodeVisibilityObject["visible"].get_bool().get(node.visible) != SUCCESS) {
						return Error::InvalidGltf;
					}
				} else if (nodeVisibilityError != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
			}

			if (hasBit(config.extensions, Extensions::KHR_node_selectability)) {
				dom::object nodeSelectabilityObject;
				if (auto nodeSelectabilityError = extensionsObject[extensions::KHR_node_selectability].get_object().get(nodeSelectabilityObject); nodeSelectabilityError == SUCCESS) {
					if (nodeSelectabilityObject["selectable"].get_bool().get(node.selectable) != SUCCESS) {
						return Error::InvalidGltf;
					}
				} else if (nodeSelectabilityError != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
			}

			if (hasBit(config.extensions, Extensions::KHR_node_hoverability)) {
				dom::object nodeHoverabilityObject;
				if (auto nodeHoverabilityError = extensionsObject[extensions::KHR_node_hoverability].get_object().get(nodeHoverabilityObject); nodeHoverabilityError == SUCCESS) {
					if (nodeHoverabilityObject["hoverable"].get_bool().get(node.hoverable) != SUCCESS) {
						return Error::InvalidGltf;
					}
				} else if (nodeHoverabilityError != NO_SUCH_FIELD) {
					return Error::InvalidGltf;
				}
			}
		}

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = nodeObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.nodes.size(), Category::Nodes, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
		}

		std::string_view name;
		if (nodeObject["name"].get_string().get(name) == SUCCESS) {
			node.name.assign(name);
		}

		asset.nodes.emplace_back(std::move(node));
	}

	return Error::None;
}

fg::Error fg::Parser::parseSamplers(const simdjson::dom::array& samplers, Asset& asset) {
	using namespace simdjson;

	std::uint64_t number;
	asset.samplers.reserve(samplers.size());
	for (auto samplerValue : samplers) {
		Sampler sampler {};
		dom::object samplerObject;
		if (samplerValue.get_object().get(samplerObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (auto error = samplerObject["magFilter"].get_uint64().get(number); error == SUCCESS) [[likely]] {
			sampler.magFilter = static_cast<Filter>(number);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}
		if (auto error = samplerObject["minFilter"].get_uint64().get(number); error == SUCCESS) [[likely]] {
			sampler.minFilter = static_cast<Filter>(number);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (auto error = samplerObject["wrapS"].get_uint64().get(number); error == SUCCESS) [[likely]] {
			sampler.wrapS = static_cast<Wrap>(number);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}
		if (auto error = samplerObject["wrapT"].get_uint64().get(number); error == SUCCESS) [[likely]] {
			sampler.wrapT = static_cast<Wrap>(number);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = samplerObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.samplers.size(), Category::Samplers, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
		}

		std::string_view name;
		if (samplerObject["name"].get_string().get(name) == SUCCESS) {
			sampler.name.assign(name);
		}

		asset.samplers.emplace_back(std::move(sampler));
	}

	return Error::None;
}

fg::Error fg::Parser::parseScenes(const simdjson::dom::array& scenes, Asset& asset) {
	using namespace simdjson;

	asset.scenes.reserve(scenes.size());
	for (auto sceneValue : scenes) {
		// The scene object can be completely empty
		Scene scene {};
		dom::object sceneObject;
		if (sceneValue.get_object().get(sceneObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::string_view name;
		if (sceneObject["name"].get_string().get(name) == SUCCESS) {
			scene.name.assign(name);
		}

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = sceneObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.scenes.size(), Category::Scenes, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
		}

		// Parse the array of nodes.
		dom::array nodes;
		auto nodeError = getJsonArray(sceneObject, "nodes", &nodes);
		if (nodeError == Error::None) [[likely]] {
			scene.nodeIndices.reserve(nodes.size());
			for (auto nodeValue : nodes) {
				std::uint64_t index;
				if (nodeValue.get_uint64().get(index) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				scene.nodeIndices.emplace_back(static_cast<std::size_t>(index));
			}

			asset.scenes.emplace_back(std::move(scene));
		} else if (nodeError != Error::MissingField) {
			return nodeError;
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parseSkins(const simdjson::dom::array& skins, Asset& asset) {
	using namespace simdjson;

	asset.skins.reserve(skins.size());
	for (auto skinValue : skins) {
		Skin skin {};
		dom::object skinObject;
		if (skinValue.get_object().get(skinObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t index;
		if (auto error = skinObject["inverseBindMatrices"].get_uint64().get(index); error == SUCCESS) [[likely]] {
			skin.inverseBindMatrices = static_cast<std::size_t>(index);
		} else if (error != NO_SUCH_FIELD) {
			return Error::InvalidGltf;
		}
		if (auto error = skinObject["skeleton"].get_uint64().get(index); error == SUCCESS) [[likely]] {
			skin.skeleton = static_cast<std::size_t>(index);
		} else if (error != NO_SUCH_FIELD) {
			return Error::InvalidGltf;
		}

		dom::array jointsArray;
		if (skinObject["joints"].get_array().get(jointsArray) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}
		skin.joints.reserve(jointsArray.size());
		for (auto jointValue : jointsArray) {
			if (jointValue.get_uint64().get(index) != SUCCESS) [[unlikely]] {
				return Error::InvalidGltf;
			}
			skin.joints.emplace_back(index);
		}

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = skinObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.skins.size(), Category::Skins, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
		}

		std::string_view name;
		if (skinObject["name"].get_string().get(name) == SUCCESS) {
			skin.name.assign(name);
		}
		asset.skins.emplace_back(std::move(skin));
	}

	return Error::None;
}

fg::Error fg::Parser::parseTextures(const simdjson::dom::array& textures, Asset& asset) {
	using namespace simdjson;

	asset.textures.reserve(textures.size());
	for (auto textureValue : textures) {
		Texture texture {};
		dom::object textureObject;
		if (textureValue.get_object().get(textureObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t sourceIndex;
		if (auto error = textureObject["source"].get_uint64().get(sourceIndex); error == SUCCESS) [[likely]] {
			texture.imageIndex = static_cast<std::size_t>(sourceIndex);
		} else if (error != NO_SUCH_FIELD) {
			return Error::InvalidGltf;
		}

		dom::object extensionsObject;
		if (auto error = textureObject["extensions"].get_object().get(extensionsObject); error == SUCCESS) [[likely]] {
			if (!parseTextureExtensions(texture, extensionsObject, config.extensions)) {
				return Error::InvalidGltf;
			}
		} else if (error != NO_SUCH_FIELD) {
			return Error::InvalidGltf;
		}

		// The index of the sampler used by this texture. When undefined, a sampler with
		// repeat wrapping and auto filtering SHOULD be used.
		std::uint64_t samplerIndex;
		if (auto error = textureObject["sampler"].get_uint64().get(samplerIndex); error == SUCCESS) [[likely]] {
			texture.samplerIndex = static_cast<std::size_t>(samplerIndex);
		} else if (error != NO_SUCH_FIELD) {
			return Error::InvalidGltf;
		}

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = textureObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.textures.size(), Category::Textures, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
		}

		std::string_view name;
		if (textureObject["name"].get_string().get(name) == SUCCESS) {
			texture.name.assign(name);
		}

		asset.textures.emplace_back(std::move(texture));
	}

	return Error::None;
}

#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
fg::Error fg::Parser::parseShapes(const simdjson::dom::array& shapes, Asset& asset) {
	using namespace simdjson;

	asset.shapes.reserve(shapes.size());
	for (auto shapeValue : shapes) {
		auto& shape = asset.shapes.emplace_back();

		dom::object shapeObject;
		if (shapeValue.get_object().get(shapeObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::string_view shapeTypeName;
		if (shapeObject["type"].get_string().get(shapeTypeName) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::object sphereObject;
		if (auto error = shapeObject["sphere"].get_object().get(sphereObject); error == SUCCESS) {
			if (shapeTypeName != "sphere") [[unlikely]] {
				return Error::InvalidGltf;
			}

			double radius;
			if (error = sphereObject["radius"].get_double().get(radius); error == SUCCESS) {
				shape = SphereShape{ static_cast<num>(radius) };
			} else if(error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		} else if(error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::object boxObject;
		if (auto error = shapeObject["box"].get_object().get(boxObject); error == SUCCESS) {
			if (shapeTypeName != "box") [[unlikely]] {
				return Error::InvalidGltf;
			}

			dom::array sizeArray;
			if (error = boxObject["size"].get_array().get(sizeArray); error == SUCCESS) {
				if (sizeArray.size() != 3) {
					return Error::InvalidGltf;
				}

				auto& box = shape.emplace<BoxShape>();
				auto curIndex = 0;
				for (auto sizeNum : sizeArray) {
					double value;
					if (sizeNum.get_double().get(value) == SUCCESS) [[likely]] {
						box.size[curIndex] = static_cast<num>(value);
					} else {
						return Error::InvalidGltf;
					}

					curIndex++;
				}
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::object capsuleObject;
		if (auto error = shapeObject["capsule"].get_object().get(capsuleObject); error == SUCCESS) {
			if (shapeTypeName != "capsule") [[unlikely]] {
				return Error::InvalidGltf;
			}

			auto& capsule = shape.emplace<CapsuleShape>();

			double height;
			if (error = capsuleObject["height"].get_double().get(height); error == SUCCESS) {
				capsule.height = static_cast<num>(height);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			double radiusBottom;
			if (error = capsuleObject["radiusBottom"].get_double().get(radiusBottom); error == SUCCESS) {
				capsule.radiusBottom = static_cast<num>(radiusBottom);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			double radiusTop;
			if (error = capsuleObject["radiusTop"].get_double().get(radiusTop); error == SUCCESS) {
				capsule.radiusTop = static_cast<num>(radiusTop);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::object cylinderObject;
		if (auto error = shapeObject["cylinder"].get_object().get(cylinderObject); error == SUCCESS) {
			if (shapeTypeName != "cylinder") [[unlikely]] {
				return Error::InvalidGltf;
			}

			auto& cylinder = shape.emplace<CylinderShape>();

			double height;
			if (error = cylinderObject["height"].get_double().get(height); error == SUCCESS) {
				cylinder.height = static_cast<num>(height);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			double radiusBottom;
			if (error = cylinderObject["radiusBottom"].get_double().get(radiusBottom); error == SUCCESS) {
				cylinder.radiusBottom = static_cast<num>(radiusBottom);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			double radiusTop;
			if (error = cylinderObject["radiusTop"].get_double().get(radiusTop); error == SUCCESS) {
				cylinder.radiusTop = static_cast<num>(radiusTop);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::object planeObject;
		if (auto error = shapeObject["plane"].get_object().get(planeObject); error == SUCCESS) {
			if (shapeTypeName != "plane") [[unlikely]] {
				return Error::InvalidGltf;
			}

			auto& plane = shape.emplace<PlaneShape>();

			double sizeX;
			if (error = planeObject["sizeX"].get_double().get(sizeX); error == SUCCESS) {
				plane.sizeX = static_cast<num>(sizeX);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

			double sizeZ;
			if (error = planeObject["sizeZ"].get_double().get(sizeZ); error == SUCCESS) {
				plane.sizeZ = static_cast<num>(sizeZ);
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

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = shapeObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.shapes.size() - 1, Category::Shapes, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		}
	}

	return Error::None;
}
#endif

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
fg::Error fg::Parser::parsePhysicsMaterials(const simdjson::dom::array& physicsMaterials, Asset& asset) {
	using namespace simdjson;

	asset.physicsMaterials.reserve(physicsMaterials.size());
	for (auto materialValue : physicsMaterials) {
		PhysicsMaterial& material = asset.physicsMaterials.emplace_back();
		dom::object materialObject;
		if (materialValue.get_object().get(materialObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		double staticFriction;
		if (auto error = materialObject["staticFriction"].get_double().get(staticFriction); error == SUCCESS) {
		   material.staticFriction = static_cast<num>(staticFriction);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		double dynamicFriction;
		if (auto error = materialObject["dynamicFriction"].get_double().get(dynamicFriction); error == SUCCESS) {
		   material.dynamicFriction = static_cast<num>(dynamicFriction);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		double restitution;
		if (auto error = materialObject["restitution"].get_double().get(restitution); error == SUCCESS) {
		   material.restitution = static_cast<num>(restitution);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::string_view frictionCombine;
		if (auto error = materialObject["frictionCombine"].get_string().get(frictionCombine); error == SUCCESS) {
			material.frictionCombine = internal::getCombineMode(frictionCombine);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::string_view restitutionCombine;
		if (auto error = materialObject["restitutionCombine"].get_string().get(restitutionCombine); error == SUCCESS) {
		   material.restitutionCombine = internal::getCombineMode(restitutionCombine);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = materialObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.physicsMaterials.size() - 1, Category::PhysicsMaterials, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parseCollisionFilters(const simdjson::dom::array& collisionFilters, Asset& asset) {
	using namespace simdjson;

	asset.collisionFilters.reserve(collisionFilters.size());
	for (auto collisionFilterValue : collisionFilters) {
		auto& collisionFilter = asset.collisionFilters.emplace_back();

		dom::object collisionFilterObject;
		if (collisionFilterValue.get_object().get(collisionFilterObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::array collisionSystems;
		if (auto error = collisionFilterObject["collisionSystems"].get_array().get(collisionSystems); error == SUCCESS) {
			collisionFilter.collisionSystems.reserve(collisionSystems.size());
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

		dom::array collideWithSystems;
		if (auto error = collisionFilterObject["collideWithSystems"].get_array().get(collideWithSystems); error == SUCCESS) {
			collisionFilter.collideWithSystems.reserve(collideWithSystems.size());
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

		dom::array notCollideWithSystems;
		if (auto error = collisionFilterObject["notCollideWithSystems"].get_array().get(notCollideWithSystems); error == SUCCESS) {
			collisionFilter.notCollideWithSystems.reserve(notCollideWithSystems.size());
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

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = collisionFilterObject["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.collisionFilters.size() - 1, Category::CollisionFilters, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parsePhysicsJoints(const simdjson::dom::array& physicsJoints, Asset& asset) {
	using namespace simdjson;

	asset.physicsJoints.reserve(physicsJoints.size());
	for (auto physicsJointValue : physicsJoints) {
		auto& physicsJoint = asset.physicsJoints.emplace_back();

		dom::object physicsJointObject;
		if (physicsJointValue.get_object().get(physicsJointObject) != SUCCESS) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::array limitsArray;
		if (physicsJointValue["limits"].get_array().get(limitsArray) == SUCCESS) {
			physicsJoint.limits.reserve(limitsArray.size());
			for (auto limitValue : limitsArray) {
				auto& limit = physicsJoint.limits.emplace_back();
				dom::object limitObject;
				if (limitValue.get_object().get(limitObject) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				if(limitObject["linearAxes"].error() == SUCCESS && limitObject["angularAxes"].error() == SUCCESS) [[unlikely]] {
					// A limit may only have one of these
					return Error::InvalidGltf;
				}

				if (limitObject["linearAxes"].error() != SUCCESS && limitObject["angularAxes"].error() != SUCCESS) [[unlikely]] {
					// A limit MUST have one of these
					return Error::InvalidGltf;
				}

				double limitMin;
				if (auto error = limitObject["min"].get_double().get(limitMin); error == SUCCESS) {
					limit.min = static_cast<num>(limitMin);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double limitMax;
				if (auto error = limitObject["max"].get_double().get(limitMax); error == SUCCESS) {
					limit.max = static_cast<num>(limitMax);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double stiffness;
				if (auto error = limitObject["stiffness"].get_double().get(stiffness); error == SUCCESS) {
					limit.stiffness = static_cast<num>(stiffness);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double damping;
				if (auto error = limitObject["damping"].get_double().get(damping); error == SUCCESS) {
					limit.damping = static_cast<num>(damping);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				dom::array linearAxesArray;
				if (auto error = limitObject["linearAxes"].get_array().get(linearAxesArray); error == SUCCESS) {
					for(auto axisValue : linearAxesArray) {
						if (limit.linearAxes.size() == limit.linearAxes.max_size()) [[unlikely]]
							return Error::InvalidGltf;

						std::uint64_t axis;
						if(axisValue.get_uint64().get(axis) == SUCCESS && axis <= 2) [[likely]] {
							limit.linearAxes.emplace_back(static_cast<uint8_t>(axis));
						} else {
							return Error::InvalidGltf;
						}
					}
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				dom::array angularAxesArray;
				if (auto error = limitObject["angularAxes"].get_array().get(angularAxesArray); error == SUCCESS) {
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

		dom::array driveArray;
		if (auto error = physicsJointValue["drives"].get_array().get(driveArray); error == SUCCESS) {
			physicsJoint.drives.reserve(driveArray.size());
			for (auto driveValue : driveArray) {
				auto& drive = physicsJoint.drives.emplace_back();
				dom::object driveObject;
				if (driveValue.get_object().get(driveObject) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}

				// Type, mode, and axis are required

				std::string_view type;
				if (driveValue["type"].get_string().get(type) == SUCCESS) [[likely]] {
					drive.type = internal::getDriveType(type);
					if (drive.type == DriveType::Invalid) [[unlikely]] {
						return Error::InvalidGltf;
					}
				} else {
					return Error::InvalidGltf;
				}

				std::string_view mode;
				if (driveValue["mode"].get_string().get(mode) == SUCCESS) [[likely]] {
					drive.mode = internal::getDriveMode(mode);
					if (drive.mode == DriveMode::Invalid) [[unlikely]] {
						return Error::InvalidGltf;
					}
				} else {
					return Error::InvalidGltf;
				}

				uint64_t axis;
				if (driveValue["axis"].get_uint64().get(axis) == SUCCESS) [[likely]] {
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
				if (error = driveValue["maxForce"].get_double().get(maxForce); error == SUCCESS) {
					drive.maxForce = static_cast<num>(maxForce);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				// If the drive has positionTarget, it must also have stiffness
				const auto hasPositionTarget = driveValue["positionTarget"].error() == SUCCESS;
				const auto hasStiffness = driveValue["stiffness"].error() == SUCCESS;
				if (hasPositionTarget != hasStiffness) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double positionTarget;
				if (error = driveValue["positionTarget"].get_double().get(positionTarget); error == SUCCESS) {
					drive.positionTarget = static_cast<num>(positionTarget);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double stiffness;
				if (error = driveValue["stiffness"].get_double().get(stiffness); error == SUCCESS) {
					drive.stiffness = static_cast<num>(stiffness);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				// If the drive has velocityTarget, it must also have damping
				const auto hasVelocityTarget = driveValue["velocityTarget"].error() == SUCCESS;
				const auto hasDamping = driveValue["damping"].error() == SUCCESS;
				if (hasVelocityTarget != hasDamping) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double velocityTarget;
				if (error = driveValue["velocityTarget"].get_double().get(velocityTarget); error == SUCCESS) {
					drive.velocityTarget = static_cast<num>(velocityTarget);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}

				double damping;
				if (error = driveValue["damping"].get_double().get(damping); error == SUCCESS) {
					drive.damping = static_cast<num>(damping);
				} else if (error != NO_SUCH_FIELD) [[unlikely]] {
					return Error::InvalidGltf;
				}
			}
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		if (config.extrasCallback != nullptr) {
			dom::object extrasObject;
			if (auto extrasError = physicsJointValue["extras"].get_object().get(extrasObject); extrasError == SUCCESS) {
				config.extrasCallback(&extrasObject, asset.physicsJoints.size() - 1, Category::PhysicsJoints, config.userPointer);
			} else if (extrasError != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}
		}
	}

	return Error::None;
}

fg::Error fg::Parser::parsePhysicsRigidBody(simdjson::dom::object& khr_physics_rigid_bodies, Node& node) {
	using namespace simdjson;

	auto& rigidBody = node.physicsRigidBody.emplace();

	dom::object motionObject;
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
			motion.mass = static_cast<num>(mass);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::array centerOfMassArray;
		if (error = motionObject["centerOfMass"].get_array().get(centerOfMassArray); error == SUCCESS) {
			if (centerOfMassArray.size() != 3) [[unlikely]] {
				return Error::InvalidGltf;
			}

			auto i = 0U;
			for (auto numValue : centerOfMassArray) {
				double val;
				if (numValue.get_double().get(val) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				motion.centerOfMass[i] = static_cast<num>(val);
				++i;
			}
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::array inertiaDiagonalArray;
		if (error = motionObject["inertiaDiagonal"].get_array().get(inertiaDiagonalArray); error == SUCCESS) {
			if (inertiaDiagonalArray.size() != 3) [[unlikely]] {
				return Error::InvalidGltf;
			}

			math::fvec3 inertialDiagonal;

			auto i = 0U;
			for (auto numValue : inertiaDiagonalArray) {
				double val;
				if (numValue.get_double().get(val) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				inertialDiagonal[i] = static_cast<num>(val);
				++i;
			}

			motion.inertialDiagonal = inertialDiagonal;

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::array inertialOrientationArray;
		if (error = motionObject["inertiaOrientation"].get_array().get(inertialOrientationArray); error == SUCCESS) {
			if (inertialOrientationArray.size() != 4) [[unlikely]] {
				return Error::InvalidGltf;
			}

			math::fvec4 inertialOrientation;

			auto i = 0U;
			for (auto numValue : inertialOrientationArray) {
				double val;
				if (numValue.get_double().get(val) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				inertialOrientation[i] = static_cast<num>(val);
				++i;
			}

			motion.inertialOrientation = inertialOrientation;

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::array linearVelocityArray;
		if (error = motionObject["linearVelocity"].get_array().get(linearVelocityArray); error == SUCCESS) {
			if (linearVelocityArray.size() != 3) [[unlikely]] {
				return Error::InvalidGltf;
			}

			auto i = 0U;
			for (auto numValue : linearVelocityArray) {
				double val;
				if (numValue.get_double().get(val) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				motion.linearVelocity[i] = static_cast<num>(val);
				++i;
			}

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::array angularVelocityArray;
		if (error = motionObject["angularVelocity"].get_array().get(angularVelocityArray); error == SUCCESS) {
			if (angularVelocityArray.size() != 3) [[unlikely]] {
				return Error::InvalidGltf;
			}

			auto i = 0U;
			for (auto numValue : angularVelocityArray) {
				double val;
				if (numValue.get_double().get(val) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				motion.angularVelocity[i] = static_cast<num>(val);
				++i;
			}
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		double gravityFactor;
		if (error = motionObject["gravityFactor"].get_double().get(gravityFactor); error == SUCCESS) {
			motion.gravityFactor = static_cast<num>(gravityFactor);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

	} else if (error != NO_SUCH_FIELD) [[unlikely]] {
		return Error::InvalidGltf;
	}

	dom::object colliderObject;
	if (auto error = khr_physics_rigid_bodies["collider"].get_object().get(colliderObject); error == SUCCESS) {
		auto& collider = rigidBody.collider.emplace();

		dom::object geometryObject;
		if (colliderObject["geometry"].get_object().get(geometryObject) == SUCCESS) [[likely]] {
			std::uint64_t shape;
			if (error = geometryObject["shape"].get_uint64().get(shape); error == SUCCESS) [[likely]] {
				collider.geometry.shape = static_cast<std::size_t>(shape);
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
			std::uint64_t geometryMesh;
			if (error = geometryObject["mesh"].get_uint64().get(geometryMesh); error == SUCCESS) {
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
			collider.physicsMaterial = static_cast<std::size_t>(physicsMaterial);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		std::uint64_t collisionFilter;
		if (error = colliderObject["collisionFilter"].get_uint64().get(collisionFilter); error == SUCCESS) {
			collider.collisionFilter = static_cast<std::size_t>(collisionFilter);
		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

	} else if (error != NO_SUCH_FIELD) [[unlikely]] {
		return Error::InvalidGltf;
	}

	dom::object triggerObject;
	if (auto error = khr_physics_rigid_bodies["trigger"].get_object().get(triggerObject); error == SUCCESS) {
		dom::object geometryObject;
		if (error = triggerObject["geometry"].get_object().get(geometryObject); error == SUCCESS) {
			auto& geometryTrigger = rigidBody.trigger.emplace().emplace<GeometryTrigger>();

			std::uint64_t shape;
			if (error = geometryObject["shape"].get_uint64().get(shape); error == SUCCESS) [[likely]] {
				geometryTrigger.geometry.shape = static_cast<std::size_t>(shape);
			} else if (error != NO_SUCH_FIELD) {
				return Error::InvalidGltf;
			}
			std::uint64_t meshIndex;
			if (error = geometryObject["mesh"].get_uint64().get(meshIndex); error == SUCCESS) {
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
				geometryTrigger.collisionFilter = static_cast<std::size_t>(collisionFilter);
			} else if (error != NO_SUCH_FIELD) [[unlikely]] {
				return Error::InvalidGltf;
			}

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}

		dom::array nodes;
		if (error = triggerObject["nodes"].get_array().get(nodes); error == SUCCESS) {
			auto& nodeTrigger = rigidBody.trigger.emplace().emplace<NodeTrigger>();

			nodeTrigger.nodes.resize(nodes.size());
			auto i = 0U;
			for (auto num : nodes) {
				std::uint64_t val;
				if (num.get_uint64().get(val) != SUCCESS) [[unlikely]] {
					return Error::InvalidGltf;
				}
				nodeTrigger.nodes[i] = static_cast<std::size_t>(val);
				++i;
			}

		} else if (error != NO_SUCH_FIELD) [[unlikely]] {
			return Error::InvalidGltf;
		}
	} else if (error != NO_SUCH_FIELD) [[unlikely]] {
		return Error::InvalidGltf;
	}

	dom::object jointObject;
	if (auto error = khr_physics_rigid_bodies["joint"].get_object().get(jointObject); error == SUCCESS) {
		auto& joint = rigidBody.joint.emplace();
		std::uint64_t connectedNodeId;
		if (jointObject["connectedNode"].get_uint64().get(connectedNodeId) == SUCCESS) [[likely]] {
			joint.connectedNode = connectedNodeId;
		} else {
			return Error::InvalidGltf;
		}

		std::uint64_t jointId;
		if (jointObject["joint"].get_uint64().get(jointId) == SUCCESS) [[likely]] {
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
#endif

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
	jsonParser = std::make_unique<simdjson::dom::parser>();
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
	dom::object root;
	if (auto error = jsonParser->parse(view).get(root); error != SUCCESS) [[unlikely]] {
		return Error::InvalidJson;
	}

	return parse(root, categories);
}

fg::Expected<fg::Asset> fg::Parser::loadGltfBinary(GltfDataGetter& data, fs::path _directory, Options _options, Category categories) {
	using namespace simdjson;

	options = _options;
	directory = std::move(_directory);

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
		jsonChunk.chunkLength > data.totalSize() - sizeof(internal::BinaryGltfHeader)) {
		return Error::InvalidGLB;
	}

	// Create a string view of the JSON chunk in the GLB data buffer. The documentation of parse()
	// says the padding can be initialised to anything, apparently. Therefore, this should work.
	auto jsonSpan = data.read(jsonChunk.chunkLength, SIMDJSON_PADDING);
	simdjson::padded_string_view jsonChunkView(reinterpret_cast<const std::uint8_t*>(jsonSpan.data()),
											   jsonChunk.chunkLength,
											   jsonChunk.chunkLength + SIMDJSON_PADDING);

	simdjson::dom::object root;
	if (jsonParser->parse(jsonChunkView).get(root) != SUCCESS) [[unlikely]] {
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

	return parse(root, categories);
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

#ifdef _MSC_VER
#pragma warning(pop)
#endif
