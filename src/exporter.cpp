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

#include <fastgltf/core.hpp>
#include <fastgltf/glb.hpp>
#include <fastgltf/exporter.hpp>
#include <fastgltf/types.hpp>

#include <simdjson.h>

namespace fg = fastgltf;
namespace fs = std::filesystem;

namespace fastgltf::internal {
	[[gnu::always_inline]] inline auto writeBinaryHeader(const BinaryGltfHeader& header) noexcept {
		std::array<std::byte, sizeof(BinaryGltfHeader)> bytes {};
		writeUint32LE(header.magic, &bytes[offsetof(BinaryGltfHeader, magic)]);
		writeUint32LE(header.version, &bytes[offsetof(BinaryGltfHeader, version)]);
		writeUint32LE(header.length, &bytes[offsetof(BinaryGltfHeader, length)]);
		return bytes;
	}

	[[gnu::always_inline]] inline auto writeBinaryChunk(const BinaryGltfChunk& chunk) noexcept {
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
	static void escapeString(std::string& string) {
		std::size_t i = 0;
		do {
			switch (string[i]) {
				case '\"': {
					constexpr std::string_view s = "\\\"";
					string.replace(i, 1, s);
					i += s.size();
					break;
				}
				case '\\': {
					constexpr std::string_view s = "\\\\";
					string.replace(i, 1, s);
					i += s.size();
					break;
				}
				default:
					break;
			}
			++i;
		} while (i < string.size());
	}

	/**
	 * Normalizes the path using lexically_normal, calls generic_string to always use forward slashes,
	 * and escapes any necessary characters.
	 */
	static std::string normalizeAndFormatPath(const fs::path& path) {
		auto string = path.lexically_normal().generic_string();
		escapeString(string);
		return string;
	}

	// replacement for std::to_string that uses simdjson's grisu2 implementation, which is
	// (1) bidirectionally lossless (std::to_string is not)
	// (2) quite a lot faster than std::to_chars and std::to_string.
	template <typename T>
	std::string to_string_fp(const T& value) {
		static_assert(std::is_floating_point_v<T>, "T needs to be a floating point type");
		char buffer[30] = {};

		// TODO: Include a own copy of grisu2 instead of accessing functions from simdjson's internal namespace?
		auto* end = simdjson::internal::to_chars(std::begin(buffer), std::end(buffer), static_cast<double>(value));
		return {std::begin(buffer), end};
	}

	std::string to_string(const math::fvec2 value) {
		return to_string_fp(value[0]) + ',' + to_string_fp(value[1]);
	}

	std::string to_string(const math::fvec3 value) {
		return to_string_fp(value[0]) + ',' + to_string_fp(value[1]) + ',' + to_string_fp(value[2]);
	}

	std::string to_string(const math::fvec4 value) {
		return to_string_fp(value[0]) + ',' + to_string_fp(value[1]) + ',' + to_string_fp(value[2]) + ',' + to_string_fp(value[3]);
	}

	template <internal::TextureInfoType type = internal::TextureInfoType::Standard>
	void writeTextureInfo(std::string& json, const TextureInfo* info) {
		json += '{';
		json += "\"index\":" + std::to_string(info->textureIndex);
		if (info->texCoordIndex != 0) {
			json += ",\"texCoord\":" + std::to_string(info->texCoordIndex);
		}

		if constexpr (type == internal::TextureInfoType::NormalTexture) {
			json += ",\"scale\":" + to_string_fp(reinterpret_cast<const NormalTextureInfo*>(info)->scale);
		} else if constexpr (type == internal::TextureInfoType::OcclusionTexture) {
			json += ",\"strength\":" + to_string_fp(reinterpret_cast<const OcclusionTextureInfo*>(info)->strength);
		}

		if (info->transform.has_value()) {
			json += R"(,"extensions":{"KHR_texture_transform":{)";
			const auto& transform = *info->transform;
			if (transform.uvOffset[0] != 0.0 || transform.uvOffset[1] != 0.0) {
				json += "\"offset\":[" + to_string_fp(transform.uvOffset[0]) + ',' + to_string_fp(transform.uvOffset[1]) + ']';
			}
			if (transform.rotation != 0.0) {
				if (json.back() != '{') json += ',';
				json += "\"rotation\":" + to_string_fp(transform.rotation);
			}
			if (transform.uvScale[0] != 1.0 || transform.uvScale[1] != 1.0) {
				if (json.back() != '{') json += ',';
				json += "\"scale\":[" + to_string_fp(transform.uvScale[0]) + ',' + to_string_fp(transform.uvScale[1]) + ']';
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

std::string fg::escapeString(std::string_view string) {
	std::string ret(string);
	escapeString(ret);
	return ret;
}

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
		json += R"("type":")" + std::string(internal::getAccessorTypeName(it->type)) + "\",";
		json += "\"componentType\":" + std::to_string(getGLComponentType(it->componentType));

		if (it->normalized) {
			json += ",\"normalized\":true";
		}

		if (it->bufferViewIndex.has_value()) {
			json += ",\"bufferView\":" + std::to_string(it->bufferViewIndex.value());
		}

		if (it->sparse.has_value())
		{
			json += R"(,"sparse":{)";
			const auto& sparse = *it->sparse;
			json += "\"count\":" + std::to_string(sparse.count) + ',';
			json += "\"indices\":{";
			json += "\"bufferView\":" + std::to_string(sparse.indicesBufferView) + ',';
			if (sparse.indicesByteOffset != 0) {
				json += "\"byteOffset\":" + std::to_string(sparse.indicesByteOffset) + ',';
			}
			// only get the lower 13 bits
			json += "\"componentType\":" + std::to_string(getGLComponentType(sparse.indexComponentType));
			json += "},";
			json += "\"values\":{";
			json += "\"bufferView\":" + std::to_string(sparse.valuesBufferView);
			if (sparse.valuesByteOffset != 0) {
				json += ",\"byteOffset\":" + std::to_string(sparse.valuesByteOffset);
			}
			json += "}}";
		}

		auto writeMinMax = [&](const optional<AccessorBoundsArray>& ref, const std::string_view name) {
			if (!ref.has_value())
				return; // This is valid, since min/max are only required on specific accessors.
			json += ",\"" + std::string(name) + "\":[";

			for (std::size_t i = 0; i < ref->size(); ++i) {
				if (ref->isType<double>()) {
					json += to_string_fp(ref->get<double>(i));
				} else if (ref->isType<std::int64_t>()) {
					json += std::to_string(ref->get<std::int64_t>(i));
				}
				if (i + 1 < ref->size())
					json += ',';
			}
			json += ']';
		};
		writeMinMax(it->max, "max");
		writeMinMax(it->min, "min");

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.accessors.begin(), it)), fastgltf::Category::Accessors, userPointer);
			if (extras.has_value()) {
				json += std::string(",\"extras\":") + *extras;
			}
		}

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';

		json += '}';
		if (uabs(std::distance(asset.accessors.begin(), it)) + 1 <asset.accessors.size())
			json += ',';
	}
	json += ']';
}

void fg::Exporter::writeAnimations(const Asset& asset, std::string& json)
{
	if (asset.animations.empty())
		return;
	if (json.back() == ']' || json.back() == '}')
		json += ',';

	json += R"("animations":[)";
	for (auto it = asset.animations.begin(); it != asset.animations.end(); ++it) {
		json += '{';

		json += R"("channels":[)";
		for (auto ci = it->channels.begin(); ci != it->channels.end(); ++ci) {
			json += "{";
			json += R"("sampler":)" + std::to_string(ci->samplerIndex) + ",";
			json += R"("target":{)";
			if (ci->nodeIndex.has_value()) {
				json += R"("node":)" + std::to_string(ci->nodeIndex.value()) + ",";
			}
			json += R"("path":")";
			switch (ci->path) {
			case fg::AnimationPath::Translation:
				json += "translation";
				break;
			case fg::AnimationPath::Rotation:
				json += "rotation";
				break;
			case fg::AnimationPath::Scale:
				json += "scale";
				break;
			case fg::AnimationPath::Weights:
				json += "weights";
				break;
			}
			json += "\"}}";

			if (uabs(std::distance(it->channels.begin(), ci)) + 1 < it->channels.size())
				json += ',';
		}
		json += "],";

		json += R"("samplers":[)";
		for (auto si = it->samplers.begin(); si != it->samplers.end(); ++si) {
			json += '{';
			json += R"("input":)" + std::to_string(si->inputAccessor) + ',';

			if (si->interpolation != fg::AnimationInterpolation::Linear) {
				json += R"("interpolation":")";
				if (si->interpolation == fg::AnimationInterpolation::Step) {
					json += "STEP\",";
				} else {
					json += "CUBICSPLINE\",";
				}
			}

			json += R"("output":)" + std::to_string(si->outputAccessor);
			json += '}';

			if (uabs(std::distance(it->samplers.begin(), si)) + 1 < it->samplers.size())
				json += ',';
		}
		json += ']';

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.animations.begin(), it)), fastgltf::Category::Animations, userPointer);
			if (extras.has_value()) {
				json += std::string(",\"extras\":") + *extras;
			}
		}

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';

		json += '}';
		if (uabs(std::distance(asset.animations.begin(), it)) + 1 < asset.animations.size())
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
				json += std::string(R"("uri":")") + fg::normalizeAndFormatPath(path) + '"' + ',';
				bufferPaths.emplace_back(path);
			},
			[&]([[maybe_unused]] const sources::Vector& vector) {
				if (bufferIdx == 0 && exportingBinary) {
					bufferPaths.emplace_back(std::nullopt);
					return;
				}
				auto path = getBufferFilePath(asset, bufferIdx);
				json += std::string(R"("uri":")") + fg::normalizeAndFormatPath(path) + '"' + ',';
				bufferPaths.emplace_back(path);
			},
			[&]([[maybe_unused]] const sources::ByteView& view) {
				if (bufferIdx == 0 && exportingBinary) {
					bufferPaths.emplace_back(std::nullopt);
					return;
				}
				auto path = getBufferFilePath(asset, bufferIdx);
				json += std::string(R"("uri":")") + fg::normalizeAndFormatPath(path) + '"' + ',';
				bufferPaths.emplace_back(path);
			},
			[&](const sources::URI& uri) {
				json += std::string(R"("uri":")") + fg::escapeString(uri.uri->string()) + '"' + ',';
				bufferPaths.emplace_back(std::nullopt);
			},
			[&]([[maybe_unused]] const sources::Fallback& fallback) {
				json += R"("extensions":{"EXT_meshopt_compression":{"fallback":true}},)";
				bufferPaths.emplace_back(std::nullopt);
			},
		}, it->data);

		json += "\"byteLength\":" + std::to_string(it->byteLength);

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.buffers.begin(), it)), fastgltf::Category::Buffers, userPointer);
			if (extras.has_value()) {
				json += std::string(",\"extras\":") + *extras;
			}
		}

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';
		json += '}';
		if (uabs(std::distance(asset.buffers.begin(), it)) + 1 <asset.buffers.size())
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

		if (it->meshoptCompression.has_value()) {
			json += R"(,"extensions":{"EXT_meshopt_compression":{)";
			const auto& meshopt = *it->meshoptCompression;
			json += "\"buffer\":" + std::to_string(meshopt.bufferIndex);
			if (meshopt.byteOffset != 0) {
				json += ",\"byteOffset\":" + std::to_string(meshopt.byteOffset);
			}
			json += ",\"byteLength\":" + std::to_string(meshopt.byteLength);
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

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.bufferViews.begin(), it)), fastgltf::Category::BufferViews, userPointer);
			if (extras.has_value()) {
				json += std::string(",\"extras\":") + *extras;
			}
		}

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';

		json += '}';
		if (uabs(std::distance(asset.bufferViews.begin(), it)) + 1 <asset.bufferViews.size())
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
			[](auto&) {},
			[&](const Camera::Perspective& perspective) {
				json += "\"perspective\":{";

				if (perspective.aspectRatio.has_value()) {
					json += "\"aspectRatio\":" + to_string_fp(perspective.aspectRatio.value()) + ',';
				}

				json += "\"yfov\":" + to_string_fp(perspective.yfov) + ',';

				if (perspective.zfar.has_value()) {
					json += "\"zfar\":" + to_string_fp(perspective.zfar.value()) + ',';
				}

				json += "\"znear\":" + to_string_fp(perspective.znear);

				json += R"(},"type":"perspective")";
			},
			[&](const Camera::Orthographic& orthographic) {
				json += "\"orthographic\":{";
				json += "\"xmag\":" + to_string_fp(orthographic.xmag) + ',';
				json += "\"ymag\":" + to_string_fp(orthographic.ymag) + ',';
				json += "\"zfar\":" + to_string_fp(orthographic.zfar) + ',';
				json += "\"znear\":" + to_string_fp(orthographic.znear);
				json += R"(},"type":"orthographic")";
			}
		}, it->camera);

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.cameras.begin(), it)), fastgltf::Category::Cameras, userPointer);
			if (extras.has_value()) {
				json += std::string(",\"extras\":") + *extras;
			}
		}

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';

		json += '}';
		if (uabs(std::distance(asset.cameras.begin(), it)) + 1 <asset.cameras.size())
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

		auto imageIdx = uabs(std::distance(asset.images.begin(), it));
		std::visit(visitor {
			[&](auto&) {
				errorCode = Error::InvalidGltf;
			},
			[&](const sources::BufferView& bufferView) {
				json += std::string(R"("bufferView":)") + std::to_string(bufferView.bufferViewIndex) + ',';
				json += std::string(R"("mimeType":")") + std::string(internal::getMimeTypeString(bufferView.mimeType)) + '"';
				imagePaths.emplace_back(std::nullopt);
			},
			[&](const sources::Array& vector) {
				auto path = getImageFilePath(asset, imageIdx, vector.mimeType);
				json += std::string(R"("uri":")") + fg::normalizeAndFormatPath(path) + '"';
				if (vector.mimeType != MimeType::None) {
					json += std::string(R"(,"mimeType":")") + std::string(internal::getMimeTypeString(vector.mimeType)) + '"';
				}
				imagePaths.emplace_back(path);
			},
			[&](const sources::Vector& vector) {
				auto path = getImageFilePath(asset, imageIdx, vector.mimeType);
				json += std::string(R"("uri":")") + fg::normalizeAndFormatPath(path) + '"';
				if (vector.mimeType != MimeType::None) {
					json += std::string(R"(,"mimeType":")") + std::string(internal::getMimeTypeString(vector.mimeType)) + '"';
				}
				imagePaths.emplace_back(path);
			},
			[&](const sources::URI& uri) {
				json += std::string(R"("uri":")") + fg::escapeString(uri.uri->string()) + '"';
				if (uri.mimeType != MimeType::None) {
					json += std::string(R"(,"mimeType":")") + std::string(internal::getMimeTypeString(uri.mimeType)) + '"';
				}
				imagePaths.emplace_back(std::nullopt);
			},
		}, it->data);
		if (errorCode != Error::None)
			return;

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.images.begin(), it)), fastgltf::Category::Images, userPointer);
			if (extras.has_value()) {
				json += std::string(",\"extras\":") + *extras;
			}
		}

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';
		json += '}';
		if (uabs(std::distance(asset.images.begin(), it)) + 1 <asset.images.size())
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
		if (!(it->color[0] == 1.0f && it->color[1] == 1.0f && it->color[2] == 1.0f)) {
			json += R"("color":[)";
			json += to_string_fp(it->color[0]) + ',' + to_string_fp(it->color[1]) + ',' + to_string_fp(it->color[2]);
			json += "],";
		}

		if (it->intensity != 1.0f) {
			json += R"("intensity":)" + to_string_fp(it->intensity) + ',';
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
			json += R"(,"range":)" + to_string_fp(it->range.value());
		}

		// Spot cone angles live in a nested "spot" object per
		// KHR_lights_punctual (the parser reads them from there too).
		if (it->type == LightType::Spot && (it->innerConeAngle.has_value() || it->outerConeAngle.has_value())) {
			json += R"(,"spot":{)";
			if (it->innerConeAngle.has_value()) {
				json += R"("innerConeAngle":)" + to_string_fp(it->innerConeAngle.value());
				if (it->outerConeAngle.has_value())
					json += ',';
			}
			if (it->outerConeAngle.has_value())
				json += R"("outerConeAngle":)" + to_string_fp(it->outerConeAngle.value());
			json += '}';
		}

		if (!it->name.empty()) {
			if (json.back() != '{') json += ',';
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';
		}
		json += '}';
		if (uabs(std::distance(asset.lights.begin(), it)) + 1 <asset.lights.size())
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
		if (it->pbrData.baseColorFactor != math::nvec4(1)) {
			json += R"("baseColorFactor":[)";
			json += to_string_fp(it->pbrData.baseColorFactor[0]) + ',' + to_string_fp(it->pbrData.baseColorFactor[1]) + ',' +
				to_string_fp(it->pbrData.baseColorFactor[2]) + ',' + to_string_fp(it->pbrData.baseColorFactor[3]);
			json += "]";
		}

		if (it->pbrData.baseColorTexture.has_value()) {
			if (json.back() != '{') json += ',';
			json += "\"baseColorTexture\":";
			writeTextureInfo(json, &it->pbrData.baseColorTexture.value());
		}

		if (it->pbrData.metallicFactor != 1.0f) {
			if (json.back() != '{') json += ',';
			json += "\"metallicFactor\":" + to_string_fp(it->pbrData.metallicFactor);
		}

		if (it->pbrData.roughnessFactor != 1.0f) {
			if (json.back() != '{') json += ',';
			json += "\"roughnessFactor\":" + to_string_fp(it->pbrData.roughnessFactor);
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
			writeTextureInfo<internal::TextureInfoType::NormalTexture>(json, &it->normalTexture.value());
		}

		if (it->occlusionTexture.has_value()) {
			if (json.back() != ',') json += ',';
			json += "\"occlusionTexture\":";
			writeTextureInfo<internal::TextureInfoType::OcclusionTexture>(json, &it->occlusionTexture.value());
		}

		if (it->emissiveTexture.has_value()) {
			if (json.back() != ',') json += ',';
			json += "\"emissiveTexture\":";
			writeTextureInfo(json, &it->emissiveTexture.value());
		}

		if (it->emissiveFactor != math::nvec3(0)) {
			if (json.back() != ',') json += ',';
			json += R"("emissiveFactor":[)";
			json += to_string_fp(it->emissiveFactor[0]) + ',' + to_string_fp(it->emissiveFactor[1]) + ',' + to_string_fp(it->emissiveFactor[2]);
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
			json += R"("alphaCutoff":)" + to_string_fp(it->alphaCutoff);
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
				json += R"("anisotropyStrength":)" + to_string_fp(it->anisotropy->anisotropyStrength);
			}
			if (it->anisotropy->anisotropyRotation != 0.0f) {
				if (json.back() != '{') json += ',';
				json += R"("anisotropyRotation":)" + to_string_fp(it->anisotropy->anisotropyRotation);
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
				json += R"("clearcoatFactor":)" + to_string_fp(it->clearcoat->clearcoatFactor);
			}
			if (it->clearcoat->clearcoatTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"clearcoatTexture\":";
				writeTextureInfo(json, &it->clearcoat->clearcoatTexture.value());
			}
			if (it->clearcoat->clearcoatRoughnessFactor != 0.0f) {
				if (json.back() != '{') json += ',';
				json += R"("clearcoatRoughnessFactor":)" + to_string_fp(it->clearcoat->clearcoatRoughnessFactor);
			}
			if (it->clearcoat->clearcoatRoughnessTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"clearcoatRoughnessTexture\":";
				writeTextureInfo(json, &it->clearcoat->clearcoatRoughnessTexture.value());
			}
			if (it->clearcoat->clearcoatNormalTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"clearcoatNormalTexture\":";
				writeTextureInfo<internal::TextureInfoType::NormalTexture>(json, &it->clearcoat->clearcoatNormalTexture.value());
			}
			json += '}';
		}

		if (it->diffuseTransmission) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_diffuse_transmission":{)";
			if (it->diffuseTransmission->diffuseTransmissionFactor != 0.0f) {
				json += R"("diffuseTransmissionFactor":)" + to_string_fp(it->diffuseTransmission->diffuseTransmissionFactor);
			}
			if (it->diffuseTransmission->diffuseTransmissionTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += R"("diffuseTransmissionTexture":)";
				writeTextureInfo(json, &it->diffuseTransmission->diffuseTransmissionTexture.value());
			}
			if (it->diffuseTransmission->diffuseTransmissionColorFactor != math::nvec3(1)) {
				if (json.back() != '{') json += ',';
				json += R"("diffuseTransmissionColorFactor":[)";
				json += to_string_fp(it->diffuseTransmission->diffuseTransmissionColorFactor[0]) + ',' +
						to_string_fp(it->diffuseTransmission->diffuseTransmissionColorFactor[1]) + ',' +
						to_string_fp(it->diffuseTransmission->diffuseTransmissionColorFactor[2]) + ']';
			}
			if (it->diffuseTransmission->diffuseTransmissionColorTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"diffuseTransmissionColorTexture\":";
				writeTextureInfo(json, &it->diffuseTransmission->diffuseTransmissionColorTexture.value());
			}
			json += '}';
		}

		if (it->dispersion != 0.0f) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_dispersion":{"dispersion":)" + to_string_fp(it->dispersion) + '}';
		}

		if (it->emissiveStrength != 1.0f) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_emissive_strength":{"emissiveStrength":)" + to_string_fp(it->emissiveStrength) + '}';
		}

		if (it->ior != 1.5f) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_ior":{"ior":)" + to_string_fp(it->ior) + '}';
		}

		if (it->iridescence) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_iridescence":{)";
			if (it->iridescence->iridescenceFactor != 0.0f) {
				json += R"("iridescenceFactor":)" + to_string_fp(it->iridescence->iridescenceFactor);
			}
			if (it->iridescence->iridescenceTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"iridescenceTexture\":";
				writeTextureInfo(json, &it->iridescence->iridescenceTexture.value());
			}
			if (it->iridescence->iridescenceIor != 1.3f) {
				if (json.back() != '{') json += ',';
				json += R"("iridescenceIor":)" + to_string_fp(it->iridescence->iridescenceIor);
			}
			if (it->iridescence->iridescenceThicknessMinimum != 100.0f) {
				if (json.back() != '{') json += ',';
				json += R"("iridescenceThicknessMinimum":)" + to_string_fp(it->iridescence->iridescenceThicknessMinimum);
			}
			if (it->iridescence->iridescenceThicknessMaximum != 400.0f) {
				if (json.back() != '{') json += ',';
				json += R"("iridescenceThicknessMaximum":)" + to_string_fp(it->iridescence->iridescenceThicknessMaximum);
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
			if (it->sheen->sheenColorFactor != math::nvec3(0)) {
				json += R"("sheenColorFactor":[)" +
					to_string_fp(it->sheen->sheenColorFactor[0]) + ',' +
					to_string_fp(it->sheen->sheenColorFactor[1]) + ',' +
					to_string_fp(it->sheen->sheenColorFactor[2]) + ']';
			}
			if (it->sheen->sheenColorTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"sheenColorTexture\":";
				writeTextureInfo(json, &it->sheen->sheenColorTexture.value());
			}
			if (it->sheen->sheenRoughnessFactor != 0.0f) {
				if (json.back() != '{') json += ',';
				json += R"("sheenRoughnessFactor":)" + to_string_fp(it->sheen->sheenRoughnessFactor);
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
				json += R"("specularFactor":)" + to_string_fp(it->specular->specularFactor);
			}
			if (it->specular->specularTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"specularTexture\":";
				writeTextureInfo(json, &it->specular->specularTexture.value());
			}
			if (it->specular->specularColorFactor != math::nvec3(1)) {
				if (json.back() != '{') json += ',';
				json += R"("specularColorFactor":[)" +
						to_string_fp(it->specular->specularColorFactor[0]) + ',' +
						to_string_fp(it->specular->specularColorFactor[1]) + ',' +
						to_string_fp(it->specular->specularColorFactor[2]) + ']';
			}
			if (it->specular->specularColorTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"specularColorTexture\":";
				writeTextureInfo(json, &it->specular->specularColorTexture.value());
			}
			json += '}';
		}
		if (it->specularGlossiness)
		{
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_pbrSpecularGlossiness":{)";
			if (it->specularGlossiness->diffuseFactor != math::nvec4(1)) {
				json += R"("diffuseFactor":[)" +
						to_string_fp(it->specularGlossiness->diffuseFactor[0]) + ',' +
						to_string_fp(it->specularGlossiness->diffuseFactor[1]) + ',' +
						to_string_fp(it->specularGlossiness->diffuseFactor[2]) + ',' +
						to_string_fp(it->specularGlossiness->diffuseFactor[3]) + ']';
			}
			if (it->specularGlossiness->diffuseTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"diffuseTexture\":";
				writeTextureInfo(json, &it->specularGlossiness->diffuseTexture.value());
			}
			if (it->specularGlossiness->specularFactor != math::nvec3(1)) {
				if (json.back() != '{') json += ',';
				json += R"("specularFactor":[)" +
						to_string_fp(it->specularGlossiness->specularFactor[0]) + ',' +
						to_string_fp(it->specularGlossiness->specularFactor[1]) + ',' +
						to_string_fp(it->specularGlossiness->specularFactor[2]) + ']';
			}
			if (it->specularGlossiness->glossinessFactor != 1.0) {
				if (json.back() != '{') json += ',';
				json += R"("glossinessFactor":)" + to_string_fp(it->specularGlossiness->glossinessFactor);
			}
			if (it->specularGlossiness->specularGlossinessTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"specularGlossinessTexture\":";
				writeTextureInfo(json, &it->specularGlossiness->specularGlossinessTexture.value());
			}
			json += '}';
		}

		if (it->transmission) {
			if (json.back() == '}') json += ',';
			json += R"("KHR_materials_transmission":{)";
			if (it->transmission->transmissionFactor != 0.0f) {
				json += R"("transmissionFactor":)" + to_string_fp(it->transmission->transmissionFactor);
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
				json += R"("thicknessFactor":)" + to_string_fp(it->volume->thicknessFactor);
			}
			if (it->volume->thicknessTexture.has_value()) {
				if (json.back() != '{') json += ',';
				json += "\"thicknessTexture\":";
				writeTextureInfo(json, &it->volume->thicknessTexture.value());
			}
			if (it->volume->attenuationDistance != std::numeric_limits<num>::infinity()) {
				if (json.back() != '{') json += ',';
				json += R"("attenuationDistance":)" + to_string_fp(it->volume->attenuationDistance);
			}
			if (it->volume->attenuationColor != math::nvec3(1)) {
				if (json.back() != '{') json += ',';
				json += R"("attenuationColor":[)" +
						to_string_fp(it->volume->attenuationColor[0]) + ',' +
						to_string_fp(it->volume->attenuationColor[1]) + ',' +
						to_string_fp(it->volume->attenuationColor[2]) + ']';
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
			json += R"("MSFT_packing_occlusionRoughnessMetallic":{)";
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

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.materials.begin(), it)), fastgltf::Category::Materials, userPointer);
			if (extras.has_value()) {
				if (json.back() != '{')
					json += ',';
				json += std::string("\"extras\":") + *extras;
			}
		}

		if (!it->name.empty()) {
			if (json.back() != ',') json += ',';
			json += R"("name":")" + fg::escapeString(it->name) + '"';
		}
		json += '}';
		if (uabs(std::distance(asset.materials.begin(), it)) + 1 <asset.materials.size())
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
						json += '"' + std::string(ita->name) + "\":" + std::to_string(ita->accessorIndex);
						if (uabs(std::distance(itp->attributes.begin(), ita)) + 1 <itp->attributes.size())
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

				if (!itp->targets.empty())
				{
					json  += R"(,"targets":[)";
					for (auto itt = itp->targets.begin(); itt != itp->targets.end(); ++itt) {
						json += '{';
						for (auto ita = itt->begin(); ita != itt->end(); ++ita) {
							json += '"' + std::string(ita->name) + "\":" + std::to_string(ita->accessorIndex);
							if (uabs(std::distance(itt->begin(), ita)) + 1 < itt->size())
								json += ',';
						}
						json += '}';
						if (uabs(std::distance(itp->targets.begin(), itt)) + 1 < itp->targets.size())
							json += ',';
					}
					json += ']';
				}

				if (itp->type != PrimitiveType::Triangles) {
					json += R"(,"mode":)" + std::to_string(to_underlying(itp->type));
				}

				const bool hasExtensions = !itp->mappings.empty() || itp->dracoCompression;
				if (hasExtensions) {
					json += R"(,"extensions":{)";
				}
				if (!itp->mappings.empty()) {
					json += R"("KHR_materials_variants":{"mappings":[)";
					// TODO: We should optimise to avoid writing multiple objects for the same material index
					for (std::size_t i = 0; i < asset.materialVariants.size(); ++i) {
						if (!itp->mappings[i].has_value())
							continue;
						if (json.back() == '}')
							json += ',';
						json += "{\"material\":" + std::to_string(itp->mappings[i].value()) + ",\"variants\":[" + std::to_string(i) + "]}";
					}
					json += "]}";
				}
				if (itp->dracoCompression) {
					if (!itp->mappings.empty())
						json += ',';

					json += R"("KHR_draco_mesh_compression":{)";
					json += R"("bufferView":)" + std::to_string(itp->dracoCompression->bufferView) + ',';
					json += R"("attributes":{)";
					for (auto ita = itp->dracoCompression->attributes.begin(); ita != itp->dracoCompression->attributes.end(); ++ita) {
						json += '"' + std::string(ita->name) + "\":" + std::to_string(ita->accessorIndex);
						if (uabs(std::distance(itp->dracoCompression->attributes.begin(), ita)) + 1 < itp->dracoCompression->attributes.size())
							json += ',';
					}
					json += "}}";
				}
				if (hasExtensions) {
					json += "}";
				}

				json += '}';
				++itp;
				if (uabs(std::distance(it->primitives.begin(), itp)) < it->primitives.size())
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
				json += to_string_fp(*itw);
				++itw;
				if (uabs(std::distance(it->weights.begin(), itw)) < it->weights.size())
					json += ',';
			}
			json += ']';
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.meshes.begin(), it)), fastgltf::Category::Meshes, userPointer);
			if (extras.has_value()) {
				if (json.back() != '{')
					json += ',';
				json += std::string("\"extras\":") + *extras;
			}
		}

		if (!it->name.empty()) {
			if (json.back() != '{')
				json += ',';
			json += R"("name":")" + fg::escapeString(it->name) + '"';
		}
		json += '}';
		if (uabs(std::distance(asset.meshes.begin(), it)) + 1 <asset.meshes.size())
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
				if (uabs(std::distance(it->children.begin(), itc)) < it->children.size())
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
				json += to_string_fp(*itw);
				++itw;
				if (uabs(std::distance(it->weights.begin(), itw)) < it->weights.size())
					json += ',';
			}
			json += ']';
		}

		visit_exhaustive(visitor {
			[&](const TRS& trs) {
				if (trs.rotation != math::fquat(0.f, 0.f, 0.f, 1.f)) {
					if (json.back() != '{')
						json += ',';
					json += R"("rotation":[)";
					json += to_string_fp(trs.rotation[0]) + ',' + to_string_fp(trs.rotation[1]) + ',' + to_string_fp(trs.rotation[2]) + ',' + to_string_fp(trs.rotation[3]);
					json += "]";
				}

				if (trs.scale != math::fvec3(1.f)) {
					if (json.back() != '{')
						json += ',';
					json += R"("scale":[)";
					json += to_string_fp(trs.scale[0]) + ',' + to_string_fp(trs.scale[1]) + ',' + to_string_fp(trs.scale[2]);
					json += "]";
				}

				if (trs.translation != math::fvec3(0.f)) {
					if (json.back() != '{')
						json += ',';
					json += R"("translation":[)";
					json += to_string_fp(trs.translation[0]) + ',' + to_string_fp(trs.translation[1]) + ',' + to_string_fp(trs.translation[2]);
					json += "]";
				}
			},
			[&](const math::fmat4x4& matrix) {
				if (json.back() != '{')
					json += ',';
				json += R"("matrix":[)";
				for (std::size_t i = 0; i < matrix.columns(); ++i) {
					for (std::size_t j = 0; j < matrix.rows(); ++j) {
						json += to_string_fp(matrix.col(i)[j]);
						if (i * matrix.columns() + j + 1 < matrix.columns() * matrix.rows()) {
							json += ',';
						}
					}
				}
				json += ']';
			},
		}, it->transform);

		if (!it->instancingAttributes.empty() || it->lightIndex.has_value()
#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
			|| it->physicsRigidBody
#endif
			|| !it->visible || !it->selectable || !it->hoverable
			) {
			if (json.back() != '{') json += ',';
			json += R"("extensions":{)";
			if (!it->instancingAttributes.empty()) {
				json += R"("EXT_mesh_gpu_instancing":{"attributes":{)";
				for (auto ait = it->instancingAttributes.begin(); ait != it->instancingAttributes.end(); ++ait) {
					json += '"' + std::string(ait->name) + "\":" + std::to_string(ait->accessorIndex);
					if (uabs(std::distance(it->instancingAttributes.begin(), ait)) + 1 <
						it->instancingAttributes.size())
						json += ',';
				}
				json += "}}";
			}
			if (it->lightIndex.has_value()) {
				if (json.back() != '{') json += ',';
				json += R"("KHR_lights_punctual":{"light":)" + std::to_string(it->lightIndex.value()) + '}';
			}

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
			if (it->physicsRigidBody) {
				if (json.back() != '{') json += ',';
				json += R"("KHR_physics_rigid_bodies":{)";

				const auto writeGeometry = [&json](const Geometry& geom) {
					json += R"("geometry":{)";
					if (geom.shape.has_value()) {
						json += R"("shape":)" + std::to_string(*geom.shape);
					} else if (geom.mesh.has_value()) {
						json += R"("mesh":)" + std::to_string(*geom.mesh);
					}
					if (geom.convexHull) {
						if (json.back() != '{') json += ',';
						json += R"("convexHull":true)";
					}
					json += '}';
				};

				if (it->physicsRigidBody->motion.has_value()) {
					const auto& motion = *it->physicsRigidBody->motion;
					json += R"("motion":{)";
					if (motion.isKinematic) json += R"("isKinematic":true)";
					if (motion.mass.has_value()) {
						if (json.back() != '{') json += ',';
						json += R"("mass":)" + to_string_fp(*motion.mass);
					}
					if (motion.centerOfMass != math::fvec3(0)) {
						if (json.back() != '{') json += ',';
						json += R"("centerOfMass":[)" + to_string_fp(motion.centerOfMass[0]) + ',' + to_string_fp(motion.centerOfMass[1]) + ',' + to_string_fp(motion.centerOfMass[2]) + ']';
					}
					if (motion.inertialDiagonal.has_value()) {
						if (json.back() != '{') json += ',';
						const auto& d = *motion.inertialDiagonal;
						json += R"("inertiaDiagonal":[)" + to_string_fp(d[0]) + ',' + to_string_fp(d[1]) + ',' + to_string_fp(d[2]) + ']';
					}
					if (motion.inertialOrientation.has_value()) {
						if (json.back() != '{') json += ',';
						const auto& q = *motion.inertialOrientation;
						json += R"("inertiaOrientation":[)" + to_string_fp(q[0]) + ',' + to_string_fp(q[1]) + ',' + to_string_fp(q[2]) + ',' + to_string_fp(q[3]) + ']';
					}
					if (motion.linearVelocity != math::fvec3(0)) {
						if (json.back() != '{') json += ',';
						json += R"("linearVelocity":[)" + to_string_fp(motion.linearVelocity[0]) + ',' + to_string_fp(motion.linearVelocity[1]) + ',' + to_string_fp(motion.linearVelocity[2]) + ']';
					}
					if (motion.angularVelocity != math::fvec3(0)) {
						if (json.back() != '{') json += ',';
						json += R"("angularVelocity":[)" + to_string_fp(motion.angularVelocity[0]) + ',' + to_string_fp(motion.angularVelocity[1]) + ',' + to_string_fp(motion.angularVelocity[2]) + ']';
					}
					if (motion.gravityFactor != num(1)) {
						if (json.back() != '{') json += ',';
						json += R"("gravityFactor":)" + to_string_fp(motion.gravityFactor);
					}
					json += '}';
				}

				if (it->physicsRigidBody->collider.has_value()) {
					const auto& collider = *it->physicsRigidBody->collider;
					if (json.back() != '{') json += ',';
					json += R"("collider":{)";
					writeGeometry(collider.geometry);
					if (collider.physicsMaterial.has_value()) {
						json += R"(,"physicsMaterial":)" + std::to_string(*collider.physicsMaterial);
					}
					if (collider.collisionFilter.has_value()) {
						json += R"(,"collisionFilter":)" + std::to_string(*collider.collisionFilter);
					}
					json += '}';
				}

				if (it->physicsRigidBody->trigger.has_value()) {
					if (json.back() != '{') json += ',';
					json += R"("trigger":{)";
					visit_exhaustive(visitor{
						[&](const GeometryTrigger& gt) {
							writeGeometry(gt.geometry);
							if (gt.collisionFilter.has_value()) {
								json += R"(,"collisionFilter":)" + std::to_string(*gt.collisionFilter);
							}
						},
						[&](const NodeTrigger& nt) {
							json += R"("nodes":[)";
							for (auto nit = nt.nodes.begin(); nit != nt.nodes.end(); ++nit) {
								if (nit != nt.nodes.begin()) json += ',';
								json += std::to_string(*nit);
							}
							json += ']';
						}
					}, *it->physicsRigidBody->trigger);
					json += '}';
				}

				if (it->physicsRigidBody->joint.has_value()) {
					const auto& joint = *it->physicsRigidBody->joint;
					if (json.back() != '{') json += ',';
					json += R"("joint":{"connectedNode":)" + std::to_string(joint.connectedNode);
					json += R"(,"joint":)" + std::to_string(joint.joint);
					if (joint.enableCollision) json += R"(,"enableCollision":true)";
					json += '}';
				}

				json += '}';
			}
#endif

			if (!it->visible) {
				if (json.back() != '{') json += ',';
				json += R"("KHR_node_visibility":{"visible":false})";
			}

			if (!it->selectable) {
				if (json.back() != '{') json += ',';
				json += R"("KHR_node_selectability":{"selectable":false})";
			}

			if (!it->hoverable) {
				if (json.back() != '{') json += ',';
				json += R"("KHR_node_hoverability":{"hoverable":false})";
			}

			json += "}";
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.nodes.begin(), it)), fastgltf::Category::Nodes, userPointer);
			if (extras.has_value()) {
				if (json.back() != '{')
					json += ',';
				json += std::string("\"extras\":") + *extras;
			}
		}

		if (!it->name.empty()) {
			if (json.back() != '{')
				json += ',';
			json += R"("name":")" + fg::escapeString(it->name) + '"';
		}
		json += '}';
		if (uabs(std::distance(asset.nodes.begin(), it)) + 1 <asset.nodes.size())
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
			json += R"("wrapT":)" + std::to_string(to_underlying(it->wrapT));
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.samplers.begin(), it)), fastgltf::Category::Samplers, userPointer);
			if (extras.has_value()) {
				if (json.back() != '{')
					json += ',';
				json += std::string("\"extras\":") + *extras;
			}
		}

		if (!it->name.empty()) {
			if (json.back() != '{') json += ',';
			json += R"("name":")" + fg::escapeString(it->name) + '"';
		}
		json += '}';
		if (uabs(std::distance(asset.samplers.begin(), it)) + 1 <asset.samplers.size())
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

		// scene.nodes is optional (minItems:1), so omit when empty.
		if (!it->nodeIndices.empty()) {
			json += R"("nodes":[)";
			auto itn = it->nodeIndices.begin();
			while (itn != it->nodeIndices.end()) {
				json += std::to_string(*itn);
				++itn;
				if (uabs(std::distance(it->nodeIndices.begin(), itn)) < it->nodeIndices.size())
					json += ',';
			}
			json += ']';
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.scenes.begin(), it)), fastgltf::Category::Scenes, userPointer);
			if (extras.has_value()) {
				if (json.back() != '{')
					json += ',';
				json += std::string("\"extras\":") + *extras;
			}
		}

		if (!it->name.empty()) {
			if (json.back() != '{')
				json += ',';
			json += R"("name":")" + fg::escapeString(it->name) + '"';
		}
		json += '}';
		if (uabs(std::distance(asset.scenes.begin(), it)) + 1 <asset.scenes.size())
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
			if (uabs(std::distance(it->joints.begin(), itj)) < it->joints.size())
				json += ',';
		}
		json += ']';

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.skins.begin(), it)), fastgltf::Category::Skins, userPointer);
			if (extras.has_value()) {
				if (json.back() != '{')
					json += ',';
				json += std::string("\"extras\":") + *extras;
			}
		}

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';
		json += '}';
		if (uabs(std::distance(asset.skins.begin(), it)) + 1 <asset.skins.size())
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

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.textures.begin(), it)), fastgltf::Category::Textures, userPointer);
			if (extras.has_value()) {
				if (json.back() != '{')
					json += ',';
				json += std::string("\"extras\":") + *extras;
			}
		}

		if (!it->name.empty())
			json += R"(,"name":")" + fg::escapeString(it->name) + '"';
		json += '}';
		if (uabs(std::distance(asset.textures.begin(), it)) + 1 <asset.textures.size())
			json += ',';
	}
	json += ']';
}

#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
void fg::Exporter::writeShapes(const Asset& asset, std::string& json) {
	if (asset.shapes.empty()) {
		return;
	}
	if (json.back() == ']' || json.back() == '}') {
		json += ',';
	}

	json += R"("KHR_implicit_shapes":{"shapes":[)";
	for (auto it = asset.shapes.begin(); it != asset.shapes.end(); ++it) {
		const auto& shape = *it;
		json += '{';

		visit_exhaustive(visitor{
			[&](const SphereShape& sphere) {
				json += R"("type":"sphere","sphere":{"radius":)" + to_string_fp(sphere.radius) + '}';
			},
			[&](const BoxShape& box) {
				json += R"("type":"box","box":{"size":[)" + to_string(box.size) + "]}";
			},
			[&](const CylinderShape& cylinder) {
				json += R"("type":"cylinder","cylinder":{"height":)" + to_string_fp(cylinder.height)
					+ R"(,"radiusBottom":)" + to_string_fp(cylinder.radiusBottom) + R"(,"radiusTop":)"
					+ to_string_fp(cylinder.radiusTop) + "}";;
			},
			[&](const CapsuleShape& capsule) {
				json += R"("type":"capsule","capsule":{"height":)" + to_string_fp(capsule.height)
					+ R"(,"radiusBottom":)" + to_string_fp(capsule.radiusBottom)
					+ R"(,"radiusTop":)" + to_string_fp(capsule.radiusTop) + '}';
			},
			[&](const PlaneShape& plane) {
				json += R"("type":"plane","plane":{"sizeX":)" + to_string_fp(plane.sizeX)
					+ R"(,"sizeZ":)" + to_string_fp(plane.sizeZ)
					+ R"(,"doubleSided":)" + (plane.doubleSided ? "true" : "false") + '}';
			}
			},
			shape);

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.shapes.begin(), it)), fastgltf::Category::Shapes, userPointer);
			if (extras.has_value()) {
				if (json.back() != '{') {
					json += ',';
				}
				json += std::string("\"extras\":") + *extras;
			}
		}

		json += '}';

		if (uabs(std::distance(asset.shapes.begin(), it)) + 1 < asset.shapes.size()) {
			json += ',';
		}
	}
	json += "]}";
}
#endif

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
void fg::Exporter::writePhysicsMaterials(const Asset& asset, std::string& json) {
	if (asset.physicsMaterials.empty()) {
		return;
	}
	if (json.back() == ']' || json.back() == '}') {
		json += ',';
	}

	json += R"("physicsMaterials":[)";

	for (auto it = asset.physicsMaterials.begin(); it != asset.physicsMaterials.end(); ++it) {
		const auto& material = *it;

		json += R"({"staticFriction":)" + to_string_fp(material.staticFriction)
			+ R"(,"dynamicFriction":)" + to_string_fp(material.dynamicFriction)
			+ R"(,"restitution":)" + to_string_fp(material.restitution)
			+ R"(,"frictionCombine":)";
		switch (material.frictionCombine) {
		case CombineMode::Average:
		case CombineMode::Invalid:
			json += R"("average")";
			break;
		case CombineMode::Minimum:
			json += R"("minimum")";
			break;
		case CombineMode::Maximum:
			json += R"("maximum")";
			break;
		case CombineMode::Multiply:
			json += R"("multiply")";
			break;
		}
		json += R"(,"restitutionCombine":)";
		switch (material.restitutionCombine) {
		case CombineMode::Average:
		case CombineMode::Invalid:
			json += R"("average")";
			break;
		case CombineMode::Minimum:
			json += R"("minimum")";
			break;
		case CombineMode::Maximum:
			json += R"("maximum")";
			break;
		case CombineMode::Multiply:
			json += R"("multiply")";
			break;
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.physicsMaterials.begin(), it)), fastgltf::Category::PhysicsMaterials, userPointer);
			if (extras.has_value()) {
				if (json.back() != '{') {
					json += ',';
				}
				json += std::string("\"extras\":") + *extras;
			}
		}

		json += '}';

		if (uabs(std::distance(asset.physicsMaterials.begin(), it)) + 1 < asset.physicsMaterials.size()) {
			json += ',';
		}
	}

	json += ']';
}

void fg::Exporter::writeCollisionFilters(const Asset& asset, std::string& json) {
	if (asset.collisionFilters.empty()) {
		return;
	}
	if (json.back() == ']' || json.back() == '}') {
		json += ',';
	}

	json += R"("collisionFilters":[)";

	for (auto it = asset.collisionFilters.begin(); it != asset.collisionFilters.end(); ++it) {
		const auto& filter = *it;

		json += '{';

		if (!filter.collisionSystems.empty()) {
			json += R"("collisionSystems":[)";
			for (auto systemIt = filter.collisionSystems.begin(); systemIt != filter.collisionSystems.end(); ++systemIt) {
				json += '"';
				json += *systemIt;
				json += '"';

				if (uabs(std::distance(filter.collisionSystems.begin(), systemIt)) + 1 < filter.collisionSystems.size()) {
					json += ',';
				}
			}
			json += ']';
		}
		if (json.back() == ']') {
			json += ',';
		}

		if (!filter.collideWithSystems.empty()) {
			json += R"("collideWithSystems":[)";
			for (auto systemIt = filter.collideWithSystems.begin(); systemIt != filter.collideWithSystems.end(); ++systemIt) {
				json += '"';
				json += *systemIt;
				json += '"';

				if (uabs(std::distance(filter.collideWithSystems.begin(), systemIt)) + 1 < filter.collideWithSystems.size()) {
					json += ',';
				}
			}
			json += ']';
		}
		if (json.back() == ']') {
			json += ',';
		}

		if (!filter.notCollideWithSystems.empty()) {
			json += R"("notCollideWithSystems":[)";
			for (auto systemIt = filter.notCollideWithSystems.begin(); systemIt != filter.notCollideWithSystems.end(); ++systemIt) {
				json += '"';
				json += *systemIt;
				json += '"';

				if (uabs(std::distance(filter.notCollideWithSystems.begin(), systemIt)) + 1 < filter.notCollideWithSystems.size()) {
					json += ',';
				}
			}
			json += ']';
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.collisionFilters.begin(), it)), fastgltf::Category::Shapes, userPointer);
			if (extras.has_value()) {
				if (json.back() != '{') {
					json += ',';
				}
				json += std::string("\"extras\":") + *extras;
			}
		}

		if (json.back() == ',') json.pop_back();
		json += '}';

		if (uabs(std::distance(asset.collisionFilters.begin(), it)) + 1 < asset.collisionFilters.size()) {
			json += ',';
		}
	}

	json += ']';
}

void fg::Exporter::writePhysicsJoints(const Asset& asset, std::string& json) {
	if (asset.physicsJoints.empty()) {
		return;
	}
	if (json.back() == ']' || json.back() == '}') {
		json += ',';
	}

	json += R"("physicsJoints":[)";

	for (auto it = asset.physicsJoints.begin(); it != asset.physicsJoints.end(); ++it) {
		const auto& joint = *it;

		json += '{';

		if (!joint.limits.empty()) {
			json += R"("limits":[)";
			for (auto limitIt = joint.limits.begin(); limitIt != joint.limits.end(); ++limitIt) {
				const auto& limit = *limitIt;

				json += '{';
				if (!limit.linearAxes.empty()) {
					json += R"("linearAxes":[)";
					for (auto axisIt = limit.linearAxes.begin(); axisIt != limit.linearAxes.end(); ++axisIt) {
						json += std::to_string(*axisIt);

						if (uabs(std::distance(limit.linearAxes.begin(), axisIt)) + 1 < limit.linearAxes.size()) {
							json += ',';
						}
					}
					json += "],";
				}
				if (!limit.angularAxes.empty()) {
					json += R"("angularAxes":[)";
					for (auto axisIt = limit.angularAxes.begin(); axisIt != limit.angularAxes.end(); ++axisIt) {
						json += std::to_string(*axisIt);

						if (uabs(std::distance(limit.angularAxes.begin(), axisIt)) + 1 < limit.angularAxes.size()) {
							json += ',';
						}
					}
					json += "],";
				}
				if (limit.min) {
					json += R"("min":)" + to_string_fp(*limit.min) + ',';
				}
				if (limit.max) {
					json += R"("max":)" + to_string_fp(*limit.max) + ',';
				}
				if (limit.stiffness) {
					json += R"("stiffness":)" + to_string_fp(*limit.stiffness) + ',';
				}
				json += R"("damping":)" + to_string_fp(limit.damping);
				json += '}';

				if (uabs(std::distance(joint.limits.begin(), limitIt)) + 1 < joint.limits.size()) {
					json += ',';
				}
			}
			json += ']';

			if(!joint.drives.empty()) {
				json += ',';
			}
		}

		if (!joint.drives.empty()) {
			json += R"("drives":[)";
			for (auto driveIt = joint.drives.begin(); driveIt != joint.drives.end(); ++driveIt) {
				const auto& drive = *driveIt;

				json += R"({"type":)";
				switch(drive.type) {
				case DriveType::Linear:
				case DriveType::Invalid:
					json += R"("linear")";
					break;
				case DriveType::Angular:
					json += R"("angular")";
					break;
				}

				json += R"(,"mode":)";
				switch (drive.mode) {
				case DriveMode::Force:
				case DriveMode::Invalid:
					json += R"("force")";
					break;
				case DriveMode::Acceleration:
					json += R"("acceleration")";
					break;
				}

				json += R"(,"axis":)" + std::to_string(drive.axis)
					+ R"(,"maxForce":)" + to_string_fp(drive.maxForce)
					+ R"(,"positionTarget":)" + to_string_fp(drive.positionTarget)
					+ R"(,"velocityTarget":)" + to_string_fp(drive.velocityTarget)
					+ R"(,"stiffness":)" + to_string_fp(drive.stiffness)
					+ R"(,"damping":)" + to_string_fp(drive.damping);

				json += '}';

				if (uabs(std::distance(joint.drives.begin(), driveIt)) + 1 < joint.drives.size()) {
					json += ',';
				}
			}

			json += ']';
		}

		if (extrasWriteCallback != nullptr) {
			auto extras = extrasWriteCallback(uabs(std::distance(asset.physicsJoints.begin(), it)), fastgltf::Category::PhysicsJoints, userPointer);
			if (extras.has_value()) {
				if (json.back() != '{') {
					json += ',';
				}
				json += std::string("\"extras\":") + *extras;
			}
		}

		json += "}";

		if (uabs(std::distance(asset.physicsJoints.begin(), it)) + 1 < asset.physicsJoints.size()) {
			json += ',';
		}
	}

	json += "]";
}
#endif

void fg::Exporter::writeExtensions(const fastgltf::Asset& asset, std::string& json) {
	if (json.back() == ']' || json.back() == '}')
		json += ',';
	json += "\"extensions\":{";

	writeLights(asset, json);

#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
	writeShapes(asset, json);
#endif

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
	if (!asset.physicsMaterials.empty() || !asset.collisionFilters.empty() || !asset.physicsJoints.empty()) {
		if (json.back() == ']' || json.back() == '}') {
			json += ',';
		}
		json += R"("KHR_physics_rigid_bodies":{)";
		writePhysicsMaterials(asset, json);
		writeCollisionFilters(asset, json);
		writePhysicsJoints(asset, json);
		json += '}';
	}
#endif

	if (!asset.materialVariants.empty()) {
		if (json.back() != '{')
			json += ',';
		json += R"("KHR_materials_variants":{"variants":[)";
		for (const auto& variant : asset.materialVariants) {
			if (json.back() == '}')
				json += ',';
			json += R"({"name":")" + variant + "\"}";
		}
		json += "]}";
	}

	json += '}';
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
		if (!asset.assetInfo->minVersion.empty())
			outputString += R"("minVersion":")" + fg::escapeString(asset.assetInfo->minVersion) + "\",";
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
			if (uabs(std::distance(asset.extensionsUsed.begin(), it)) + 1 <asset.extensionsUsed.size())
				outputString += ',';
		}
		outputString += ']';
	}
	if (!asset.extensionsRequired.empty()) {
		if (outputString.back() != '{') outputString += ',';
		outputString += "\"extensionsRequired\":[";
		for (auto it = asset.extensionsRequired.begin(); it != asset.extensionsRequired.end(); ++it) {
			outputString += '\"' + *it + '\"';
			if (uabs(std::distance(asset.extensionsRequired.begin(), it)) + 1 <asset.extensionsRequired.size())
				outputString += ',';
		}
		outputString += ']';
	}

	writeAccessors(asset, outputString);
	writeAnimations(asset, outputString);
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
	std::error_code prepareDir(const fs::path& directory) {
		std::error_code ec;
		if (!directory.empty()) {
			fs::create_directories(directory, ec);
		}
		return ec;
	}

	bool writeFile(const DataSource& dataSource, const fs::path& baseFolder, const fs::path& filePath) {
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
