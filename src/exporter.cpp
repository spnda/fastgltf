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

#include <fstream>
#include <ranges>

#include <fastgltf/core.hpp>
#include <fastgltf/glb.hpp>
#include <fastgltf/exporter.hpp>
#include <fastgltf/types.hpp>

#include <simdjson.h>

namespace fg = fastgltf;
namespace fs = std::filesystem;

namespace fastgltf::internal {
	inline auto writeBinaryHeader(const BinaryGltfHeader& header) noexcept {
		std::array<std::byte, sizeof(BinaryGltfHeader)> bytes {};
		writeUint32LE(header.magic, &bytes[offsetof(BinaryGltfHeader, magic)]);
		writeUint32LE(header.version, &bytes[offsetof(BinaryGltfHeader, version)]);
		writeUint32LE(header.length, &bytes[offsetof(BinaryGltfHeader, length)]);
		return bytes;
	}

	inline auto writeBinaryChunk(const BinaryGltfChunk& chunk) noexcept {
		std::array<std::byte, sizeof(BinaryGltfChunk)> bytes {};
		writeUint32LE(chunk.chunkLength, &bytes[offsetof(BinaryGltfChunk, chunkLength)]);
		writeUint32LE(chunk.chunkType, &bytes[offsetof(BinaryGltfChunk, chunkType)]);
		return bytes;
	}
}

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

namespace fastgltf {
	/**
	 * Normalizes the path using lexically_normal and calls generic_string to always use forward slashes.
	 */
	static std::string normalizeAndFormatPath(const fs::path& path) {
		return path.lexically_normal().generic_string();
	}

	template <simdjson::constevalutil::fixed_string key = "">
	struct scoped_object {
		simdjson::builder::string_builder& _builder;

		[[nodiscard]] explicit scoped_object(
			simdjson::builder::string_builder& builder) noexcept : _builder(builder) {
			if constexpr (key.view().size() != 0) {
				_builder.escape_and_append_with_quotes<key>();
				_builder.append_colon();
			}
			_builder.start_object();
		}
		~scoped_object() {
			_builder.end_object();
		}

		scoped_object(const scoped_object&) = delete;
		scoped_object(scoped_object&&) = delete;
		scoped_object& operator=(const scoped_object&) = delete;
		scoped_object& operator=(scoped_object&&) = delete;
	};
	template <simdjson::constevalutil::fixed_string key = "">
	struct scoped_array {
		simdjson::builder::string_builder& _builder;

		[[nodiscard]] explicit scoped_array(
			simdjson::builder::string_builder& builder) noexcept : _builder(builder) {
			if constexpr (key.view().size() != 0) {
				_builder.escape_and_append_with_quotes<key>();
				_builder.append_colon();
			}
			_builder.start_array();
		}
		~scoped_array() {
			_builder.end_array();
		}

		scoped_array(const scoped_array&) = delete;
		scoped_array(scoped_array&&) = delete;
		scoped_array& operator=(const scoped_array&) = delete;
		scoped_array& operator=(scoped_array&&) = delete;
	};

	// appends a comma before every member except the first
	// useful for objects where all members are optional
	struct comma_separator {
		simdjson::builder::string_builder& builder;
		bool first = true;

		void operator()() {
			if (!first)
				builder.append_comma();
			first = false;
		}
	};

	/**
	 * Appends a key with a value that already is valid JSON, without quoting or escaping it.
	 */
	template <simdjson::constevalutil::fixed_string key>
	FASTGLTF_FORCEINLINE static void append_raw_key_value(simdjson::builder::string_builder& builder, std::string_view json) {
		builder.escape_and_append_with_quotes<key>();
		builder.append_colon();
		builder.append_raw(json);
	}

	template <simdjson::constevalutil::fixed_string key, typename T>
	FASTGLTF_FORCEINLINE static void append_key_range(simdjson::builder::string_builder& builder, const T& values) {
		auto arr = scoped_array<key>(builder);
		for (std::size_t i = 0; i < values.size(); ++i) {
			if (i != 0)
				builder.append_comma();
			builder.append(values[i]);
		}
	}

	template <simdjson::constevalutil::fixed_string key, std::ranges::range R>
	FASTGLTF_FORCEINLINE static void append_key_range(simdjson::builder::string_builder& builder, const R& range) {
		builder.escape_and_append_with_quotes<key>();
		builder.append_colon();
		builder.append(range);
	}

	template <simdjson::constevalutil::fixed_string key, auto value>
	FASTGLTF_FORCEINLINE static void append_const_key_value(simdjson::builder::string_builder& builder) {
		// This API only got added in 5.0.4
		if constexpr (requires { builder.template append_key_value<key, value>(); }) {
			builder.template append_key_value<key, value>();
		} else {
			builder.append_key_value<key>(value);
		}
	}
	template <simdjson::constevalutil::fixed_string key, simdjson::constevalutil::fixed_string value>
	FASTGLTF_FORCEINLINE static void append_const_key_value(simdjson::builder::string_builder& builder) {
		// This API only got added in 5.0.4
		if constexpr (requires { builder.template append_key_value<key, value>(); }) {
			builder.template append_key_value<key, value>();
		} else {
			builder.append_key_value<key>(value);
		}
	}

	template <simdjson::constevalutil::fixed_string key>
	static void writeMinMax(simdjson::builder::string_builder& builder, const optional<AccessorBoundsArray>& ref) {
		if (!ref.has_value())
			return; // This is valid, since min/max are only required on specific accessors.

		builder.append_comma();

		auto arr = scoped_array<key>(builder);

		const auto size = ref->size();
		if (ref->isType<double>()) {
			for (std::size_t i = 0; i < size; ++i) {
				builder.append(ref->get<double>(i));

				if (i + 1 < ref->size())
					builder.append_comma();
			}
		} else if (ref->isType<std::int64_t>()) {
			for (std::size_t i = 0; i < size; ++i) {
				builder.append(ref->get<std::int64_t>(i));

				if (i + 1 < ref->size())
					builder.append_comma();
			}
		}
	}

	template <simdjson::constevalutil::fixed_string key, internal::TextureInfoType type = internal::TextureInfoType::Standard>
	static void append_texture_info(simdjson::builder::string_builder& builder, const TextureInfo* info) {
		auto obj = scoped_object<key>(builder);

		builder.append_key_value<"index">(info->textureIndex);
		if (info->texCoordIndex != 0) {
			builder.append_comma();
			builder.append_key_value<"texCoord">(info->texCoordIndex);
		}

		if constexpr (type == internal::TextureInfoType::NormalTexture) {
			builder.append_comma();
			builder.append_key_value<"scale">(reinterpret_cast<const NormalTextureInfo*>(info)->scale);
		} else if constexpr (type == internal::TextureInfoType::OcclusionTexture) {
			builder.append_comma();
			builder.append_key_value<"strength">(reinterpret_cast<const OcclusionTextureInfo*>(info)->strength);
		}

		if (info->transform.has_value()) {
			builder.append_comma();

			auto extensions_obj = scoped_object<"extensions">(builder);
			auto transform_obj = scoped_object<"KHR_texture_transform">(builder);

			const auto& transform = *info->transform;
			comma_separator comma {builder};
			if (transform.uvOffset[0] != 0.0 || transform.uvOffset[1] != 0.0) {
				comma();
				append_key_range<"offset">(builder, transform.uvOffset);
			}
			if (transform.rotation != 0.0) {
				comma();
				builder.append_key_value<"rotation">(transform.rotation);
			}
			if (transform.uvScale[0] != 1.0 || transform.uvScale[1] != 1.0) {
				comma();
				append_key_range<"scale">(builder, transform.uvScale);
			}
			if (transform.texCoordIndex.has_value()) {
				comma();
				builder.append_key_value<"texCoord">(transform.texCoordIndex.value());
			}
		}
	}
} // namespace fastgltf

void fg::Exporter::setBufferPath(fs::path folder) {
	if (!folder.is_relative()) {
		return;
	}
	bufferFolder = std::move(folder);
}

void fg::Exporter::setImagePath(fs::path folder) {
	if (!folder.is_relative()) {
		return;
	}
	imageFolder = std::move(folder);
}

void fg::Exporter::setExtrasWriteCallback(ExtrasWriteCallback* callback) noexcept {
	extrasWriteCallback = callback;
}

void fg::Exporter::setUserPointer(void* pointer) noexcept {
	userPointer = pointer;
}

void fg::Exporter::writeAccessors(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.accessors.empty())
		return;

	auto accessors = scoped_array<"accessors">(builder);

	for (auto it = asset.accessors.begin(); it != asset.accessors.end(); ++it) {
		if (it != asset.accessors.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		if (it->byteOffset != 0) {
			builder.append_key_value<"byteOffset">(it->byteOffset);
			builder.append_comma();
		}

		builder.append_key_value<"count">(it->count);
		builder.append_comma();
		builder.append_key_value<"type">(internal::getAccessorTypeName(it->type));
		builder.append_comma();
		builder.append_key_value<"componentType">(getGLComponentType(it->componentType));

		if (it->normalized) {
			builder.append_comma();
			append_const_key_value<"normalized", true>(builder);
		}

		if (it->bufferViewIndex.has_value()) {
			builder.append_comma();
			builder.append_key_value<"bufferView">(it->bufferViewIndex.value());
		}

		if (it->sparse.has_value()) {
			builder.append_comma();

			auto sparse_obj = scoped_object<"sparse">(builder);
			const auto& sparse = *it->sparse;

			builder.append_key_value<"count">(sparse.count);

			{
				builder.append_comma();
				auto indices = scoped_object<"indices">(builder);
				builder.append_key_value<"bufferView">(sparse.indicesBufferView);
				builder.append_comma();
				if (sparse.indicesByteOffset != 0) {
					builder.append_key_value<"byteOffset">(sparse.indicesByteOffset);
					builder.append_comma();
				}
				builder.append_key_value<"componentType">(getGLComponentType(sparse.indexComponentType));
			}

			{
				builder.append_comma();
				auto indices = scoped_object<"values">(builder);
				builder.append_key_value<"bufferView">(sparse.valuesBufferView);
				if (sparse.valuesByteOffset != 0) {
					builder.append_comma();
					builder.append_key_value<"byteOffset">(sparse.valuesByteOffset);
				}
			}
		}

		writeMinMax<"max">(builder, it->max);
		writeMinMax<"min">(builder, it->min);

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.accessors.begin(), it)), Category::Accessors, userPointer);
			if (extras.has_value()) {
				builder.append_comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}

		if (!it->name.empty()) {
			builder.append_comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

void fg::Exporter::writeAnimations(const Asset& asset, simdjson::builder::string_builder& builder)
{
	if (asset.animations.empty())
		return;

	auto animations = scoped_array<"animations">(builder);

	for (auto it = asset.animations.begin(); it != asset.animations.end(); ++it) {
		if (it != asset.animations.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		{
			auto channels = scoped_array<"channels">(builder);
			for (auto ci = it->channels.begin(); ci != it->channels.end(); ++ci) {
				if (ci != it->channels.begin())
					builder.append_comma();

				auto channel_obj = scoped_object(builder);
				builder.append_key_value<"sampler">(ci->samplerIndex);
				builder.append_comma();

				{
					auto target_obj = scoped_object<"target">(builder);
					if (ci->nodeIndex.has_value()) {
						builder.append_key_value<"node">(ci->nodeIndex.value());
						builder.append_comma();
					}

					std::string_view path;
					switch (ci->path) {
					case fg::AnimationPath::Translation:
						path = "translation";
						break;
					case fg::AnimationPath::Rotation:
						path = "rotation";
						break;
					case fg::AnimationPath::Scale:
						path = "scale";
						break;
					case fg::AnimationPath::Weights:
						path = "weights";
						break;
					}
					builder.append_key_value<"path">(path);
				}
			}
		}
		builder.append_comma();
		{
			auto samplers = scoped_array<"samplers">(builder);
			for (auto si = it->samplers.begin(); si != it->samplers.end(); ++si) {
				if (si != it->samplers.begin())
					builder.append_comma();

				auto sampler_obj = scoped_object(builder);

				builder.append_key_value<"input">(si->inputAccessor);
				builder.append_comma();

				if (si->interpolation != fg::AnimationInterpolation::Linear) {
					if (si->interpolation == fg::AnimationInterpolation::Step) {
						append_const_key_value<"interpolation", "STEP">(builder);
					} else {
						append_const_key_value<"interpolation", "CUBICSPLINE">(builder);
					}
					builder.append_comma();
				}

				builder.append_key_value<"output">(si->outputAccessor);
			}
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.animations.begin(), it)), Category::Animations, userPointer);
			if (extras.has_value()) {
				builder.append_comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}

		if (!it->name.empty()) {
			builder.append_comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

void fg::Exporter::writeBuffers(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.buffers.empty())
		return;

	auto buffers = scoped_array<"buffers">(builder);

	for (auto it = asset.buffers.begin(); it != asset.buffers.end(); ++it) {
		if (it != asset.buffers.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		auto bufferIdx = uabs(std::distance(asset.buffers.begin(), it));
		std::visit(visitor {
			[&](auto&) {
				// Covers BufferView and CustomBuffer.
				errorCode = Error::InvalidGltf;
			},
			[&]([[maybe_unused]] const sources::Array& vector) {
				if (bufferIdx == 0 && exportingBinary) {
					bufferPaths.emplace_back(std::nullopt);
					return;
				}
				auto path = getBufferFilePath(asset, bufferIdx);
				builder.append_key_value<"uri">(fg::normalizeAndFormatPath(path));
				builder.append_comma();
				bufferPaths.emplace_back(path);
			},
			[&]([[maybe_unused]] const sources::Vector& vector) {
				if (bufferIdx == 0 && exportingBinary) {
					bufferPaths.emplace_back(std::nullopt);
					return;
				}
				auto path = getBufferFilePath(asset, bufferIdx);
				builder.append_key_value<"uri">(fg::normalizeAndFormatPath(path));
				builder.append_comma();
				bufferPaths.emplace_back(path);
			},
			[&]([[maybe_unused]] const sources::ByteView& view) {
				if (bufferIdx == 0 && exportingBinary) {
					bufferPaths.emplace_back(std::nullopt);
					return;
				}
				auto path = getBufferFilePath(asset, bufferIdx);
				builder.append_key_value<"uri">(fg::normalizeAndFormatPath(path));
				builder.append_comma();
				bufferPaths.emplace_back(path);
			},
			[&](const sources::URI& uri) {
				builder.append_key_value<"uri">(uri.uri->string());
				builder.append_comma();
				bufferPaths.emplace_back(std::nullopt);
			},
			[&]([[maybe_unused]] const sources::Fallback& fallback) {
				{
					auto extensions_obj = scoped_object<"extensions">(builder);
					auto meshopt_obj = scoped_object<"EXT_meshopt_compression">(builder);
					append_const_key_value<"fallback", true>(builder);
				}
				builder.append_comma();
				bufferPaths.emplace_back(std::nullopt);
			},
		}, it->data);

		builder.append_key_value<"byteLength">(it->byteLength);

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.buffers.begin(), it)), Category::Buffers, userPointer);
			if (extras.has_value()) {
				builder.append_comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}

		if (!it->name.empty()) {
			builder.append_comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

void fg::Exporter::writeBufferViews(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.bufferViews.empty())
		return;

	auto bufferViews = scoped_array<"bufferViews">(builder);

	for (auto it = asset.bufferViews.begin(); it != asset.bufferViews.end(); ++it) {
		if (it != asset.bufferViews.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		builder.append_key_value<"buffer">(it->bufferIndex);
		builder.append_comma();
		builder.append_key_value<"byteLength">(it->byteLength);

		if (it->byteOffset != 0) {
			builder.append_comma();
			builder.append_key_value<"byteOffset">(it->byteOffset);
		}

		if (it->byteStride.has_value()) {
			builder.append_comma();
			builder.append_key_value<"byteStride">(it->byteStride.value());
		}

		if (it->target.has_value()) {
			builder.append_comma();
			builder.append_key_value<"target">(to_underlying(it->target.value()));
		}

		if (it->meshoptCompression.has_value()) {
			builder.append_comma();

			auto extensions = scoped_object<"extensions">(builder);
			auto meshopt_object = scoped_object<"EXT_meshopt_compression">(builder);

			const auto& meshopt = *it->meshoptCompression;
			builder.append_key_value<"buffer">(meshopt.bufferIndex);

			if (meshopt.byteOffset != 0) {
				builder.append_comma();
				builder.append_key_value<"byteOffset">(meshopt.byteOffset);
			}

			builder.append_comma();
			builder.append_key_value<"byteLength">(meshopt.byteLength);
			builder.append_comma();
			builder.append_key_value<"byteStride">(meshopt.byteStride);
			builder.append_comma();
			builder.append_key_value<"count">(meshopt.count);

			{
				builder.append_comma();
				builder.escape_and_append_with_quotes<"mode">();
				builder.append_colon();
				switch (meshopt.mode) {
					case MeshoptCompressionMode::Attributes:
						builder.escape_and_append_with_quotes<"ATTRIBUTES">();
						break;
					case MeshoptCompressionMode::Triangles:
						builder.escape_and_append_with_quotes<"TRIANGLES">();
						break;
					case MeshoptCompressionMode::Indices:
						builder.escape_and_append_with_quotes<"INDICES">();
						break;
				}
			}

			if (meshopt.filter != MeshoptCompressionFilter::None) {
				builder.append_comma();
				builder.escape_and_append_with_quotes<"filter">();
				builder.append_colon();
				switch (meshopt.filter) {
					case MeshoptCompressionFilter::None:
						builder.escape_and_append_with_quotes<"NONE">();
						break;
					case MeshoptCompressionFilter::Exponential:
						builder.escape_and_append_with_quotes<"EXPONENTIAL">();
						break;
					case MeshoptCompressionFilter::Quaternion:
						builder.escape_and_append_with_quotes<"QUATERNION">();
						break;
					case MeshoptCompressionFilter::Octahedral:
						builder.escape_and_append_with_quotes<"OCTAHEDRAL">();
						break;
					case MeshoptCompressionFilter::Color:
						builder.escape_and_append_with_quotes<"COLOR">();
						break;
				}
			}
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.bufferViews.begin(), it)), Category::BufferViews, userPointer);
			if (extras.has_value()) {
				builder.append_comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}

		if (!it->name.empty()) {
			builder.append_comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

void fg::Exporter::writeCameras(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.cameras.empty())
		return;

	auto cameras = scoped_array<"cameras">(builder);

	for (auto it = asset.cameras.begin(); it != asset.cameras.end(); ++it) {
		if (it != asset.cameras.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		std::visit(visitor {
			[](auto&) {},
			[&](const Camera::Perspective& perspective) {
				append_const_key_value<"type", "perspective">(builder);
				builder.append_comma();

				{
					auto perspective_obj = scoped_object<"perspective">(builder);

					if (perspective.aspectRatio.has_value()) {
						builder.append_key_value<"aspectRatio">(perspective.aspectRatio.value());
						builder.append_comma();
					}

					builder.append_key_value<"yfov">(perspective.yfov);
					builder.append_comma();

					if (perspective.zfar.has_value()) {
						builder.append_key_value<"zfar">(perspective.zfar.value());
						builder.append_comma();
					}

					builder.append_key_value<"znear">(perspective.znear);
				}
			},
			[&](const Camera::Orthographic& orthographic) {
				append_const_key_value<"type", "orthographic">(builder);
				builder.append_comma();

				{
					auto perspective_obj = scoped_object<"orthographic">(builder);

					builder.append_key_value<"xmag">(orthographic.xmag);
					builder.append_comma();
					builder.append_key_value<"ymag">(orthographic.ymag);
					builder.append_comma();
					builder.append_key_value<"zfar">(orthographic.zfar);
					builder.append_comma();
					builder.append_key_value<"znear">(orthographic.znear);
				}
			}
		}, it->camera);

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.cameras.begin(), it)), Category::Cameras, userPointer);
			if (extras.has_value()) {
				builder.append_comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}

		if (!it->name.empty()) {
			builder.append_comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

void fg::Exporter::writeImages(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.images.empty())
		return;

	auto images = scoped_array<"images">(builder);

	for (auto it = asset.images.begin(); it != asset.images.end(); ++it) {
		if (it != asset.images.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		auto imageIdx = uabs(std::distance(asset.images.begin(), it));
		std::visit(visitor {
			[&](auto&) {
				errorCode = Error::InvalidGltf;
			},
			[&](const sources::BufferView& bufferView) {
				builder.append_key_value<"bufferView">(bufferView.bufferViewIndex);
				builder.append_comma();
				builder.append_key_value<"mimeType">(internal::getMimeTypeString(bufferView.mimeType));
				imagePaths.emplace_back(std::nullopt);
			},
			[&](const sources::Array& vector) {
				auto path = getImageFilePath(asset, imageIdx, vector.mimeType);
				builder.append_key_value<"uri">(fg::normalizeAndFormatPath(path));
				if (vector.mimeType != MimeType::None) {
					builder.append_comma();
					builder.append_key_value<"mimeType">(internal::getMimeTypeString(vector.mimeType));
				}
				imagePaths.emplace_back(path);
			},
			[&](const sources::Vector& vector) {
				auto path = getImageFilePath(asset, imageIdx, vector.mimeType);
				builder.append_key_value<"uri">(fg::normalizeAndFormatPath(path));
				if (vector.mimeType != MimeType::None) {
					builder.append_comma();
					builder.append_key_value<"mimeType">(internal::getMimeTypeString(vector.mimeType));
				}
				imagePaths.emplace_back(path);
			},
			[&](const sources::URI& uri) {
				builder.append_key_value<"uri">(uri.uri->string());
				if (uri.mimeType != MimeType::None) {
					builder.append_comma();
					builder.append_key_value<"mimeType">(internal::getMimeTypeString(uri.mimeType));
				}
				imagePaths.emplace_back(std::nullopt);
			},
		}, it->data);
		if (errorCode != Error::None)
			return;

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.images.begin(), it)), Category::Images, userPointer);
			if (extras.has_value()) {
				builder.append_comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}

		if (!it->name.empty()) {
			builder.append_comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

void fg::Exporter::writeLights(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.lights.empty())
		return;

	auto lights_punctual = scoped_object<"KHR_lights_punctual">(builder);
	auto lights = scoped_array<"lights">(builder);

	for (auto it = asset.lights.begin(); it != asset.lights.end(); ++it) {
		if (it != asset.lights.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		// [1.0f, 1.0f, 1.0f] is the default.
		if (!(it->color[0] == 1.0f && it->color[1] == 1.0f && it->color[2] == 1.0f)) {
			append_key_range<"color">(builder, it->color);
			builder.append_comma();
		}

		if (it->intensity != 1.0f) {
			builder.append_key_value<"intensity">(it->intensity);
			builder.append_comma();
		}

		switch (it->type) {
			case LightType::Directional: {
				append_const_key_value<"type", "directional">(builder);
				break;
			}
			case LightType::Spot: {
				append_const_key_value<"type", "spot">(builder);
				break;
			}
			case LightType::Point: {
				append_const_key_value<"type", "point">(builder);
				break;
			}
		}

		if (it->range.has_value()) {
			builder.append_comma();
			builder.append_key_value<"range">(it->range.value());
		}

		// Spot cone angles live in a nested "spot" object per
		// KHR_lights_punctual (the parser reads them from there too).
		if (it->type == LightType::Spot && (it->innerConeAngle.has_value() || it->outerConeAngle.has_value())) {
			builder.append_comma();

			auto spot_obj = scoped_object<"spot">(builder);
			if (it->innerConeAngle.has_value()) {
				builder.append_key_value<"innerConeAngle">(it->innerConeAngle.value());
				if (it->outerConeAngle.has_value())
					builder.append_comma();
			}
			if (it->outerConeAngle.has_value())
				builder.append_key_value<"outerConeAngle">(it->outerConeAngle.value());
		}

		if (!it->name.empty()) {
			builder.append_comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

void fg::Exporter::writeMaterialExtensions(const Material &material, simdjson::builder::string_builder &builder) {
	auto obj = scoped_object<"extensions">(builder);

	comma_separator extensions_comma { builder };

	if (material.anisotropy) {
		extensions_comma();

		auto anisotropy_obj = scoped_object<"KHR_materials_anisotropy">(builder);

		comma_separator comma { builder };
		if (material.anisotropy->anisotropyStrength != 0.0f) {
			comma();
			builder.append_key_value<"anisotropyStrength">(material.anisotropy->anisotropyStrength);
		}
		if (material.anisotropy->anisotropyRotation != 0.0f) {
			comma();
			builder.append_key_value<"anisotropyRotation">(material.anisotropy->anisotropyRotation);
		}
		if (material.anisotropy->anisotropyTexture.has_value()) {
			comma();
			append_texture_info<"anisotropyTexture">(
				builder, &material.anisotropy->anisotropyTexture.value());
		}
	}

	if (material.clearcoat) {
		extensions_comma();

		auto clearcoat_obj = scoped_object<"KHR_materials_clearcoat">(builder);

		comma_separator comma { builder };
		if (material.clearcoat->clearcoatFactor != 0.0f) {
			comma();
			builder.append_key_value<"clearcoatFactor">(material.clearcoat->clearcoatFactor);
		}
		if (material.clearcoat->clearcoatTexture.has_value()) {
			comma();
			append_texture_info<"clearcoatTexture">(builder, &material.clearcoat->clearcoatTexture.value());
		}
		if (material.clearcoat->clearcoatRoughnessFactor != 0.0f) {
			comma();
			builder.append_key_value<"clearcoatRoughnessFactor">(material.clearcoat->clearcoatRoughnessFactor);
		}
		if (material.clearcoat->clearcoatRoughnessTexture.has_value()) {
			comma();
			append_texture_info<"clearcoatRoughnessTexture">(
				builder, &material.clearcoat->clearcoatRoughnessTexture.value());
		}
		if (material.clearcoat->clearcoatNormalTexture.has_value()) {
			comma();
			append_texture_info<"clearcoatNormalTexture", internal::TextureInfoType::NormalTexture>(
				builder, &material.clearcoat->clearcoatNormalTexture.value());
		}
	}

	if (material.diffuseTransmission) {
		extensions_comma();

		auto diffuse_transmission_obj = scoped_object<"KHR_materials_diffuse_transmission">(builder);

		comma_separator comma { builder };
		if (material.diffuseTransmission->diffuseTransmissionFactor != 0.0f) {
			comma();
			builder.append_key_value<"diffuseTransmissionFactor">(
				material.diffuseTransmission->diffuseTransmissionFactor);
		}
		if (material.diffuseTransmission->diffuseTransmissionTexture.has_value()) {
			comma();
			append_texture_info<"diffuseTransmissionTexture">(
				builder, &material.diffuseTransmission->diffuseTransmissionTexture.value());
		}
		if (material.diffuseTransmission->diffuseTransmissionColorFactor != math::nvec3(1)) {
			comma();
			append_key_range<"diffuseTransmissionColorFactor">(
				builder, material.diffuseTransmission->diffuseTransmissionColorFactor);
		}
		if (material.diffuseTransmission->diffuseTransmissionColorTexture.has_value()) {
			comma();
			append_texture_info<"diffuseTransmissionColorTexture">(
				builder, &material.diffuseTransmission->diffuseTransmissionColorTexture.value());
		}
	}

	if (material.dispersion != 0.0f) {
		extensions_comma();
		auto dispersion_obj = scoped_object<"KHR_materials_dispersion">(builder);
		builder.append_key_value<"dispersion">(material.dispersion);
	}

	if (material.emissiveStrength != 1.0f) {
		extensions_comma();
		auto emissive_strength_obj = scoped_object<"KHR_materials_emissive_strength">(builder);
		builder.append_key_value<"emissiveStrength">(material.emissiveStrength);
	}

	if (material.ior != 1.5f) {
		extensions_comma();
		auto ior_obj = scoped_object<"KHR_materials_ior">(builder);
		builder.append_key_value<"ior">(material.ior);
	}

	if (material.iridescence) {
		extensions_comma();

		auto iridescence_obj = scoped_object<"KHR_materials_iridescence">(builder);

		comma_separator comma { builder };
		if (material.iridescence->iridescenceFactor != 0.0f) {
			comma();
			builder.append_key_value<"iridescenceFactor">(
				material.iridescence->iridescenceFactor);
		}
		if (material.iridescence->iridescenceTexture.has_value()) {
			comma();
			append_texture_info<"iridescenceTexture">(
				builder, &material.iridescence->iridescenceTexture.value());
		}
		if (material.iridescence->iridescenceIor != 1.3f) {
			comma();
			builder.append_key_value<"iridescenceIor">(
				material.iridescence->iridescenceIor);
		}
		if (material.iridescence->iridescenceThicknessMinimum != 100.0f) {
			comma();
			builder.append_key_value<"iridescenceThicknessMinimum">(
				material.iridescence->iridescenceThicknessMinimum);
		}
		if (material.iridescence->iridescenceThicknessMaximum != 400.0f) {
			comma();
			builder.append_key_value<"iridescenceThicknessMaximum">(
				material.iridescence->iridescenceThicknessMaximum);
		}
		if (material.iridescence->iridescenceThicknessTexture.has_value()) {
			comma();
			append_texture_info<"iridescenceThicknessTexture">(
				builder, &material.iridescence->iridescenceThicknessTexture.value());
		}
	}

	if (material.sheen) {
		extensions_comma();

		auto sheen_obj = scoped_object<"KHR_materials_sheen">(builder);

		comma_separator comma { builder };
		if (material.sheen->sheenColorFactor != math::nvec3(0)) {
			comma();
			append_key_range<"sheenColorFactor">(
				builder, material.sheen->sheenColorFactor);
		}
		if (material.sheen->sheenColorTexture.has_value()) {
			comma();
			append_texture_info<"sheenColorTexture">(
				builder, &material.sheen->sheenColorTexture.value());
		}
		if (material.sheen->sheenRoughnessFactor != 0.0f) {
			comma();
			builder.append_key_value<"sheenRoughnessFactor">(
				material.sheen->sheenRoughnessFactor);
		}
		if (material.sheen->sheenRoughnessTexture.has_value()) {
			comma();
			append_texture_info<"sheenRoughnessTexture">(
				builder, &material.sheen->sheenRoughnessTexture.value());
		}
	}

	if (material.specular) {
		extensions_comma();

		auto specular_obj = scoped_object<"KHR_materials_specular">(builder);

		comma_separator comma { builder };
		if (material.specular->specularFactor != 1.0f) {
			comma();
			builder.append_key_value<"specularFactor">(
				material.specular->specularFactor);
		}
		if (material.specular->specularTexture.has_value()) {
			comma();
			append_texture_info<"specularTexture">(
				builder, &material.specular->specularTexture.value());
		}
		if (material.specular->specularColorFactor != math::nvec3(1)) {
			comma();
			append_key_range<"specularColorFactor">(
				builder, material.specular->specularColorFactor);
		}
		if (material.specular->specularColorTexture.has_value()) {
			comma();
			append_texture_info<"specularColorTexture">(
				builder, &material.specular->specularColorTexture.value());
		}
	}

	if (material.specularGlossiness) {
		extensions_comma();

		auto pbr_obj = scoped_object<"KHR_materials_pbrSpecularGlossiness">(builder);

		comma_separator comma { builder };
		if (material.specularGlossiness->diffuseFactor != math::nvec4(1)) {
			comma();
			append_key_range<"diffuseFactor">(
				builder, material.specularGlossiness->diffuseFactor);
		}
		if (material.specularGlossiness->diffuseTexture.has_value()) {
			comma();
			append_texture_info<"diffuseTexture">(
				builder, &material.specularGlossiness->diffuseTexture.value());
		}
		if (material.specularGlossiness->specularFactor != math::nvec3(1)) {
			comma();
			append_key_range<"specularFactor">(
				builder, material.specularGlossiness->specularFactor);
		}
		if (material.specularGlossiness->glossinessFactor != 1.0) {
			comma();
			builder.append_key_value<"glossinessFactor">(
				material.specularGlossiness->glossinessFactor);
		}
		if (material.specularGlossiness->specularGlossinessTexture.has_value()) {
			comma();
			append_texture_info<"specularGlossinessTexture">(
				builder, &material.specularGlossiness->specularGlossinessTexture.value());
		}
	}

	if (material.transmission) {
		extensions_comma();

		auto transmission_obj = scoped_object<"KHR_materials_transmission">(builder);

		comma_separator comma { builder };
		if (material.transmission->transmissionFactor != 0.0f) {
			comma();
			builder.append_key_value<"transmissionFactor">(
				material.transmission->transmissionFactor);
		}
		if (material.transmission->transmissionTexture.has_value()) {
			comma();
			append_texture_info<"transmissionTexture">(
				builder, &material.transmission->transmissionTexture.value());
		}
	}

	if (material.unlit) {
		extensions_comma();

		auto unlit_obj = scoped_object<"KHR_materials_unlit">(builder);
	}

	if (material.volume) {
		extensions_comma();

		auto volume_obj = scoped_object<"KHR_materials_volume">(builder);

		comma_separator comma { builder };
		if (material.volume->thicknessFactor != 0.0f) {
			comma();
			builder.append_key_value<"thicknessFactor">(
				material.volume->thicknessFactor);
		}
		if (material.volume->thicknessTexture.has_value()) {
			comma();
			append_texture_info<"thicknessTexture">(
				builder, &material.volume->thicknessTexture.value());
		}
		if (material.volume->attenuationDistance != std::numeric_limits<num>::infinity()) {
			comma();
			builder.append_key_value<"attenuationDistance">(
				material.volume->attenuationDistance);
		}
		if (material.volume->attenuationColor != math::nvec3(1)) {
			comma();
			append_key_range<"attenuationColor">(
				builder, material.volume->attenuationColor);
		}
	}

	if (material.packedNormalMetallicRoughnessTexture.has_value()) {
		extensions_comma();

		auto packing_obj = scoped_object<"MSFT_packing_normalRoughnessMetallic">(builder);

		append_texture_info<"normalRoughnessMetallicTexture">(
			builder, &material.packedNormalMetallicRoughnessTexture.value());
	}

	if (material.packedOcclusionRoughnessMetallicTextures) {
		extensions_comma();

		auto packing_obj = scoped_object<"MSFT_packing_occlusionRoughnessMetallic">(builder);

		comma_separator comma { builder };
		if (material.packedOcclusionRoughnessMetallicTextures->occlusionRoughnessMetallicTexture.has_value()) {
			comma();
			append_texture_info<"occlusionRoughnessMetallicTexture">(
				builder, &material.packedOcclusionRoughnessMetallicTextures->occlusionRoughnessMetallicTexture.value());
		}
		if (material.packedOcclusionRoughnessMetallicTextures->roughnessMetallicOcclusionTexture.has_value()) {
			comma();
			append_texture_info<"roughnessMetallicOcclusionTexture">(
				builder, &material.packedOcclusionRoughnessMetallicTextures->roughnessMetallicOcclusionTexture.value());
		}
		if (material.packedOcclusionRoughnessMetallicTextures->normalTexture.has_value()) {
			comma();
			append_texture_info<"normalTexture">(
				builder, &material.packedOcclusionRoughnessMetallicTextures->normalTexture.value());
		}
	}
}

void fg::Exporter::writeMaterials(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.materials.empty())
		return;

	auto materials = scoped_array<"materials">(builder);

	for (auto it = asset.materials.begin(); it != asset.materials.end(); ++it) {
		if (it != asset.materials.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		{
			auto pbr_obj = scoped_object<"pbrMetallicRoughness">(builder);

			comma_separator comma {builder};
			if (it->pbrData.baseColorFactor != math::nvec4(1)) {
				comma();
				append_key_range<"baseColorFactor">(builder, it->pbrData.baseColorFactor);
			}

			if (it->pbrData.baseColorTexture.has_value()) {
				comma();
				append_texture_info<"baseColorTexture">(builder, &it->pbrData.baseColorTexture.value());
			}

			if (it->pbrData.metallicFactor != 1.0f) {
				comma();
				builder.append_key_value<"metallicFactor">(it->pbrData.metallicFactor);
			}

			if (it->pbrData.roughnessFactor != 1.0f) {
				comma();
				builder.append_key_value<"roughnessFactor">(it->pbrData.roughnessFactor);
			}

			if (it->pbrData.metallicRoughnessTexture.has_value()) {
				comma();
				append_texture_info<"metallicRoughnessTexture">(builder, &it->pbrData.metallicRoughnessTexture.value());
			}
		}

		if (it->normalTexture.has_value()) {
			builder.append_comma();
			append_texture_info<"normalTexture", internal::TextureInfoType::NormalTexture>(builder, &it->normalTexture.value());
		}

		if (it->occlusionTexture.has_value()) {
			builder.append_comma();
			append_texture_info<"occlusionTexture", internal::TextureInfoType::OcclusionTexture>(builder, &it->occlusionTexture.value());
		}

		if (it->emissiveTexture.has_value()) {
			builder.append_comma();
			append_texture_info<"emissiveTexture">(builder, &it->emissiveTexture.value());
		}

		if (it->emissiveFactor != math::nvec3(0)) {
			builder.append_comma();
			append_key_range<"emissiveFactor">(builder, it->emissiveFactor);
		}

		if (it->alphaMode != AlphaMode::Opaque) {
			builder.append_comma();
			builder.escape_and_append_with_quotes<"alphaMode">();
			builder.append_colon();
			if (it->alphaMode == AlphaMode::Blend) {
				builder.escape_and_append_with_quotes<"BLEND">();
			} else if (it->alphaMode == AlphaMode::Mask) {
				builder.escape_and_append_with_quotes<"MASK">();
			}
		}

		if (it->alphaMode == AlphaMode::Mask && it->alphaCutoff != 0.5f) {
			builder.append_comma();
			builder.append_key_value<"alphaCutoff">(it->alphaCutoff);
		}

		if (it->doubleSided) {
			builder.append_comma();
			append_const_key_value<"doubleSided", true>(builder);
		}

		builder.append_comma();
		writeMaterialExtensions(*it, builder);

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.materials.begin(), it)), Category::Materials, userPointer);
			if (extras.has_value()) {
				builder.append_comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}

		if (!it->name.empty()) {
			builder.append_comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

void fg::Exporter::writeMeshes(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.meshes.empty())
		return;

	auto meshes = scoped_array<"meshes">(builder);

	for (auto it = asset.meshes.begin(); it != asset.meshes.end(); ++it) {
		if (it != asset.meshes.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		comma_separator comma { builder };

		if (!it->primitives.empty()) {
			comma();

			auto primitives = scoped_array<"primitives">(builder);

			for (auto itp = it->primitives.begin(); itp != it->primitives.end(); ++itp) {
				if (itp != it->primitives.begin())
					builder.append_comma();

				auto pobj = scoped_object(builder);

				{
					auto attributes = scoped_object<"attributes">(builder);
					for (auto ita = itp->attributes.begin(); ita != itp->attributes.end(); ++ita) {
						if (ita != itp->attributes.begin())
							builder.append_comma();

						builder.append_key_value(ita->name, ita->accessorIndex);
					}
				}

				if (itp->indicesAccessor.has_value()) {
					builder.append_comma();
					builder.append_key_value<"indices">(itp->indicesAccessor.value());
				}

				if (itp->materialIndex.has_value()) {
					builder.append_comma();
					builder.append_key_value<"material">(itp->materialIndex.value());
				}

				if (!itp->targets.empty()) {
					builder.append_comma();

					auto targets = scoped_array<"targets">(builder);
					for (auto itt = itp->targets.begin(); itt != itp->targets.end(); ++itt) {
						if (itt != itp->targets.begin())
							builder.append_comma();

						auto target_obj = scoped_object(builder);
						for (auto ita = itt->begin(); ita != itt->end(); ++ita) {
							if (ita != itt->begin())
								builder.append_comma();

							builder.append_key_value(ita->name, ita->accessorIndex);
						}
					}
				}

				if (itp->type != PrimitiveType::Triangles) {
					builder.append_comma();
					builder.append_key_value<"mode">(to_underlying(itp->type));
				}

				const bool hasExtensions = !itp->mappings.empty() || itp->dracoCompression;
				if (hasExtensions) {
					builder.append_comma();
					builder.escape_and_append_with_quotes<"extensions">();
					builder.append_colon();
					builder.start_object();
				}
				if (!itp->mappings.empty()) {
					auto variants_obj = scoped_object<"KHR_materials_variants">(builder);
					auto mappings = scoped_array<"mappings">(builder);

					comma_separator mappingComma {builder};
					// TODO: We should optimise to avoid writing multiple objects for the same material index
					for (std::size_t i = 0; i < asset.materialVariants.size(); ++i) {
						if (!itp->mappings[i].has_value())
							continue;
						mappingComma();

						auto variant_obj = scoped_object(builder);
						builder.append_key_value<"material">(itp->mappings[i].value());
						builder.append_comma();
						builder.escape_and_append_with_quotes<"variants">();
						builder.append_colon();

						auto arr = scoped_array(builder);
						builder.append(i);
					}
				}
				if (itp->dracoCompression) {
					if (!itp->mappings.empty())
						builder.append_comma();

					auto draco_obj = scoped_object<"KHR_draco_mesh_compression">(builder);

					builder.append_key_value<"bufferView">(itp->dracoCompression->bufferView);
					builder.append_comma();

					auto attributes = scoped_object<"attributes">(builder);
					for (auto ita = itp->dracoCompression->attributes.begin(); ita != itp->dracoCompression->attributes.end(); ++ita) {
						if (ita != itp->dracoCompression->attributes.begin())
							builder.append_comma();

						builder.append_key_value(ita->name, ita->accessorIndex);
					}
				}
				if (hasExtensions) {
					builder.end_object();
				}
			}
		}

		if (!it->weights.empty()) {
			comma();
			append_key_range<"weights">(builder, it->weights);
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.meshes.begin(), it)), Category::Meshes, userPointer);
			if (extras.has_value()) {
				comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}

		if (!it->name.empty()) {
			comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

void fg::Exporter::writeNodes(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.nodes.empty())
		return;

	auto nodes = scoped_array<"nodes">(builder);

	for (auto it = asset.nodes.begin(); it != asset.nodes.end(); ++it) {
		if (it != asset.nodes.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		comma_separator comma { builder };

		if (it->meshIndex.has_value()) {
			comma();
			builder.append_key_value<"mesh">(it->meshIndex.value());
		}
		if (it->cameraIndex.has_value()) {
			comma();
			builder.append_key_value<"camera">(it->cameraIndex.value());
		}
		if (it->skinIndex.has_value()) {
			comma();
			builder.append_key_value<"skin">(it->skinIndex.value());
		}

		if (!it->children.empty()) {
			comma();
			append_key_range<"children">(builder, it->children);
		}

		if (!it->weights.empty()) {
			comma();
			append_key_range<"weights">(builder, it->weights);
		}

		visit_exhaustive(visitor {
			[&](const TRS& trs) {
				if (trs.rotation != math::fquat(0.f, 0.f, 0.f, 1.f)) {
					comma();
					append_key_range<"rotation">(builder, trs.rotation);
				}

				if (trs.scale != math::fvec3(1.f)) {
					comma();
					append_key_range<"scale">(builder, trs.scale);
				}

				if (trs.translation != math::fvec3(0.f)) {
					comma();
					append_key_range<"translation">(builder, trs.translation);
				}
			},
			[&](const math::fmat4x4& matrix) {
				comma();
				auto arr = scoped_array<"matrix">(builder);
				for (std::size_t i = 0; i < matrix.columns(); ++i) {
					for (std::size_t j = 0; j < matrix.rows(); ++j) {
						builder.append(matrix.col(i)[j]);
						if (i * matrix.columns() + j + 1 < matrix.columns() * matrix.rows()) {
							builder.append_comma();
						}
					}
				}
			},
		}, it->transform);

		if (!it->instancingAttributes.empty() || it->lightIndex.has_value()
#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
			|| it->physicsRigidBody
#endif
			|| !it->visible || !it->selectable || !it->hoverable
			) {
			comma();

			auto extensions_obj = scoped_object<"extensions">(builder);

			comma_separator extensionComma { builder };

			if (!it->instancingAttributes.empty()) {
				extensionComma();

				auto instancing_obj = scoped_object<"EXT_mesh_gpu_instancing">(builder);
				auto attributes = scoped_object<"attributes">(builder);
				for (auto ait = it->instancingAttributes.begin(); ait != it->instancingAttributes.end(); ++ait) {
					if (ait != it->instancingAttributes.begin())
						builder.append_comma();

					builder.append_key_value(ait->name, ait->accessorIndex);
				}
			}
			if (it->lightIndex.has_value()) {
				extensionComma();

				auto lights_obj = scoped_object<"KHR_lights_punctual">(builder);
				builder.append_key_value<"light">(it->lightIndex.value());
			}

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
			if (it->physicsRigidBody) {
				extensionComma();

				auto physics_obj = scoped_object<"KHR_physics_rigid_bodies">(builder);
				comma_separator bodyComma { builder };

				const auto writeGeometry = [&builder](const Geometry& geom) {
					auto geometry_obj = scoped_object<"geometry">(builder);

					comma_separator geometryComma { builder };

					if (geom.shape.has_value()) {
						geometryComma();
						builder.append_key_value<"shape">(*geom.shape);
					} else if (geom.mesh.has_value()) {
						geometryComma();
						builder.append_key_value<"mesh">(*geom.mesh);
					}
					if (geom.convexHull) {
						geometryComma();
						append_const_key_value<"convexHull", true>(builder);
					}
				};

				if (it->physicsRigidBody->motion.has_value()) {
					const auto& motion = *it->physicsRigidBody->motion;
					bodyComma();

					auto motion_obj = scoped_object<"motion">(builder);

					comma_separator motionComma { builder };

					if (motion.isKinematic) {
						motionComma();
						append_const_key_value<"isKinematic", true>(builder);
					}
					if (motion.mass.has_value()) {
						motionComma();
						builder.append_key_value<"mass">(*motion.mass);
					}
					if (motion.centerOfMass != math::fvec3(0)) {
						motionComma();
						append_key_range<"centerOfMass">(builder, motion.centerOfMass);
					}
					if (motion.inertialDiagonal.has_value()) {
						motionComma();
						append_key_range<"inertiaDiagonal">(builder, *motion.inertialDiagonal);
					}
					if (motion.inertialOrientation.has_value()) {
						motionComma();
						append_key_range<"inertiaOrientation">(builder, *motion.inertialOrientation);
					}
					if (motion.linearVelocity != math::fvec3(0)) {
						motionComma();
						append_key_range<"linearVelocity">(builder, motion.linearVelocity);
					}
					if (motion.angularVelocity != math::fvec3(0)) {
						motionComma();
						append_key_range<"angularVelocity">(builder, motion.angularVelocity);
					}
					if (motion.gravityFactor != num(1)) {
						motionComma();
						builder.append_key_value<"gravityFactor">(motion.gravityFactor);
					}
				}

				if (it->physicsRigidBody->collider.has_value()) {
					const auto& collider = *it->physicsRigidBody->collider;
					bodyComma();

					auto collider_obj = scoped_object<"collider">(builder);
					writeGeometry(collider.geometry);
					if (collider.physicsMaterial.has_value()) {
						builder.append_comma();
						builder.append_key_value<"physicsMaterial">(*collider.physicsMaterial);
					}
					if (collider.collisionFilter.has_value()) {
						builder.append_comma();
						builder.append_key_value<"collisionFilter">(*collider.collisionFilter);
					}
				}

				if (it->physicsRigidBody->trigger.has_value()) {
					bodyComma();

					auto trigger_obj = scoped_object<"trigger">(builder);
					visit_exhaustive(visitor{
						[&](const GeometryTrigger& gt) {
							writeGeometry(gt.geometry);
							if (gt.collisionFilter.has_value()) {
								builder.append_comma();
								builder.append_key_value<"collisionFilter">(*gt.collisionFilter);
							}
						},
						[&](const NodeTrigger& nt) {
							append_key_range<"nodes">(builder, nt.nodes);
						}
					}, *it->physicsRigidBody->trigger);
				}

				if (it->physicsRigidBody->joint.has_value()) {
					const auto& joint = *it->physicsRigidBody->joint;
					bodyComma();

					auto joint_obj = scoped_object<"joint">(builder);
					builder.append_key_value<"connectedNode">(joint.connectedNode);
					builder.append_comma();
					builder.append_key_value<"joint">(joint.joint);
					if (joint.enableCollision) {
						builder.append_comma();
						append_const_key_value<"enableCollision", true>(builder);
					}
				}
			}
#endif

			if (!it->visible) {
				extensionComma();

				auto visibility_obj = scoped_object<"KHR_node_visibility">(builder);
				append_const_key_value<"visible", false>(builder);
			}

			if (!it->selectable) {
				extensionComma();

				auto selectable_obj = scoped_object<"KHR_node_selectability">(builder);
				append_const_key_value<"selectable", false>(builder);
			}

			if (!it->hoverable) {
				extensionComma();

				auto hoverability_obj = scoped_object<"KHR_node_hoverability">(builder);
				append_const_key_value<"hoverable", false>(builder);
			}
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.nodes.begin(), it)), Category::Nodes, userPointer);
			if (extras.has_value()) {
				comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}

		if (!it->name.empty()) {
			comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

void fg::Exporter::writeSamplers(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.samplers.empty())
		return;

	auto samplers = scoped_array<"samplers">(builder);

	for (auto it = asset.samplers.begin(); it != asset.samplers.end(); ++it) {
		if (it != asset.samplers.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		comma_separator comma { builder };

		if (it->magFilter.has_value()) {
			comma();
			builder.append_key_value<"magFilter">(to_underlying(it->magFilter.value()));
		}
		if (it->minFilter.has_value()) {
			comma();
			builder.append_key_value<"minFilter">(to_underlying(it->minFilter.value()));
		}
		if (it->wrapS != Wrap::Repeat) {
			comma();
			builder.append_key_value<"wrapS">(to_underlying(it->wrapS));
		}
		if (it->wrapT != Wrap::Repeat) {
			comma();
			builder.append_key_value<"wrapT">(to_underlying(it->wrapT));
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.samplers.begin(), it)), Category::Samplers, userPointer);
			if (extras.has_value()) {
				comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}

		if (!it->name.empty()) {
			comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

void fg::Exporter::writeScenes(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.scenes.empty())
		return;

	if (asset.defaultScene.has_value()) {
		builder.append_key_value<"scene">(asset.defaultScene.value());
		builder.append_comma();
	}

	auto scenes = scoped_array<"scenes">(builder);

	for (auto it = asset.scenes.begin(); it != asset.scenes.end(); ++it) {
		if (it != asset.scenes.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		comma_separator comma {builder};

		// scene.nodes is optional (minItems:1), so omit when empty.
		if (!it->nodeIndices.empty()) {
			comma();
			append_key_range<"nodes">(builder, it->nodeIndices);
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.scenes.begin(), it)), Category::Scenes, userPointer);
			if (extras.has_value()) {
				comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}

		if (!it->name.empty()) {
			comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

void fg::Exporter::writeSkins(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.skins.empty())
		return;

	auto skins = scoped_array<"skins">(builder);

	for (auto it = asset.skins.begin(); it != asset.skins.end(); ++it) {
		if (it != asset.skins.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		if (it->inverseBindMatrices.has_value()) {
			builder.append_key_value<"inverseBindMatrices">(it->inverseBindMatrices.value());
			builder.append_comma();
		}

		if (it->skeleton.has_value()) {
			builder.append_key_value<"skeleton">(it->skeleton.value());
			builder.append_comma();
		}

		append_key_range<"joints">(builder, it->joints);

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.skins.begin(), it)), Category::Skins, userPointer);
			if (extras.has_value()) {
				builder.append_comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}

		if (!it->name.empty()) {
			builder.append_comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

void fg::Exporter::writeTextures(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.textures.empty())
		return;

	auto textures = scoped_array<"textures">(builder);

	for (auto it = asset.textures.begin(); it != asset.textures.end(); ++it) {
		if (it != asset.textures.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		comma_separator comma { builder };

		if (it->samplerIndex.has_value()) {
			comma();
			builder.append_key_value<"sampler">(it->samplerIndex.value());
		}

		if (it->imageIndex.has_value()) {
			comma();
			builder.append_key_value<"source">(it->imageIndex.value());
		}

		if (it->basisuImageIndex.has_value() || it->ddsImageIndex.has_value() || it->webpImageIndex.has_value()) {
			comma();

			auto extensions_obj = scoped_object<"extensions">(builder);

			comma_separator extensionComma { builder };

			if (it->basisuImageIndex.has_value()) {
				extensionComma();
				auto basisu_obj = scoped_object<"KHR_texture_basisu">(builder);
				builder.append_key_value<"source">(it->basisuImageIndex.value());
			}
			if (it->ddsImageIndex.has_value()) {
				extensionComma();
				auto dds_obj = scoped_object<"MSFT_texture_dds">(builder);
				builder.append_key_value<"source">(it->ddsImageIndex.value());
			}
			if (it->webpImageIndex.has_value()) {
				extensionComma();
				auto webp_obj = scoped_object<"EXT_texture_webp">(builder);
				builder.append_key_value<"source">(it->webpImageIndex.value());
			}
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.textures.begin(), it)), Category::Textures, userPointer);
			if (extras.has_value()) {
				comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}

		if (!it->name.empty()) {
			comma();
			builder.append_key_value<"name">(it->name);
		}
	}
}

#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
void fg::Exporter::writeShapes(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.shapes.empty()) {
		return;
	}

	auto implicit_shapes = scoped_object<"KHR_implicit_shapes">(builder);
	auto shapes = scoped_array<"shapes">(builder);

	for (auto it = asset.shapes.begin(); it != asset.shapes.end(); ++it) {
		if (it != asset.shapes.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		const auto& shape = *it;

		visit_exhaustive(visitor {
			[&](const SphereShape& sphere) {
				append_const_key_value<"type", "sphere">(builder);
				builder.append_comma();

				auto sphere_obj = scoped_object<"sphere">(builder);
				builder.append_key_value<"radius">(sphere.radius);
			},
			[&](const BoxShape& box) {
				append_const_key_value<"type", "box">(builder);
				builder.append_comma();

				auto box_obj = scoped_object<"box">(builder);
				append_key_range<"size">(builder, box.size);
			},
			[&](const CylinderShape& cylinder) {
				append_const_key_value<"type", "cylinder">(builder);
				builder.append_comma();

				auto cylinder_obj = scoped_object<"cylinder">(builder);
				builder.append_key_value<"height">(cylinder.height);
				builder.append_comma();
				builder.append_key_value<"radiusBottom">(cylinder.radiusBottom);
				builder.append_comma();
				builder.append_key_value<"radiusTop">(cylinder.radiusTop);
			},
			[&](const CapsuleShape& capsule) {
				append_const_key_value<"type", "capsule">(builder);
				builder.append_comma();

				auto capsule_obj = scoped_object<"capsule">(builder);
				builder.append_key_value<"height">(capsule.height);
				builder.append_comma();
				builder.append_key_value<"radiusBottom">(capsule.radiusBottom);
				builder.append_comma();
				builder.append_key_value<"radiusTop">(capsule.radiusTop);
			},
			[&](const PlaneShape& plane) {
				append_const_key_value<"type", "plane">(builder);
				builder.append_comma();

				auto plane_obj = scoped_object<"plane">(builder);
				builder.append_key_value<"sizeX">(plane.sizeX);
				builder.append_comma();
				builder.append_key_value<"sizeZ">(plane.sizeZ);
				builder.append_comma();
				builder.append_key_value<"doubleSided">(plane.doubleSided);
			}
		}, shape);

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.shapes.begin(), it)), Category::Shapes, userPointer);
			if (extras.has_value()) {
				builder.append_comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}
	}
}
#endif

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
void fg::Exporter::writePhysicsMaterials(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.physicsMaterials.empty()) {
		return;
	}

	auto physics_materials = scoped_array<"physicsMaterials">(builder);

	const auto writeCombineMode = [&builder](CombineMode mode) {
		switch (mode) {
		case CombineMode::Average:
		case CombineMode::Invalid:
			builder.escape_and_append_with_quotes<"average">();
			break;
		case CombineMode::Minimum:
			builder.escape_and_append_with_quotes<"minimum">();
			break;
		case CombineMode::Maximum:
			builder.escape_and_append_with_quotes<"maximum">();
			break;
		case CombineMode::Multiply:
			builder.escape_and_append_with_quotes<"multiply">();
			break;
		}
	};

	for (auto it = asset.physicsMaterials.begin(); it != asset.physicsMaterials.end(); ++it) {
		if (it != asset.physicsMaterials.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		const auto& material = *it;

		builder.append_key_value<"staticFriction">(material.staticFriction);
		builder.append_comma();
		builder.append_key_value<"dynamicFriction">(material.dynamicFriction);
		builder.append_comma();
		builder.append_key_value<"restitution">(material.restitution);
		builder.append_comma();

		builder.escape_and_append_with_quotes<"frictionCombine">();
		builder.append_colon();
		writeCombineMode(material.frictionCombine);

		builder.append_comma();

		builder.escape_and_append_with_quotes<"restitutionCombine">();
		builder.append_colon();
		writeCombineMode(material.restitutionCombine);

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.physicsMaterials.begin(), it)), Category::PhysicsMaterials, userPointer);
			if (extras.has_value()) {
				builder.append_comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}
	}
}

void fg::Exporter::writeCollisionFilters(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.collisionFilters.empty()) {
		return;
	}

	auto collision_filters = scoped_array<"collisionFilters">(builder);

	for (auto it = asset.collisionFilters.begin(); it != asset.collisionFilters.end(); ++it) {
		if (it != asset.collisionFilters.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		const auto& filter = *it;

		comma_separator comma { builder };

		if (!filter.collisionSystems.empty()) {
			comma();
			append_key_range<"collisionSystems">(builder, filter.collisionSystems);
		}

		if (!filter.collideWithSystems.empty()) {
			comma();
			append_key_range<"collideWithSystems">(builder, filter.collideWithSystems);
		}

		if (!filter.notCollideWithSystems.empty()) {
			comma();
			append_key_range<"notCollideWithSystems">(builder, filter.notCollideWithSystems);
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.collisionFilters.begin(), it)), Category::Shapes, userPointer);
			if (extras.has_value()) {
				comma();
				append_raw_key_value<"extras">(builder, *extras);
			}
		}
	}
}

void fg::Exporter::writePhysicsJoints(const Asset& asset, simdjson::builder::string_builder& builder) {
	if (asset.physicsJoints.empty()) {
		return;
	}

	auto physics_joints = scoped_array<"physicsJoints">(builder);

	for (auto it = asset.physicsJoints.begin(); it != asset.physicsJoints.end(); ++it) {
		if (it != asset.physicsJoints.begin())
			builder.append_comma();

		auto obj = scoped_object(builder);

		const auto& joint = *it;

		if (!joint.limits.empty()) {
			auto limits = scoped_array<"limits">(builder);

			for (auto lit = joint.limits.begin(); lit != joint.limits.end(); ++lit) {
				if (lit != joint.limits.begin())
					builder.append_comma();

				auto limit_obj = scoped_object(builder);

				const auto& limit = *lit;

				if (!limit.linearAxes.empty()) {
					append_key_range<"linearAxes">(builder, limit.linearAxes);
					builder.append_comma();
				}
				if (!limit.angularAxes.empty()) {
					append_key_range<"angularAxes">(builder, limit.angularAxes);
					builder.append_comma();
				}
				if (limit.min) {
					builder.append_key_value<"min">(*limit.min);
					builder.append_comma();
				}
				if (limit.max) {
					builder.append_key_value<"max">(*limit.max);
					builder.append_comma();
				}
				if (limit.stiffness) {
					builder.append_key_value<"stiffness">(*limit.stiffness);
					builder.append_comma();
				}
				builder.append_key_value<"damping">(limit.damping);
			}
		}

		if (!joint.drives.empty()) {
			if (!joint.limits.empty())
				builder.append_comma();

			auto drives = scoped_array<"drives">(builder);

			for (auto dit = joint.drives.begin(); dit != joint.drives.end(); ++dit) {
				if (dit != joint.drives.begin())
					builder.append_comma();

				auto drive_obj = scoped_object(builder);

				const auto& drive = *dit;

				switch (drive.type) {
				case DriveType::Linear:
				case DriveType::Invalid:
					append_const_key_value<"type", "linear">(builder);
					break;
				case DriveType::Angular:
					append_const_key_value<"type", "angular">(builder);
					break;
				}

				builder.append_comma();
				switch (drive.mode) {
				case DriveMode::Force:
				case DriveMode::Invalid:
					append_const_key_value<"mode", "force">(builder);
					break;
				case DriveMode::Acceleration:
					append_const_key_value<"mode", "acceleration">(builder);
					break;
				}

				builder.append_comma();
				builder.append_key_value<"axis">(drive.axis);
				builder.append_comma();
				builder.append_key_value<"maxForce">(drive.maxForce);
				builder.append_comma();
				builder.append_key_value<"positionTarget">(drive.positionTarget);
				builder.append_comma();
				builder.append_key_value<"velocityTarget">(drive.velocityTarget);
				builder.append_comma();
				builder.append_key_value<"stiffness">(drive.stiffness);
				builder.append_comma();
				builder.append_key_value<"damping">(drive.damping);
			}
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.physicsJoints.begin(), it)), Category::PhysicsJoints, userPointer);
			if (extras.has_value()) {
				if (!joint.limits.empty() || !joint.drives.empty()) {
					builder.append_comma();
				}
				append_raw_key_value<"extras">(builder, *extras);
			}
		}
	}
}
#endif

void fg::Exporter::writeExtensions(const Asset& asset, simdjson::builder::string_builder& builder) {
	auto extensions = scoped_object<"extensions">(builder);

	comma_separator comma { builder };

	if (!asset.lights.empty()) {
		comma();
		writeLights(asset, builder);
	}

#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
	if (!asset.shapes.empty()) {
		comma();
		writeShapes(asset, builder);
	}
#endif

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
	if (!asset.physicsMaterials.empty() || !asset.collisionFilters.empty() || !asset.physicsJoints.empty()) {
		comma();

		auto obj = scoped_object<"KHR_physics_rigid_bodies">(builder);

		comma_separator physicsComma { builder };
		if (!asset.physicsMaterials.empty()) {
			physicsComma();
			writePhysicsMaterials(asset, builder);
		}
		if (!asset.collisionFilters.empty()) {
			physicsComma();
			writeCollisionFilters(asset, builder);
		}
		if (!asset.physicsJoints.empty()) {
			physicsComma();
			writePhysicsJoints(asset, builder);
		}
	}
#endif

	if (!asset.materialVariants.empty()) {
		comma();

		auto obj = scoped_object<"KHR_materials_variants">(builder);
		auto arr = scoped_array<"variants">(builder);
		for (auto it = asset.materialVariants.begin(); it != asset.materialVariants.end(); ++it) {
			if (it != asset.materialVariants.begin())
				builder.append_comma();

			auto variant_obj = scoped_object(builder);
			builder.append_key_value<"name">(*it);
		}
	}
}

fs::path fg::Exporter::getBufferFilePath(const Asset& asset, std::size_t index) {
	const auto& bufferName = asset.buffers[index].name;
	if (bufferName.empty()) {
		return bufferFolder / ("buffer" + std::to_string(index) + ".bin");
	}
	return bufferFolder / (bufferName + ".bin");
}

fs::path fg::Exporter::getImageFilePath(const Asset& asset, std::size_t index, MimeType mimeType) {
	std::string_view extension;
	switch (mimeType) {
		case MimeType::JPEG:
			extension = ".jpeg";
			break;
		case MimeType::PNG:
			extension = ".png";
			break;
		case MimeType::KTX2:
			extension = ".ktx2";
			break;
		case MimeType::DDS:
			extension = ".dds";
			break;
		default:
		case MimeType::None:
		case MimeType::GltfBuffer:
		case MimeType::OctetStream:
			extension = ".bin";
			break;
	}

	const auto& imageName = asset.images[index].name;
	if (imageName.empty()) {
		return imageFolder / ("image" + std::to_string(index) + std::string(extension));
	}
	return imageFolder / (std::string(imageName) + std::string(extension));
}

std::string fg::Exporter::writeJson(const Asset &asset) {
	simdjson::builder::string_builder builder;

	{
		auto root_obj = scoped_object(builder);

		// Write asset info
		{
			auto asset_obj = scoped_object<"asset">(builder);
			if (asset.assetInfo.has_value()) {
				builder.append_key_value<"version">(asset.assetInfo->gltfVersion);
				if (!asset.assetInfo->copyright.empty()) {
					builder.append_comma();
					builder.append_key_value<"copyright">(asset.assetInfo->copyright);
				}
				if (!asset.assetInfo->generator.empty()) {
					builder.append_comma();
					builder.append_key_value<"generator">(asset.assetInfo->generator);
				}
				if (!asset.assetInfo->minVersion.empty()) {
					builder.append_comma();
					builder.append_key_value<"minVersion">(asset.assetInfo->minVersion);
				}
			} else {
				append_const_key_value<"generator", "fastgltf">(builder);
				builder.append_comma();
				append_const_key_value<"version", "2.0">(builder);
			}
		}

		// Write extension usage info
		if (!asset.extensionsUsed.empty()) {
			builder.append_comma();
			append_key_range<"extensionsUsed">(builder, asset.extensionsUsed);
		}
		if (!asset.extensionsRequired.empty()) {
			builder.append_comma();
			append_key_range<"extensionsRequired">(builder, asset.extensionsRequired);
		}

		if (!asset.accessors.empty()) {
			builder.append_comma();
			writeAccessors(asset, builder);
		}
		if (!asset.animations.empty()) {
			builder.append_comma();
			writeAnimations(asset, builder);
		}
		if (!asset.buffers.empty()) {
			builder.append_comma();
			writeBuffers(asset, builder);
		}
		if (!asset.bufferViews.empty()) {
			builder.append_comma();
			writeBufferViews(asset, builder);
		}
		if (!asset.cameras.empty()) {
			builder.append_comma();
			writeCameras(asset, builder);
		}
		if (!asset.images.empty()) {
			builder.append_comma();
			writeImages(asset, builder);
		}
		if (!asset.materials.empty()) {
			builder.append_comma();
			writeMaterials(asset, builder);
		}
		if (!asset.meshes.empty()) {
			builder.append_comma();
			writeMeshes(asset, builder);
		}
		if (!asset.nodes.empty()) {
			builder.append_comma();
			writeNodes(asset, builder);
		}
		if (!asset.samplers.empty()) {
			builder.append_comma();
			writeSamplers(asset, builder);
		}
		if (!asset.scenes.empty()) {
			builder.append_comma();
			writeScenes(asset, builder);
		}
		if (!asset.skins.empty()) {
			builder.append_comma();
			writeSkins(asset, builder);
		}
		if (!asset.textures.empty()) {
			builder.append_comma();
			writeTextures(asset, builder);
		}

		builder.append_comma();
		writeExtensions(asset, builder);
	}

	if (hasBit(options, ExportOptions::PrettyPrintJson)) {
		// TODO: how do we solve this now
		// return simdjson::prettify(builder);
	}

	return std::string(builder);
}

fg::Expected<fg::ExportResult<std::string>> fg::Exporter::writeGltfJson(const Asset& asset, ExportOptions _options) {
	bufferPaths.clear();
	imagePaths.clear();
	options = _options;
	exportingBinary = false;

	if (hasBit(options, ExportOptions::ValidateAsset)) {
		if (const auto validation = validate(asset); validation != Error::None) {
			return validation;
		}
	}

	// Fairly rudimentary approach of just composing the JSON string using a std::string.
	std::string outputString = writeJson(asset);
	if (errorCode != Error::None) {
		return errorCode;
	}

	ExportResult<std::string> result;
	result.output = std::move(outputString);
	result.bufferPaths = std::move(bufferPaths);
	result.imagePaths = std::move(imagePaths);
	return result;
}

fg::Expected<fg::ExportResult<std::vector<std::byte>>> fg::Exporter::writeGltfBinary(const Asset& asset, ExportOptions _options) {
	bufferPaths.clear();
	imagePaths.clear();
	options = _options;
	exportingBinary = true;

	options &= (~ExportOptions::PrettyPrintJson);

	ExportResult<std::vector<std::byte>> result;
	auto json = writeJson(asset);
	if (errorCode != Error::None) {
		return errorCode;
	}

	result.bufferPaths = std::move(bufferPaths);
	result.imagePaths = std::move(imagePaths);

	// TODO: Add ExportOption enumeration for disabling this?
	const bool withEmbeddedBuffer = !asset.buffers.empty() &&
			// We only support writing Vectors and ByteViews as embedded buffers
			(std::holds_alternative<sources::Array>(asset.buffers.front().data) ||
				std::holds_alternative<sources::ByteView>(asset.buffers.front().data) ||
				std::holds_alternative<sources::Vector>(asset.buffers.front().data)) &&
			asset.buffers.front().byteLength < std::numeric_limits<decltype(internal::BinaryGltfChunk::chunkLength)>::max();

	std::size_t binarySize = 0;
	binarySize += sizeof(internal::BinaryGltfHeader); // glTF header
	binarySize += sizeof(internal::BinaryGltfChunk) + alignUp(json.size(), 4); // JSON chunk
	if (withEmbeddedBuffer) {
		binarySize += sizeof(internal::BinaryGltfChunk) + alignUp(asset.buffers.front().byteLength, 4); // BIN chunk
	}

	// A GLB is limited to 2^32 bytes since the length field in the file header is a 32-bit integer.
	if (binarySize >= static_cast<std::size_t>(std::numeric_limits<decltype(internal::BinaryGltfHeader::length)>::max())) {
		return Error::InvalidGLB;
	}

	result.output.resize(binarySize);
	auto write = [output = result.output.data()](const void* data, std::size_t size) mutable {
		std::memcpy(output, data, size);
		output += size;
	};

	// Write glTF header
	internal::BinaryGltfHeader header {};
	header.magic = internal::binaryGltfHeaderMagic;
	header.version = 2;
	header.length = static_cast<std::uint32_t>(binarySize);
	auto headerBytes = writeBinaryHeader(header);
	write(headerBytes.data(), headerBytes.size());

	// Write JSON chunk
	internal::BinaryGltfChunk jsonChunk {};
	jsonChunk.chunkType = internal::binaryGltfJsonChunkMagic;
	jsonChunk.chunkLength = static_cast<std::uint32_t>(alignUp(json.size(), 4));
	auto chunkBytes = writeBinaryChunk(jsonChunk);
	write(chunkBytes.data(), chunkBytes.size());

	write(json.data(), json.size());

	// 4 bytes padding with space character (0x20)
	for (std::size_t i = json.size(); i % 4 != 0; ++i) {
		static constexpr std::uint8_t space = 0x20U;
		write(&space, sizeof space);
	}

	if (withEmbeddedBuffer) {
		const auto& buffer = asset.buffers.front();

		// Write BIN chunk
		internal::BinaryGltfChunk dataChunk {};
		dataChunk.chunkType = internal::binaryGltfDataChunkMagic;
		dataChunk.chunkLength = static_cast<std::uint32_t>(alignUp(buffer.byteLength, 4));
		chunkBytes = writeBinaryChunk(dataChunk);
		write(chunkBytes.data(), chunkBytes.size());

		std::visit(visitor {
			[](auto&) {},
			[&](const sources::Array& vector) {
				write(vector.bytes.data(), buffer.byteLength);
			},
			[&](const sources::Vector& vector) {
				write(vector.bytes.data(), buffer.byteLength);
			},
			[&](const sources::ByteView& byteView) {
				write(byteView.bytes.data(), buffer.byteLength);
			},
		}, buffer.data);

		// 4 bytes padding with zeros
		for (std::size_t i = buffer.byteLength; i % 4 != 0; ++i) {
			static constexpr std::uint8_t zero = 0x0U;
			write(&zero, sizeof zero);
		}
	}

	return result;
}

namespace fastgltf {
	static std::error_code prepareDir(const fs::path& directory) {
		std::error_code ec;
		if (!directory.empty()) {
			fs::create_directories(directory, ec);
		}
		return ec;
	}

	static bool writeFile(const DataSource& dataSource, const fs::path& baseFolder, const fs::path& filePath) {
		// Get the final normalized path. TODO: Perhaps move these filesystem checks to the parent function?
		auto finalPath = (baseFolder / filePath).lexically_normal();
		if (auto ec = prepareDir(finalPath.parent_path())) {
			return false;
		}

		return std::visit(visitor {
			[](auto&) {
				return false;
			},
			[&](const sources::Array& vector) {
				std::ofstream file(finalPath, std::ios::out | std::ios::binary);
				if (!file.is_open())
					return false;
				file.write(reinterpret_cast<const char *>(vector.bytes.data()),
						   static_cast<std::streamsize>(vector.bytes.size()));
				file.close();
				return file.good();
			},
			[&](const sources::Vector& vector) {
				std::ofstream file(finalPath, std::ios::out | std::ios::binary);
				if (!file.is_open())
					return false;
				file.write(reinterpret_cast<const char *>(vector.bytes.data()),
						   static_cast<std::streamsize>(vector.bytes.size()));
				file.close();
				return file.good();
			},
			[&](const sources::ByteView& view) {
				std::ofstream file(finalPath, std::ios::out | std::ios::binary);
				if (!file.is_open())
					return false;
				file.write(reinterpret_cast<const char *>(view.bytes.data()),
						   static_cast<std::streamsize>(view.bytes.size()));
				file.close();
				return file.good();
			},
		}, dataSource);
	}

	template<typename T>
	bool writeFiles(const Asset& asset, ExportResult<T> &result, const fs::path& baseFolder) {
		for (std::size_t i = 0; i < asset.buffers.size(); ++i) {
			auto& path = result.bufferPaths[i];
			if (path.has_value()) {
				if (!writeFile(asset.buffers[i].data, baseFolder, path.value()))
					return false;
			}
		}

		for (std::size_t i = 0; i < asset.images.size(); ++i) {
			auto& path = result.imagePaths[i];
			if (path.has_value()) {
				if (!writeFile(asset.images[i].data, baseFolder, path.value()))
					return false;
			}
		}
		return true;
	}
} // namespace fastgltf

fg::Error fg::FileExporter::writeGltfJson(const Asset& asset, const fs::path& target, const ExportOptions _options) {
	if (auto ec = prepareDir(target.parent_path())) {
		return Error::InvalidPath;
	}

	auto expected = Exporter::writeGltfJson(asset, _options);

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

	if (!writeFiles(asset, result, target.parent_path())) {
		return Error::FailedWritingFiles;
	}
	return Error::None;
}

fg::Error fg::FileExporter::writeGltfBinary(const Asset& asset, const fs::path& target, const ExportOptions _options) {
	if (auto ec = prepareDir(target.parent_path())) {
		return Error::InvalidPath;
	}

	auto expected = Exporter::writeGltfBinary(asset, _options);

	if (!expected) {
		return expected.error();
	}
	auto& result = expected.get();

	std::ofstream file(target, std::ios::out | std::ios::binary);
	if (!file.is_open()) {
		return Error::InvalidPath;
	}

	file.write(reinterpret_cast<const char*>(result.output.data()),
			   static_cast<std::streamsize>(result.output.size()));

	if (!writeFiles(asset, result, target.parent_path())) {
		return Error::FailedWritingFiles;
	}
	return Error::None;
}
#pragma endregion
