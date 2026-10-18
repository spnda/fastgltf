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

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

namespace fg = fastgltf;

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

	if (asset.assetInfo.version.major != 2)
		return Error::UnsupportedVersion;
	if (asset.assetInfo.minVersion) {
		auto& minVersion = *asset.assetInfo.minVersion;
		if (minVersion > Version { 2, 1 })
			return Error::UnsupportedVersion;
		if (minVersion > asset.assetInfo.version)
			return Error::InvalidVersion;
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

	auto isUsingImageExtension =
		isExtensionUsed(extensions::KHR_texture_basisu) ||
		isExtensionUsed(extensions::MSFT_texture_dds) ||
		isExtensionUsed(extensions::EXT_texture_webp) ||
		isExtensionUsed(extensions::EXT_texture_avif);
	for (const auto& texture : asset.textures) {
		if (texture.samplerIndex.has_value() && texture.samplerIndex.value() >= asset.samplers.size())
			return Error::InvalidGltf;
		// imageIndex needs to be defined, unless one of the texture extensions were enabled and define another image index.
		if (isUsingImageExtension) {
			auto hasExtensionImage =
				texture.basisuImageIndex.has_value() ||
				texture.ddsImageIndex.has_value() ||
				texture.webpImageIndex.has_value() ||
				texture.avifImageIndex.has_value();
			if (!texture.imageIndex.has_value() && !hasExtensionImage) {
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
		if (texture.avifImageIndex.has_value() && texture.avifImageIndex.value() >= asset.images.size())
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
