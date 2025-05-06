#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include <simdjson.h>

#include <fastgltf/core.hpp>
#include "gltf_path.hpp"

TEST_CASE("Test simple glTF composition", "[write-tests]") {
	fastgltf::BufferView bufferView = {};
	bufferView.bufferIndex = 0;
	bufferView.byteStride = 4;
	bufferView.byteLength = 16;

	fastgltf::Asset asset;
	asset.bufferViews.emplace_back(std::move(bufferView));

	fastgltf::Exporter exporter;
	auto result = exporter.writeGltfJson(asset);
	REQUIRE(result.error() == fastgltf::Error::None);
	REQUIRE(!result.get().output.empty());
}

TEST_CASE("Read glTF, write it, and then read it again and validate", "[write-tests]") {
	auto cubePath = sampleAssets / "Models" / "Cube" / "glTF";
	fastgltf::GltfFileStream cubeJsonData(cubePath / "Cube.gltf");
	REQUIRE(cubeJsonData.isOpen());

	fastgltf::Parser parser;
	auto cube = parser.loadGltfJson(cubeJsonData, cubePath);
	REQUIRE(cube.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(cube.get()) == fastgltf::Error::None);

	fastgltf::Exporter exporter;
	auto expected = exporter.writeGltfJson(cube.get());
    REQUIRE(expected.error() == fastgltf::Error::None);

	auto exportedJsonData = fastgltf::GltfDataBuffer::FromBytes(
			reinterpret_cast<const std::byte*>(expected.get().output.data()), expected.get().output.size());
	REQUIRE(exportedJsonData.error() == fastgltf::Error::None);
	auto cube2 = parser.loadGltfJson(exportedJsonData.get(), cubePath);
	REQUIRE(cube2.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(cube2.get()) == fastgltf::Error::None);
}

TEST_CASE("Rewrite read glTF with multiple material extensions", "[write-tests]") {
	auto dishPath = sampleAssets / "Models" / "IridescentDishWithOlives" / "glTF";
	fastgltf::GltfFileStream dishJsonData(dishPath / "IridescentDishWithOlives.gltf");
	REQUIRE(dishJsonData.isOpen());

	static constexpr auto requiredExtensions = fastgltf::Extensions::KHR_materials_ior |
		fastgltf::Extensions::KHR_materials_iridescence |
		fastgltf::Extensions::KHR_materials_transmission |
		fastgltf::Extensions::KHR_materials_volume;

	fastgltf::Parser parser(requiredExtensions);
	auto dish = parser.loadGltfJson(dishJsonData, dishPath);
	REQUIRE(dish.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(dish.get()) == fastgltf::Error::None);

	fastgltf::Exporter exporter;
	auto expected = exporter.writeGltfJson(dish.get());
	REQUIRE(expected.error() == fastgltf::Error::None);

	auto exportedDishJsonData = fastgltf::GltfDataBuffer::FromBytes(
			reinterpret_cast<const std::byte*>(expected.get().output.data()), expected.get().output.size());
	REQUIRE(exportedDishJsonData.error() == fastgltf::Error::None);
	auto exportedDish = parser.loadGltfJson(exportedDishJsonData.get(), dishPath);
	REQUIRE(exportedDish.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(exportedDish.get()) == fastgltf::Error::None);
}

TEST_CASE("Try writing a glTF with all buffers and images", "[write-tests]") {
    auto cubePath = sampleAssets / "Models" / "Cube" / "glTF";

	fastgltf::GltfFileStream cubeJson(cubePath / "Cube.gltf");
	REQUIRE(cubeJson.isOpen());

    fastgltf::Parser parser;
    auto options = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;
    auto cube = parser.loadGltfJson(cubeJson, cubePath, options);
    REQUIRE(cube.error() == fastgltf::Error::None);

	// Destroy the directory to make sure that the FileExporter correctly creates directories.
	auto exportedFolder = path / "export";
	if (std::filesystem::is_directory(exportedFolder)) {
		std::error_code ec;
		std::filesystem::remove_all(exportedFolder, ec);
		REQUIRE(!ec);
	}

    fastgltf::FileExporter exporter;
    auto error = exporter.writeGltfJson(cube.get(), exportedFolder / "cube.gltf",
                                        fastgltf::ExportOptions::PrettyPrintJson);
    REQUIRE(error == fastgltf::Error::None);
	REQUIRE(std::filesystem::exists(exportedFolder / "buffer0.bin"));
	REQUIRE(std::filesystem::exists(exportedFolder / "image0.bin"));
	REQUIRE(std::filesystem::exists(exportedFolder / "image1.bin"));
}

static bool run_official_gltf_validator(const std::filesystem::path &model_path) {
	static constexpr const char * path_to_validator = "/Users/nikita/Downloads/gltf_validator_macos/gltf_validator";

	// run the validator on the model
	std::string command = std::string(path_to_validator) + " -m " + model_path.string();
	auto result = std::system(command.c_str());
	if (result != 0) {
		return false;
	}
	return true;
}

// template<typename T>
bool isABASame(const fastgltf::AccessorBoundsArray& a, const fastgltf::AccessorBoundsArray& b) {
	// static_assert(std::is_same_v<T, fastgltf::AccessorBoundsArray> || std::is_same_v<T, fastgltf::AccessorBoundsArray>);
	REQUIRE(a.type() == b.type());
	for (std::size_t i = 0; i < a.size(); ++i) {
		if (a.type() == fastgltf::AccessorBoundsArray::BoundsType::float64) {
			REQUIRE (a.get<double>(i) == b.get<double>(i));
		} else {
			REQUIRE (a.get<int64_t>(i) == b.get<int64_t>(i));
		}
	}
	return true;
}

template<typename T>
bool isArraySame(const T& a, const T& b, std::size_t minSize = -1) {
	if (minSize == -1) {
		REQUIRE(a.size() == b.size());
		minSize = a.size();
	} else {
		REQUIRE(minSize <= a.size());
		REQUIRE(minSize <= b.size());
	}
	for (std::size_t i = 0; i < minSize; ++i) {
		REQUIRE (a[i] == b[i]);
	}
	return true;
}

bool isDataSourceSame(const fastgltf::DataSource& a, const fastgltf::DataSource& b, size_t minSize = -1) {
	//    FASTGLTF_EXPORT using DataSource = std::variant<std::monostate, sources::BufferView, sources::URI, sources::Array, sources::Vector, sources::CustomBuffer, sources::ByteView, sources::Fallback>;

	if (std::holds_alternative<std::monostate>(a)) {
		if (!std::holds_alternative<std::monostate>(b)) {
			return false;
		}
		return true;
	} else if (std::holds_alternative<fastgltf::sources::BufferView>(a)) {
		if (!std::holds_alternative<fastgltf::sources::BufferView>(b)) {
			return false;
		}
		auto a_bv = std::get<fastgltf::sources::BufferView>(a);
		auto b_bv = std::get<fastgltf::sources::BufferView>(b);
		return a_bv.bufferViewIndex == b_bv.bufferViewIndex && a_bv.mimeType == b_bv.mimeType;
	} else if (std::holds_alternative<fastgltf::sources::URI>(a)) {
		if (!std::holds_alternative<fastgltf::sources::URI>(b)) {
			return false;
		}
		auto a_uri = std::get<fastgltf::sources::URI>(a);
		auto b_uri = std::get<fastgltf::sources::URI>(b);
		return a_uri.uri.string() == b_uri.uri.string();
	} else if (std::holds_alternative<fastgltf::sources::Array>(a)) {
		if (!std::holds_alternative<fastgltf::sources::Array>(b)) {
			return false;
		}
		auto a_array = std::get<fastgltf::sources::Array>(a);
		auto b_array = std::get<fastgltf::sources::Array>(b);
		return a_array.mimeType == b_array.mimeType && isArraySame(a_array.bytes, b_array.bytes, minSize);
	} else if (std::holds_alternative<fastgltf::sources::Vector>(a)) {
		if (!std::holds_alternative<fastgltf::sources::Vector>(b)) {
			return false;
		}
		auto a_vector = std::get<fastgltf::sources::Vector>(a);
		auto b_vector = std::get<fastgltf::sources::Vector>(b);
		return a_vector.mimeType == b_vector.mimeType && isArraySame(a_vector.bytes, b_vector.bytes, minSize);
	} else if (std::holds_alternative<fastgltf::sources::CustomBuffer>(a)) {
		if (!std::holds_alternative<fastgltf::sources::CustomBuffer>(b)) {
			return false;
		}
		auto a_custom_buffer = std::get<fastgltf::sources::CustomBuffer>(a);
		auto b_custom_buffer = std::get<fastgltf::sources::CustomBuffer>(b);
		return a_custom_buffer.mimeType == b_custom_buffer.mimeType && b_custom_buffer.id == a_custom_buffer.id;
	} else if (std::holds_alternative<fastgltf::sources::ByteView>(a)) {
		if (!std::holds_alternative<fastgltf::sources::ByteView>(b)) {
			return false;
		}
		auto a_byte_view = std::get<fastgltf::sources::ByteView>(a);
		auto b_byte_view = std::get<fastgltf::sources::ByteView>(b);
		return a_byte_view.mimeType == b_byte_view.mimeType && isArraySame(a_byte_view.bytes, b_byte_view.bytes, minSize);
	} else if (std::holds_alternative<fastgltf::sources::Fallback>(a)) {
		if (!std::holds_alternative<fastgltf::sources::Fallback>(b)) {
			return false;
		}
		return true;
	}
	return false;
}


static bool compare_TextureInfo(const fastgltf::TextureInfo& a, const fastgltf::TextureInfo& b) {
	REQUIRE (a.textureIndex == b.textureIndex);
	REQUIRE (a.texCoordIndex == b.texCoordIndex);
	if (a.transform) {
		REQUIRE (a.transform->uvOffset == b.transform->uvOffset);
		REQUIRE (a.transform->uvScale == b.transform->uvScale);
		REQUIRE (a.transform->rotation == b.transform->rotation);
		REQUIRE (a.transform->texCoordIndex == b.transform->texCoordIndex);
	}
	return true;
}

static bool compare_OptionalTextureInfo(const fastgltf::Optional<fastgltf::TextureInfo>& a, const fastgltf::Optional<fastgltf::TextureInfo>& b) {
	if (a.has_value() != b.has_value()) {
		return false;
	}
	if (!a.has_value()) {
		return true;
	}
	REQUIRE (compare_TextureInfo(a.value(), b.value()));
	return true;
}

static bool compare_vector_attributes(const FASTGLTF_STD_PMR_NS::vector<fastgltf::Attribute>& a, const FASTGLTF_STD_PMR_NS::vector<fastgltf::Attribute>& b) {
	REQUIRE (a.size() == b.size());
	for (std::size_t i = 0; i < a.size(); ++i) {
		REQUIRE (a[i].accessorIndex == b[i].accessorIndex);
		REQUIRE (a[i].name == b[i].name);
	}
	return true;
}

static bool compare_vector_attributes(const fastgltf::SmallVector<fastgltf::Attribute, 4, std::pmr::polymorphic_allocator<fastgltf::Attribute>>& a, const fastgltf::SmallVector<fastgltf::Attribute, 4, std::pmr::polymorphic_allocator<fastgltf::Attribute>>& b) {
	REQUIRE (a.size() == b.size());
	for (std::size_t i = 0; i < a.size(); ++i) {
		REQUIRE (a[i].accessorIndex == b[i].accessorIndex);
		REQUIRE (a[i].name == b[i].name);
	}
	return true;
}

// change the if... return false to REQUIRE
static bool isSame(const fastgltf::Asset& asset1, const fastgltf::Asset& asset2) {
	// assetInfo
	REQUIRE (asset1.assetInfo->gltfVersion == asset2.assetInfo->gltfVersion);
	REQUIRE (asset1.assetInfo->generator == asset2.assetInfo->generator);
	REQUIRE (asset1.assetInfo->copyright == asset2.assetInfo->copyright);
	// extensionsUsed
	REQUIRE (asset1.extensionsUsed.size() == asset2.extensionsUsed.size());
	for (std::size_t j = 0; j < asset1.extensionsUsed.size(); ++j) {
		REQUIRE (asset1.extensionsUsed[j] == asset2.extensionsUsed[j]);
	}
	// extensionsRequired
	REQUIRE (asset1.extensionsRequired.size() == asset2.extensionsRequired.size())	;
	for (std::size_t j = 0; j < asset1.extensionsRequired.size(); ++j) {
		REQUIRE (asset1.extensionsRequired[j] == asset2.extensionsRequired[j]);
	}
	// defaultScene
	REQUIRE (asset1.defaultScene == asset2.defaultScene);
	// accessors
	REQUIRE (asset1.accessors.size() == asset2.accessors.size());
	for (std::size_t j = 0; j < asset1.accessors.size(); ++j) {
		auto& accessor1 = asset1.accessors[j];
		auto& accessor2 = asset2.accessors[j];
		REQUIRE (accessor1.byteOffset == accessor2.byteOffset);
		REQUIRE (accessor1.count == accessor2.count);
		REQUIRE (accessor1.type == accessor2.type);
		REQUIRE (accessor1.componentType == accessor2.componentType);
		REQUIRE (accessor1.normalized == accessor2.normalized);
		REQUIRE (accessor1.bufferViewIndex == accessor2.bufferViewIndex);
		REQUIRE (accessor1.min.has_value() == accessor2.min.has_value());
		REQUIRE (accessor1.max.has_value() == accessor2.max.has_value());
		if (accessor1.min.has_value()) {
			REQUIRE (isABASame(accessor1.min.value(), accessor2.min.value()));
		}
		if (accessor1.max.has_value()) {
			REQUIRE (isABASame(accessor1.max.value(), accessor2.max.value()));
		}
		REQUIRE (accessor1.sparse.has_value() == accessor2.sparse.has_value());
		if (accessor1.sparse.has_value()) {
			REQUIRE (accessor1.sparse->count == accessor2.sparse->count);
			REQUIRE (accessor1.sparse->indicesBufferView == accessor2.sparse->indicesBufferView);
			REQUIRE (accessor1.sparse->indicesByteOffset == accessor2.sparse->indicesByteOffset);
			REQUIRE (accessor1.sparse->valuesBufferView == accessor2.sparse->valuesBufferView);
			REQUIRE (accessor1.sparse->valuesByteOffset == accessor2.sparse->valuesByteOffset);
			REQUIRE (accessor1.sparse->indexComponentType == accessor2.sparse->indexComponentType);
		}
		REQUIRE(accessor1.name == accessor2.name);
	}
	// animations
	REQUIRE (asset1.animations.size() == asset2.animations.size());
	for (std::size_t j = 0; j < asset1.animations.size(); ++j) {
		auto& animation1 = asset1.animations[j];
		auto& animation2 = asset2.animations[j];
		REQUIRE (animation1.name == animation2.name);
		REQUIRE (animation1.channels.size() == animation2.channels.size());
		for (std::size_t k = 0; k < animation1.channels.size(); ++k) {
			auto& channel1 = animation1.channels[k];
			auto& channel2 = animation2.channels[k];
			REQUIRE (channel1.samplerIndex == channel2.samplerIndex);
			REQUIRE (channel1.nodeIndex == channel2.nodeIndex);
			REQUIRE (channel1.path == channel2.path);
		}
		REQUIRE (animation1.samplers.size() == animation2.samplers.size());
		for (std::size_t k = 0; k < animation1.samplers.size(); ++k) {
			auto& sampler1 = animation1.samplers[k];
			auto& sampler2 = animation2.samplers[k];
			REQUIRE (sampler1.inputAccessor == sampler2.inputAccessor);
			REQUIRE (sampler1.interpolation == sampler2.interpolation);
			REQUIRE (sampler1.outputAccessor == sampler2.outputAccessor);
		}
	}
	// buffers
	REQUIRE (asset1.buffers.size() == asset2.buffers.size());
	for (std::size_t j = 0; j < asset1.buffers.size(); ++j) {
		// buffer may be different size because it wasn't aligned properly before
		REQUIRE (fastgltf::alignUp(asset1.buffers[j].byteLength, 4) == fastgltf::alignUp(asset2.buffers[j].byteLength, 4));
		REQUIRE (asset1.buffers[j].name == asset2.buffers[j].name);
		auto minSize = std::min(asset1.buffers[j].byteLength, asset2.buffers[j].byteLength);
		REQUIRE (isDataSourceSame(asset1.buffers[j].data, asset2.buffers[j].data, minSize));
	}

	// bufferViews
	REQUIRE (asset1.bufferViews.size() == asset2.bufferViews.size());
	for (std::size_t j = 0; j < asset1.bufferViews.size(); ++j) {
		REQUIRE (asset1.bufferViews[j].bufferIndex == asset2.bufferViews[j].bufferIndex);
		REQUIRE (asset1.bufferViews[j].byteOffset == asset2.bufferViews[j].byteOffset);
		REQUIRE (asset1.bufferViews[j].byteLength == asset2.bufferViews[j].byteLength);
		REQUIRE (asset1.bufferViews[j].byteStride == asset2.bufferViews[j].byteStride);
		REQUIRE (asset1.bufferViews[j].target == asset2.bufferViews[j].target);
		REQUIRE (!asset1.bufferViews[j].meshoptCompression == !asset2.bufferViews[j].meshoptCompression);
		if (asset1.bufferViews[j].meshoptCompression) {
			REQUIRE (asset1.bufferViews[j].meshoptCompression->bufferIndex == asset2.bufferViews[j].meshoptCompression->bufferIndex);
			REQUIRE (asset1.bufferViews[j].meshoptCompression->byteOffset == asset2.bufferViews[j].meshoptCompression->byteOffset);
			REQUIRE (asset1.bufferViews[j].meshoptCompression->byteLength == asset2.bufferViews[j].meshoptCompression->byteLength);
			REQUIRE (asset1.bufferViews[j].meshoptCompression->count == asset2.bufferViews[j].meshoptCompression->count);
			REQUIRE (asset1.bufferViews[j].meshoptCompression->mode == asset2.bufferViews[j].meshoptCompression->mode);
			REQUIRE (asset1.bufferViews[j].meshoptCompression->filter == asset2.bufferViews[j].meshoptCompression->filter);
			REQUIRE (asset1.bufferViews[j].meshoptCompression->byteStride == asset2.bufferViews[j].meshoptCompression->byteStride);
		}
	}

	// cameras
	REQUIRE (asset1.cameras.size() == asset2.cameras.size());
	for (std::size_t j = 0; j < asset1.cameras.size(); ++j) {
		REQUIRE (asset1.cameras[j].name == asset2.cameras[j].name);
		if (std::holds_alternative<fastgltf::Camera::Perspective>(asset1.cameras[j].camera)) {
			auto camera1 = std::get_if<fastgltf::Camera::Perspective>(&asset1.cameras[j].camera);
			auto camera2 = std::get_if<fastgltf::Camera::Perspective>(&asset2.cameras[j].camera);
			REQUIRE (camera1->aspectRatio == camera2->aspectRatio);
			REQUIRE (camera1->yfov == camera2->yfov);
			REQUIRE (camera1->znear == camera2->znear);
			REQUIRE (camera1->zfar == camera2->zfar);
		} else if (std::holds_alternative<fastgltf::Camera::Orthographic>(asset1.cameras[j].camera)) {
			auto camera1 = std::get_if<fastgltf::Camera::Orthographic>(&asset1.cameras[j].camera);
			auto camera2 = std::get_if<fastgltf::Camera::Orthographic>(&asset2.cameras[j].camera);
			REQUIRE (camera1->xmag == camera2->xmag);
			REQUIRE (camera1->ymag == camera2->ymag);
			REQUIRE (camera1->znear == camera2->znear);
			REQUIRE (camera1->zfar == camera2->zfar);
		}
	}

	// images
	REQUIRE (asset1.images.size() == asset2.images.size());
	for (std::size_t j = 0; j < asset1.images.size(); ++j) {
		REQUIRE (asset1.images[j].name == asset2.images[j].name);
		REQUIRE (isDataSourceSame(asset1.images[j].data, asset2.images[j].data));
	}

	// lights
	REQUIRE (asset1.lights.size() == asset2.lights.size());
	for (std::size_t j = 0; j < asset1.lights.size(); ++j) {
		REQUIRE (asset1.lights[j].type == asset2.lights[j].type);
		REQUIRE (asset1.lights[j].color.x() == asset2.lights[j].color.x());
		REQUIRE (asset1.lights[j].color.y() == asset2.lights[j].color.y());
		REQUIRE (asset1.lights[j].color.z() == asset2.lights[j].color.z());
		REQUIRE (asset1.lights[j].intensity == asset2.lights[j].intensity);
		REQUIRE (asset1.lights[j].range == asset2.lights[j].range);
		REQUIRE (asset1.lights[j].innerConeAngle == asset2.lights[j].innerConeAngle);
		REQUIRE (asset1.lights[j].outerConeAngle == asset2.lights[j].outerConeAngle);
		REQUIRE (asset1.lights[j].name == asset2.lights[j].name);
	}
	// materials
	REQUIRE (asset1.materials.size() == asset2.materials.size());
	for (std::size_t j = 0; j < asset1.materials.size(); ++j) {
		REQUIRE (asset1.materials[j].pbrData.metallicRoughnessTexture.has_value() == asset2.materials[j].pbrData.metallicRoughnessTexture.has_value());
		if (asset1.materials[j].pbrData.metallicRoughnessTexture.has_value()) {
			REQUIRE (asset1.materials[j].pbrData.metallicRoughnessTexture.value().textureIndex == asset2.materials[j].pbrData.metallicRoughnessTexture.value().textureIndex);
			REQUIRE (asset1.materials[j].pbrData.metallicRoughnessTexture.value().texCoordIndex == asset2.materials[j].pbrData.metallicRoughnessTexture.value().texCoordIndex);
		}
		REQUIRE (asset1.materials[j].pbrData.metallicFactor == asset2.materials[j].pbrData.metallicFactor);
		REQUIRE (asset1.materials[j].pbrData.roughnessFactor == asset2.materials[j].pbrData.roughnessFactor);
		REQUIRE (asset1.materials[j].pbrData.baseColorFactor == asset2.materials[j].pbrData.baseColorFactor);
		compare_OptionalTextureInfo(asset1.materials[j].pbrData.baseColorTexture, asset2.materials[j].pbrData.baseColorTexture);
		compare_OptionalTextureInfo(asset1.materials[j].pbrData.metallicRoughnessTexture, asset2.materials[j].pbrData.metallicRoughnessTexture);

		REQUIRE (asset1.materials[j].normalTexture.has_value() == asset2.materials[j].normalTexture.has_value());
		if (asset1.materials[j].normalTexture.has_value()) {
			REQUIRE (asset1.materials[j].normalTexture.value().scale == asset2.materials[j].normalTexture.value().scale);
			compare_TextureInfo(asset1.materials[j].normalTexture.value(), asset2.materials[j].normalTexture.value());
		}
		REQUIRE (asset1.materials[j].occlusionTexture.has_value() == asset2.materials[j].occlusionTexture.has_value());
		if (asset1.materials[j].occlusionTexture.has_value()) {
			REQUIRE (asset1.materials[j].occlusionTexture.value().strength == asset2.materials[j].occlusionTexture.value().strength);
			compare_TextureInfo(asset1.materials[j].occlusionTexture.value(), asset2.materials[j].occlusionTexture.value());
		}
		compare_OptionalTextureInfo(asset1.materials[j].emissiveTexture, asset2.materials[j].emissiveTexture);
		//emissiveFactor
		REQUIRE (asset1.materials[j].emissiveFactor == asset2.materials[j].emissiveFactor);
		//alphaMode
		REQUIRE (asset1.materials[j].alphaMode == asset2.materials[j].alphaMode);
		//doubleSided
		REQUIRE (asset1.materials[j].doubleSided == asset2.materials[j].doubleSided);
		//unlit
		REQUIRE (asset1.materials[j].unlit == asset2.materials[j].unlit);
		//alphaCutoff
		REQUIRE (asset1.materials[j].alphaCutoff == asset2.materials[j].alphaCutoff);
		//emissiveStrength
		REQUIRE (asset1.materials[j].emissiveStrength == asset2.materials[j].emissiveStrength);
		//ior
		REQUIRE (asset1.materials[j].ior == asset2.materials[j].ior);
		//dispersion
		REQUIRE (asset1.materials[j].dispersion == asset2.materials[j].dispersion);
		//anisotropy
		REQUIRE ((!asset1.materials[j].anisotropy) == (!asset2.materials[j].anisotropy));
		if (asset1.materials[j].anisotropy) {
			REQUIRE (asset1.materials[j].anisotropy->anisotropyStrength == asset2.materials[j].anisotropy->anisotropyStrength);
			REQUIRE (asset1.materials[j].anisotropy->anisotropyRotation == asset2.materials[j].anisotropy->anisotropyRotation);
			compare_OptionalTextureInfo(asset1.materials[j].anisotropy->anisotropyTexture, asset2.materials[j].anisotropy->anisotropyTexture);
		}
		//clearcoat
		REQUIRE ((!asset1.materials[j].clearcoat) == (!asset2.materials[j].clearcoat));
		if (asset1.materials[j].clearcoat) {
			REQUIRE (asset1.materials[j].clearcoat->clearcoatFactor == asset2.materials[j].clearcoat->clearcoatFactor);
			REQUIRE (asset1.materials[j].clearcoat->clearcoatRoughnessFactor == asset2.materials[j].clearcoat->clearcoatRoughnessFactor);
			compare_OptionalTextureInfo(asset1.materials[j].clearcoat->clearcoatTexture, asset2.materials[j].clearcoat->clearcoatTexture);
			compare_OptionalTextureInfo(asset1.materials[j].clearcoat->clearcoatRoughnessTexture, asset2.materials[j].clearcoat->clearcoatRoughnessTexture);
			REQUIRE (asset1.materials[j].clearcoat->clearcoatNormalTexture.has_value() == asset2.materials[j].clearcoat->clearcoatNormalTexture.has_value());
			if (asset1.materials[j].clearcoat->clearcoatNormalTexture.has_value()) {
				REQUIRE (asset1.materials[j].clearcoat->clearcoatNormalTexture.value().scale == asset2.materials[j].clearcoat->clearcoatNormalTexture.value().scale);
				compare_TextureInfo(asset1.materials[j].clearcoat->clearcoatNormalTexture.value(), asset2.materials[j].clearcoat->clearcoatNormalTexture.value());
			}
		}
		//iridescence
		REQUIRE ((!asset1.materials[j].iridescence) == (!asset2.materials[j].iridescence));
		if (asset1.materials[j].iridescence) {
			REQUIRE (asset1.materials[j].iridescence->iridescenceFactor == asset2.materials[j].iridescence->iridescenceFactor);
			REQUIRE (asset1.materials[j].iridescence->iridescenceIor == asset2.materials[j].iridescence->iridescenceIor);
			compare_OptionalTextureInfo(asset1.materials[j].iridescence->iridescenceTexture, asset2.materials[j].iridescence->iridescenceTexture);
			REQUIRE (asset1.materials[j].iridescence->iridescenceThicknessMinimum == asset2.materials[j].iridescence->iridescenceThicknessMinimum);
			REQUIRE (asset1.materials[j].iridescence->iridescenceThicknessMaximum == asset2.materials[j].iridescence->iridescenceThicknessMaximum);
			compare_OptionalTextureInfo(asset1.materials[j].iridescence->iridescenceThicknessTexture, asset2.materials[j].iridescence->iridescenceThicknessTexture);
		}
		//sheen
		REQUIRE ((!asset1.materials[j].sheen) == (!asset2.materials[j].sheen));
		if (asset1.materials[j].sheen) {
			REQUIRE (asset1.materials[j].sheen->sheenColorFactor == asset2.materials[j].sheen->sheenColorFactor);
			compare_OptionalTextureInfo(asset1.materials[j].sheen->sheenColorTexture, asset2.materials[j].sheen->sheenColorTexture);
			REQUIRE (asset1.materials[j].sheen->sheenRoughnessFactor == asset2.materials[j].sheen->sheenRoughnessFactor);
			compare_OptionalTextureInfo(asset1.materials[j].sheen->sheenRoughnessTexture, asset2.materials[j].sheen->sheenRoughnessTexture);
		}
		//specular
		REQUIRE ((!asset1.materials[j].specular) == (!asset2.materials[j].specular));
		if (asset1.materials[j].specular) {
			REQUIRE (asset1.materials[j].specular->specularFactor == asset2.materials[j].specular->specularFactor);
			compare_OptionalTextureInfo(asset1.materials[j].specular->specularTexture, asset2.materials[j].specular->specularTexture);
			REQUIRE(asset1.materials[j].specular->specularColorFactor == asset2.materials[j].specular->specularColorFactor);
			compare_OptionalTextureInfo(asset1.materials[j].specular->specularColorTexture, asset2.materials[j].specular->specularColorTexture);
		}
		//specularGlossiness
		REQUIRE ((!asset1.materials[j].specularGlossiness) == (!asset2.materials[j].specularGlossiness));
		if (asset1.materials[j].specularGlossiness) {
			REQUIRE (asset1.materials[j].specularGlossiness->diffuseFactor == asset2.materials[j].specularGlossiness->diffuseFactor);
			REQUIRE (asset1.materials[j].specularGlossiness->specularFactor == asset2.materials[j].specularGlossiness->specularFactor);
			compare_OptionalTextureInfo(asset1.materials[j].specularGlossiness->diffuseTexture, asset2.materials[j].specularGlossiness->diffuseTexture);
			REQUIRE (asset1.materials[j].specularGlossiness->glossinessFactor == asset2.materials[j].specularGlossiness->glossinessFactor);
			compare_OptionalTextureInfo(asset1.materials[j].specularGlossiness->specularGlossinessTexture, asset2.materials[j].specularGlossiness->specularGlossinessTexture);
		}
		//transmission
		REQUIRE ((!asset1.materials[j].transmission) == (!asset2.materials[j].transmission));
		if (asset1.materials[j].transmission) {
			REQUIRE (asset1.materials[j].transmission->transmissionFactor == asset2.materials[j].transmission->transmissionFactor);
			compare_OptionalTextureInfo(asset1.materials[j].transmission->transmissionTexture, asset2.materials[j].transmission->transmissionTexture);
		}
		//volume
		REQUIRE ((!asset1.materials[j].volume) == (!asset2.materials[j].volume));
		if (asset1.materials[j].volume) {
			REQUIRE(asset1.materials[j].volume->thicknessFactor == asset2.materials[j].volume->thicknessFactor);
			compare_OptionalTextureInfo(asset1.materials[j].volume->thicknessTexture, asset2.materials[j].volume->thicknessTexture);
			REQUIRE (asset1.materials[j].volume->attenuationDistance == asset2.materials[j].volume->attenuationDistance);
			REQUIRE (asset1.materials[j].volume->attenuationColor == asset2.materials[j].volume->attenuationColor);
		}
		//packedOcclusionRoughnessMetallicTextures
		REQUIRE ((!asset1.materials[j].packedOcclusionRoughnessMetallicTextures) == (!asset2.materials[j].packedOcclusionRoughnessMetallicTextures));
		if (asset1.materials[j].packedOcclusionRoughnessMetallicTextures) {
			REQUIRE(compare_OptionalTextureInfo(asset1.materials[j].packedOcclusionRoughnessMetallicTextures->occlusionRoughnessMetallicTexture, asset2.materials[j].packedOcclusionRoughnessMetallicTextures->occlusionRoughnessMetallicTexture));
			REQUIRE(compare_OptionalTextureInfo(asset1.materials[j].packedOcclusionRoughnessMetallicTextures->roughnessMetallicOcclusionTexture, asset2.materials[j].packedOcclusionRoughnessMetallicTextures->roughnessMetallicOcclusionTexture));
			REQUIRE(compare_OptionalTextureInfo(asset1.materials[j].packedOcclusionRoughnessMetallicTextures->normalTexture, asset2.materials[j].packedOcclusionRoughnessMetallicTextures->normalTexture));
		}
		//name
		REQUIRE (asset1.materials[j].name == asset2.materials[j].name);
	}

	//meshes
	REQUIRE (asset1.meshes.size() == asset2.meshes.size());
	for (std::size_t j = 0; j < asset1.meshes.size(); ++j) {
		// primitives
		REQUIRE (asset1.meshes[j].primitives.size() == asset2.meshes[j].primitives.size());
		for (std::size_t k = 0; k < asset1.meshes[j].primitives.size(); ++k) {
			REQUIRE (asset1.meshes[j].primitives[k].materialIndex == asset2.meshes[j].primitives[k].materialIndex);
			REQUIRE(compare_vector_attributes(asset1.meshes[j].primitives[k].attributes, asset2.meshes[j].primitives[k].attributes));
			REQUIRE(asset1.meshes[j].primitives[k].targets.size() == asset2.meshes[j].primitives[k].targets.size());
			for (std::size_t l = 0; l < asset1.meshes[j].primitives[k].targets.size(); ++l) {
				REQUIRE(compare_vector_attributes(asset1.meshes[j].primitives[k].targets[l], asset2.meshes[j].primitives[k].targets[l]));
			}
			REQUIRE (asset1.meshes[j].primitives[k].indicesAccessor == asset2.meshes[j].primitives[k].indicesAccessor);
			REQUIRE (asset1.meshes[j].primitives[k].materialIndex == asset2.meshes[j].primitives[k].materialIndex);
			REQUIRE (isArraySame(asset1.meshes[j].primitives[k].mappings, asset2.meshes[j].primitives[k].mappings));
			REQUIRE (asset1.meshes[j].primitives[k].type == asset2.meshes[j].primitives[k].type);
			REQUIRE ((!asset1.meshes[j].primitives[k].dracoCompression) == (!asset2.meshes[j].primitives[k].dracoCompression));
			if (asset1.meshes[j].primitives[k].dracoCompression) {
				REQUIRE (asset1.meshes[j].primitives[k].dracoCompression->bufferView == asset2.meshes[j].primitives[k].dracoCompression->bufferView);
				REQUIRE(compare_vector_attributes(asset1.meshes[j].primitives[k].dracoCompression->attributes, asset2.meshes[j].primitives[k].dracoCompression->attributes));
			}
		}
		REQUIRE (asset1.meshes[j].name == asset2.meshes[j].name);
		REQUIRE (isArraySame(asset1.meshes[j].weights, asset2.meshes[j].weights));
	}

	// nodes
	REQUIRE (asset1.nodes.size() == asset2.nodes.size());
	for (std::size_t j = 0; j < asset1.nodes.size(); ++j) {
		REQUIRE (asset1.nodes[j].meshIndex == asset2.nodes[j].meshIndex);
		REQUIRE (asset1.nodes[j].skinIndex == asset2.nodes[j].skinIndex);
		REQUIRE (asset1.nodes[j].cameraIndex == asset2.nodes[j].cameraIndex);
		REQUIRE (asset1.nodes[j].lightIndex == asset2.nodes[j].lightIndex);
		REQUIRE (isArraySame(asset1.nodes[j].children, asset2.nodes[j].children));
		REQUIRE (isArraySame(asset1.nodes[j].weights, asset2.nodes[j].weights));
		// REQUIRE (asset1.nodes[j].transform == asset2.nodes[j].transform);
		//        std::variant<TRS, math::fmat4x4> transform;

		REQUIRE(std::holds_alternative<fastgltf::TRS>(asset1.nodes[j].transform) == std::holds_alternative<fastgltf::TRS>(asset2.nodes[j].transform));
		if (std::holds_alternative<fastgltf::TRS>(asset1.nodes[j].transform)) {
			auto &a = std::get<fastgltf::TRS>(asset1.nodes[j].transform);
			auto &b = std::get<fastgltf::TRS>(asset2.nodes[j].transform);
			REQUIRE (a.translation == b.translation);
			REQUIRE (a.rotation == b.rotation);
			REQUIRE (a.scale == b.scale);
		} else {
			auto &a = std::get<fastgltf::math::fmat4x4>(asset1.nodes[j].transform);
			auto &b = std::get<fastgltf::math::fmat4x4>(asset2.nodes[j].transform);
			REQUIRE (a == b);
		}
		REQUIRE(asset1.nodes[j].instancingAttributes.size() == asset2.nodes[j].instancingAttributes.size());
		for (std::size_t k = 0; k < asset1.nodes[j].instancingAttributes.size(); ++k) {
			REQUIRE(asset1.nodes[j].instancingAttributes[k].name == asset2.nodes[j].instancingAttributes[k].name);
			REQUIRE(asset1.nodes[j].instancingAttributes[k].accessorIndex == asset2.nodes[j].instancingAttributes[k].accessorIndex);
		}
		compare_vector_attributes(asset1.nodes[j].instancingAttributes, asset2.nodes[j].instancingAttributes);
		REQUIRE (asset1.nodes[j].name == asset2.nodes[j].name);

	}
	// samplers
	REQUIRE (asset1.samplers.size() == asset2.samplers.size());
	for (std::size_t j = 0; j < asset1.samplers.size(); ++j) {
		REQUIRE (asset1.samplers[j].magFilter == asset2.samplers[j].magFilter);
		REQUIRE (asset1.samplers[j].minFilter == asset2.samplers[j].minFilter);
		REQUIRE (asset1.samplers[j].wrapS == asset2.samplers[j].wrapS);
		REQUIRE (asset1.samplers[j].wrapT == asset2.samplers[j].wrapT);
		REQUIRE (asset1.samplers[j].name == asset2.samplers[j].name);
	}
        // std::vector<Scene> scenes;
        // std::vector<Skin> skins;
        // std::vector<Texture> textures;

		// std::vector<std::string> materialVariants;

	// scenes
	REQUIRE (asset1.scenes.size() == asset2.scenes.size());
	for (std::size_t j = 0; j < asset1.scenes.size(); ++j) {
		REQUIRE (asset1.scenes[j].name == asset2.scenes[j].name);
		REQUIRE (isArraySame(asset1.scenes[j].nodeIndices, asset2.scenes[j].nodeIndices));
	}
	
	// skins
	REQUIRE (asset1.skins.size() == asset2.skins.size());
	for (std::size_t j = 0; j < asset1.skins.size(); ++j) {
		REQUIRE (asset1.skins[j].name == asset2.skins[j].name);
		REQUIRE (isArraySame(asset1.skins[j].joints, asset2.skins[j].joints));
		REQUIRE (asset1.skins[j].skeleton == asset2.skins[j].skeleton);
		REQUIRE (asset1.skins[j].inverseBindMatrices == asset2.skins[j].inverseBindMatrices);
	}
	
	// textures
	REQUIRE (asset1.textures.size() == asset2.textures.size());
	for (std::size_t j = 0; j < asset1.textures.size(); ++j) {
		REQUIRE (asset1.textures[j].samplerIndex == asset2.textures[j].samplerIndex);
		REQUIRE (asset1.textures[j].imageIndex == asset2.textures[j].imageIndex);
		REQUIRE (asset1.textures[j].basisuImageIndex == asset2.textures[j].basisuImageIndex);
		REQUIRE (asset1.textures[j].ddsImageIndex == asset2.textures[j].ddsImageIndex);
		REQUIRE (asset1.textures[j].webpImageIndex == asset2.textures[j].webpImageIndex);
		REQUIRE (asset1.textures[j].name == asset2.textures[j].name);
	}
	
	// material variants
	REQUIRE (isArraySame(asset1.materialVariants, asset2.materialVariants));

	return true;
}


static void inline test_glb_rewrite(const std::filesystem::path &model_parent_dir, const std::filesystem::path &model_file) {
	if (!run_official_gltf_validator(sampleAssets / model_parent_dir / model_file)) {
		auto report = std::string( "Failed to validate the model: " + (model_parent_dir / model_file).string());
		SKIP(report.c_str());
		return;
	}
	
	auto sampleDir = sampleAssets / model_parent_dir;
	auto samplePath = sampleDir / model_file;
	fastgltf::GltfFileStream sampleFileStream(samplePath);
	REQUIRE(sampleFileStream.isOpen());
	static constexpr auto requiredExtensions = fastgltf::Extensions::KHR_texture_transform |
			fastgltf::Extensions::KHR_texture_basisu |
			fastgltf::Extensions::MSFT_texture_dds |
			fastgltf::Extensions::KHR_mesh_quantization |
			fastgltf::Extensions::EXT_meshopt_compression |
			fastgltf::Extensions::KHR_lights_punctual |
			fastgltf::Extensions::EXT_texture_webp |
			fastgltf::Extensions::KHR_materials_specular |
			fastgltf::Extensions::KHR_materials_ior |
			fastgltf::Extensions::KHR_materials_iridescence |
			fastgltf::Extensions::KHR_materials_volume |
			fastgltf::Extensions::KHR_materials_transmission |
			fastgltf::Extensions::KHR_materials_clearcoat |
			fastgltf::Extensions::KHR_materials_emissive_strength |
			fastgltf::Extensions::KHR_materials_sheen |
			fastgltf::Extensions::KHR_materials_unlit |
			fastgltf::Extensions::KHR_materials_anisotropy |
			fastgltf::Extensions::EXT_mesh_gpu_instancing |
			#if FASTGLTF_ENABLE_DEPRECATED_EXT
			fastgltf::Extensions::KHR_materials_pbrSpecularGlossiness |
			#endif
			fastgltf::Extensions::MSFT_packing_normalRoughnessMetallic |
			fastgltf::Extensions::MSFT_packing_occlusionRoughnessMetallic |
			fastgltf::Extensions::KHR_materials_dispersion |		
			fastgltf::Extensions::KHR_materials_variants |
			fastgltf::Extensions::KHR_accessor_float64 |
			fastgltf::Extensions::KHR_draco_mesh_compression;
	fastgltf::Parser parser(requiredExtensions);
	auto options = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages ;
	fastgltf::Expected<fastgltf::Asset> asset = model_file.extension() == ".gltf" ?
		parser.loadGltfJson(sampleFileStream, sampleDir, options) :
		parser.loadGltfBinary(sampleFileStream, sampleDir, options);
	REQUIRE(getErrorName(asset.error()) == "None");
	REQUIRE(getErrorName(fastgltf::validate(asset.get())) == "None");

	// Destroy the directory to make sure that the FileExporter correctly creates directories.
	auto exportedFolder = path / "export_glb";

	fastgltf::FileExporter exporter;
	auto model_base_name = model_file.stem();
	auto exportedGLTFPath = exportedFolder / "rewritten_gltf" / model_parent_dir / model_base_name += ".gltf";
	auto exportedGLBPath = exportedFolder / "rewritten_glb" / model_parent_dir / model_base_name += ".glb";
	// ensure dir
	std::filesystem::create_directories(exportedGLBPath.parent_path());
	auto error = exporter.writeGltfBinary(asset.get(), exportedGLBPath, fastgltf::ExportOptions::ValidateAsset);
	REQUIRE(error == fastgltf::Error::None);
	REQUIRE(std::filesystem::exists(exportedGLBPath));
	// error = exporter.writeGltfJson(asset.get(), exportedGLTFPath, fastgltf::ExportOptions::PrettyPrintJson);


	std::uint32_t oldJsonChunkLength = 0;
	{
		std::ifstream glb_old(samplePath, std::ios::binary);
		REQUIRE(glb_old.is_open());

		// Skip over the header
		glb_old.seekg(sizeof(std::uint32_t) * 3, std::ifstream::beg);
		// Read the chunk length
		glb_old.read(reinterpret_cast<char*>(&oldJsonChunkLength), sizeof oldJsonChunkLength);
	}

	// Make sure the GLB buffer is written
	std::ifstream glb(exportedGLBPath, std::ios::binary);
	REQUIRE(glb.is_open());

	// Skip over the header
	glb.seekg(sizeof(std::uint32_t) * 3, std::ifstream::beg);

	// Read the chunk length
	std::uint32_t jsonChunkLength = 0;
	glb.read(reinterpret_cast<char*>(&jsonChunkLength), sizeof jsonChunkLength);
	// REQUIRE(jsonChunkLength == fastgltf::alignUp(oldJsonChunkLength, 4));
	
	// Skip over the chunk type + chunk
	glb.seekg(sizeof(std::uint32_t) + fastgltf::alignUp(jsonChunkLength, 4), std::ifstream::cur);

	// Read binary chunk length
	std::uint32_t binChunkLength = 0;
	glb.read(reinterpret_cast<char*>(&binChunkLength), sizeof binChunkLength);
	REQUIRE(binChunkLength == fastgltf::alignUp(asset->buffers.front().byteLength, 4));

	// Read & verify BIN chunk type id
	std::array<std::uint8_t, 4> binType {};
	glb.read(reinterpret_cast<char*>(binType.data()), sizeof binType);
	REQUIRE(binType[0] == 'B');
	REQUIRE(binType[1] == 'I');
	REQUIRE(binType[2] == 'N');
	REQUIRE(binType[3] == 0);

	glb.close();
	// Re-read the GLB
	auto result = run_official_gltf_validator(exportedGLBPath);
	REQUIRE(result);
	fastgltf::Parser new_parser(requiredExtensions);
	fastgltf::GltfFileStream rewrittenGlb(exportedGLBPath);
	REQUIRE(rewrittenGlb.isOpen());
	fastgltf::Expected<fastgltf::Asset> rewrittenAsset = new_parser.loadGltfBinary(rewrittenGlb, exportedGLBPath.parent_path(), options);
	REQUIRE(rewrittenAsset.error() == fastgltf::Error::None);
	REQUIRE(getErrorName(fastgltf::validate(rewrittenAsset.get())) == "None");
	REQUIRE(isSame(asset.get(), rewrittenAsset.get()));
}

std::vector<std::filesystem::path> getSampleAssets(const std::filesystem::path& dir, bool include_text = false, bool include_binary = false) {
  std::vector<std::filesystem::path> models;
	try {
		for (const auto& entry : std::filesystem::directory_iterator(dir)) {
			if (entry.is_directory()) {
				auto subModels = getSampleAssets(entry.path(), include_text, include_binary);
				models.insert(models.end(), subModels.begin(), subModels.end());
			} else if (entry.is_regular_file()) {
				auto ext = entry.path().extension();
				if (include_text && ext == ".gltf") {
					models.push_back(entry.path());
				} else if (include_binary && ext == ".glb") {
					models.push_back(entry.path());
				}
			}
		}
	} catch (const std::exception& e) {
	}
    return models;
}

TEST_CASE("Try writing a GLB with all buffers and images (ALL IN MODEL)", "[write-tests]") {
	auto models = getSampleAssets(sampleAssets / "Models", false, true);
	// Destroy the directory to make sure that the FileExporter correctly creates directories.
	// auto exportedFolder = path / "export_glb";
	// if (std::filesystem::exists(exportedFolder)) {
	// 	std::error_code ec;
	// 	std::filesystem::remove_all(exportedFolder, ec);
	// 	REQUIRE(!ec);
	// }

	for (const auto& model : models) {
		// get the directory relative to sampleAssets
		auto rel_dir = std::filesystem::relative(model, sampleAssets);
		SECTION(rel_dir.string()) {
			// we want to get the relative directory to sampleAssets
			auto model_parent_path = std::filesystem::relative(model.parent_path(), sampleAssets);
			test_glb_rewrite(model_parent_path, model.filename());
		}
	}
}

TEST_CASE("Try writing a GLB with all buffers and images (DUCK)", "[write-tests]") {
	test_glb_rewrite(std::filesystem::path("Models") / "Duck" / "glTF-Binary", "Duck.glb");
}

TEST_CASE("Try writing a GLB with all buffers and images", "[write-tests]") {
    auto cubePath = sampleAssets / "Models" / "Cube" / "glTF";

	fastgltf::GltfFileStream cubeJson(cubePath / "Cube.gltf");
	REQUIRE(cubeJson.isOpen());

    fastgltf::Parser parser;
    auto options = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;
    auto cube = parser.loadGltfJson(cubeJson, cubePath, options);
    REQUIRE(cube.error() == fastgltf::Error::None);

	// Destroy the directory to make sure that the FileExporter correctly creates directories.
	auto exportedFolder = path / "export_glb";
	if (std::filesystem::exists(exportedFolder)) {
		std::error_code ec;
		std::filesystem::remove_all(exportedFolder, ec);
		REQUIRE(!ec);
	}

    fastgltf::FileExporter exporter;
	auto exportedPath = exportedFolder / "cube.glb";
    auto error = exporter.writeGltfBinary(cube.get(), exportedPath);
    REQUIRE(error == fastgltf::Error::None);
	REQUIRE(std::filesystem::exists(exportedFolder / "image0.bin"));
	REQUIRE(std::filesystem::exists(exportedFolder / "image1.bin"));

	// Make sure the GLB buffer is written
	std::ifstream glb(exportedPath, std::ios::binary);
	REQUIRE(glb.is_open());

	// Skip over the header
	glb.seekg(sizeof(std::uint32_t) * 3, std::ifstream::beg);

	// Read the chunk length
	std::uint32_t chunkLength = 0;
	glb.read(reinterpret_cast<char*>(&chunkLength), sizeof chunkLength);

	// Skip over the chunk type + chunk
	glb.seekg(sizeof(std::uint32_t) + fastgltf::alignUp(chunkLength, 4), std::ifstream::cur);

	// Read binary chunk length
	glb.read(reinterpret_cast<char*>(&chunkLength), sizeof chunkLength);
	REQUIRE(chunkLength == cube->buffers.front().byteLength);

	// Read & verify BIN chunk type id
	std::array<std::uint8_t, 4> binType {};
	glb.read(reinterpret_cast<char*>(binType.data()), sizeof binType);
	REQUIRE(binType[0] == 'B');
	REQUIRE(binType[1] == 'I');
	REQUIRE(binType[2] == 'N');
	REQUIRE(binType[3] == 0);
}

TEST_CASE("Test string escape", "[write-tests]") {
    std::string x = "\"stuff\\";
    std::string escaped = fastgltf::escapeString(x);
    REQUIRE(escaped == "\\\"stuff\\\\");
}

TEST_CASE("Test pretty-print", "[write-tests]") {
    std::string json = R"({"value":5,"thing":{}})";
    fastgltf::prettyPrintJson(json);
	REQUIRE(json == "{\n\t\"value\":5,\n\t\"thing\":{\n\t\t\n\t}\n}");
}

TEST_CASE("Test all local models and re-export them", "[write-tests]") {
	// Enable all extensions
	static constexpr auto requiredExtensions = static_cast<fastgltf::Extensions>(~0U);
	fastgltf::Parser parser(requiredExtensions);

	static std::filesystem::path folderPath = "";
	if (folderPath.empty()) {
		SKIP();
	}

	std::uint64_t testedAssets = 0;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath)) {
		if (!entry.is_regular_file())
			continue;
		const auto& epath = entry.path();
		if (!epath.has_extension())
			continue;
		if (epath.extension() != ".gltf" && epath.extension() != ".glb")
			continue;

		// Parse the glTF
		fastgltf::GltfFileStream gltfData(epath);
		auto model = parser.loadGltf(gltfData, epath.parent_path());
		if (model.error() == fastgltf::Error::UnsupportedVersion || model.error() == fastgltf::Error::UnknownRequiredExtension || model.error() == fastgltf::Error::InvalidOrMissingAssetField)
			continue; // Skip any glTF 1.0 or 0.x files or glTFs with unsupported extensions.

		REQUIRE(model.error() == fastgltf::Error::None);
		REQUIRE(fastgltf::validate(model.get()) == fastgltf::Error::None);

		// Re-export the glTF as an in-memory JSON
		fastgltf::Exporter exporter;
		auto exported = exporter.writeGltfJson(model.get());
		REQUIRE(exported.error() == fastgltf::Error::None);

		// UTF-8 validation on the exported JSON string
		auto& exportedJson = exported.get().output;
		REQUIRE(simdjson::validate_utf8(exportedJson));

		// Parse the re-generated glTF and validate
		auto regeneratedJson = fastgltf::GltfDataBuffer::FromBytes(
				reinterpret_cast<const std::byte*>(exportedJson.data()), exportedJson.size());
		REQUIRE(regeneratedJson.error() == fastgltf::Error::None);
		auto regeneratedModel = parser.loadGltf(regeneratedJson.get(), epath.parent_path());
		REQUIRE(regeneratedModel.error() == fastgltf::Error::None);

		REQUIRE(fastgltf::validate(regeneratedModel.get()) == fastgltf::Error::None);

		++testedAssets;
	}
	printf("Successfully tested fastgltf exporter on %" PRIu64 " assets.", testedAssets);
}

TEST_CASE("Test Unicode exporting", "[write-tests]") {
#if FASTGLTF_CPP_20
	auto unicodePath = sampleAssets / "Models" / std::filesystem::path(u8"Unicode❤♻Test") / "glTF";
	fastgltf::GltfFileStream jsonData(unicodePath / std::filesystem::path(u8"Unicode❤♻Test.gltf"));
#else
	auto unicodePath = sampleAssets / "Models" / std::filesystem::u8path(u8"Unicode❤♻Test") / "glTF";
	fastgltf::GltfFileStream jsonData(unicodePath / std::filesystem::u8path(u8"Unicode❤♻Test.gltf"));
#endif
	REQUIRE(jsonData.isOpen());

	fastgltf::Parser parser;
	auto asset = parser.loadGltfJson(jsonData, unicodePath);
	REQUIRE(asset.error() == fastgltf::Error::None);

	fastgltf::Exporter exporter;
	auto exported = exporter.writeGltfJson(asset.get());
	REQUIRE(exported.error() == fastgltf::Error::None);

	// UTF-8 validation on the exported JSON string
	auto& exportedJson = exported.get().output;
	REQUIRE(simdjson::validate_utf8(exportedJson));

	auto regeneratedJson = fastgltf::GltfDataBuffer::FromBytes(
			reinterpret_cast<const std::byte*>(exportedJson.data()), exportedJson.size());
	REQUIRE(regeneratedJson.error() == fastgltf::Error::None);
	auto reparsed = parser.loadGltfJson(regeneratedJson.get(), unicodePath);
	REQUIRE(reparsed.error() == fastgltf::Error::None);

	REQUIRE(!asset->materials.empty());
	REQUIRE(asset->materials[0].name == "Unicode❤♻Material");

	REQUIRE(!asset->buffers.empty());
	REQUIRE(std::holds_alternative<fastgltf::sources::URI>(asset->buffers.front().data));
	auto bufferUri = std::get<fastgltf::sources::URI>(asset->buffers.front().data);
	REQUIRE(bufferUri.uri.path() == "Unicode❤♻Binary.bin");
}

TEST_CASE("Test URI normalization and removing backslashes", "[write-tests]") {
#if FASTGLTF_CPP_20
	auto unicodePath = sampleAssets / "Models" / std::filesystem::path(u8"Unicode❤♻Test") / "glTF";
	fastgltf::GltfFileStream jsonData(unicodePath / std::filesystem::path(u8"Unicode❤♻Test.gltf"));
#else
	auto unicodePath = sampleAssets / "Models" / std::filesystem::u8path(u8"Unicode❤♻Test") / "glTF";
	fastgltf::GltfFileStream jsonData(unicodePath / std::filesystem::u8path(u8"Unicode❤♻Test.gltf"));
#endif
	REQUIRE(jsonData.isOpen());

	fastgltf::Parser parser;
	auto asset = parser.loadGltfJson(jsonData, unicodePath, fastgltf::Options::LoadExternalImages);
	REQUIRE(asset.error() == fastgltf::Error::None);

	REQUIRE(asset->images.size() == 1);
	asset->images.front().name = "Unicode❤♻Texture";

	// When exporting, we specify a redundant image base path to test if the normalization properly works.
	// Additionally, on Windows we test if the slashes are changed properly.
	fastgltf::Exporter exporter;
#ifdef _WIN32
	// When on Windows the default path separator is a backslash, but glTF requires a forward slash.
	exporter.setImagePath(".\\textures1\\textures2\\..\\");
#else
	exporter.setImagePath("./textures1/textures2/../");
#endif
	auto exported = exporter.writeGltfJson(asset.get());

	// Parse the JSON and inspect the image URI.
	simdjson::ondemand::parser jsonParser;
	simdjson::ondemand::document doc;
	REQUIRE(jsonParser.iterate(exported.get().output.c_str(), exported.get().output.size(), exported.get().output.capacity()).get(doc) == simdjson::SUCCESS);

	simdjson::ondemand::object object;
	REQUIRE(doc.get_object().get(object) == simdjson::SUCCESS);

	simdjson::ondemand::array images;
	REQUIRE(doc["images"].get_array().get(images) == simdjson::SUCCESS);
	REQUIRE(images.count_elements().value() == 1);

	simdjson::ondemand::object imageObject;
	REQUIRE(images.at(0).get_object().get(imageObject) == simdjson::SUCCESS);

	std::string_view imageUri;
	REQUIRE(imageObject["uri"].get_string().get(imageUri) == simdjson::SUCCESS);
	REQUIRE(imageUri == "textures1/Unicode❤♻Texture.bin");
}

TEST_CASE("Test floating point round-trip precision", "[write-tests]") {
	auto cubePath = sampleAssets / "Models" / "Duck" / "glTF-Binary";

	fastgltf::Parser parser;

	auto original = [&parser, &cubePath]() {
		fastgltf::GltfFileStream cubeJson(cubePath / "Duck.glb");
		REQUIRE(cubeJson.isOpen());

		auto asset = parser.loadGltfBinary(cubeJson, cubePath,
			fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages);
		REQUIRE(asset.error() == fastgltf::Error::None);
		REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);
		return std::move(asset.get());
	}();

	fastgltf::FileExporter exporter;
	auto exportedPath = path / "export_glb" / "Duck_rewritten.glb";
	REQUIRE(exporter.writeGltfBinary(original, exportedPath) == fastgltf::Error::None);

	// Now read the exported file again, and check if the floating-point values are the same as the original asset.
	auto reimported = [&parser, &exportedPath] {
		fastgltf::GltfFileStream rewrittenJson(exportedPath);
		REQUIRE(rewrittenJson.isOpen());

		auto asset = parser.loadGltfBinary(rewrittenJson, exportedPath.parent_path());
		REQUIRE(asset.error() == fastgltf::Error::None);
		REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);
		return std::move(asset.get());
	}();

	REQUIRE(original.accessors.size() == reimported.accessors.size());
	for (std::size_t i = 0; i < reimported.accessors.size(); ++i) {
		const auto& accessor_a = original.accessors[i];
		const auto& accessor_b = reimported.accessors[i];

		REQUIRE(accessor_a.min.has_value());
		REQUIRE(accessor_b.min.has_value());
		REQUIRE(accessor_a.min->type() == accessor_b.min->type());
		if (!accessor_a.min->isType<double>())
			continue;

		const auto& min_a = accessor_a.min.value();
		const auto& min_b = accessor_b.min.value();
		REQUIRE(min_a.size() == min_b.size());
		for (std::size_t j = 0; j < min_a.size(); ++j) {
			REQUIRE(min_a.get<double>(j) == min_b.get<double>(j));
		}

		REQUIRE(accessor_a.max->type() == accessor_b.max->type());
		if (!accessor_a.max->isType<double>())
			continue;

		const auto& max_a = accessor_a.max.value();
		const auto& max_b = accessor_b.max.value();
		REQUIRE(max_a.size() == max_b.size());
		for (std::size_t j = 0; j < max_a.size(); ++j) {
			REQUIRE(max_a.get<double>(j) == max_b.get<double>(j));
		}
	}

	REQUIRE(original.materials.size() == reimported.materials.size());
	for (std::size_t i = 0; i < reimported.materials.size(); ++i) {
		const auto& material_a = original.materials[i];
		const auto& material_b = reimported.materials[i];

		for (std::size_t j = 0; j < 4; ++j) {
			REQUIRE(material_a.pbrData.baseColorFactor[j] == material_b.pbrData.baseColorFactor[j]);
		}
		REQUIRE(material_a.pbrData.metallicFactor == material_b.pbrData.metallicFactor);
		REQUIRE(material_a.pbrData.roughnessFactor == material_b.pbrData.roughnessFactor);

		for (std::size_t j = 0; j < 3; ++j) {
			REQUIRE(material_a.emissiveFactor[j] == material_b.emissiveFactor[j]);
		}
		REQUIRE(material_a.alphaCutoff == material_b.alphaCutoff);
	}
}

TEST_CASE("Test Accessor::updateBoundsToInclude", "[write-tests]") {
	SECTION("Scalar") {
		fastgltf::Accessor accessor;

		accessor.updateBoundsToInclude(static_cast<std::int64_t>(2));
		accessor.updateBoundsToInclude(static_cast<std::int64_t>(4));
		accessor.updateBoundsToInclude(static_cast<std::int64_t>(-2));

		REQUIRE(accessor.max.has_value());
		REQUIRE(accessor.max->isType<std::int64_t>());
		REQUIRE(accessor.max->size() == 1);
		REQUIRE(accessor.max->get<std::int64_t>(0) == 4);

		REQUIRE(accessor.min.has_value());
		REQUIRE(accessor.min->isType<std::int64_t>());
		REQUIRE(accessor.min->size() == 1);
		REQUIRE(accessor.min->get<std::int64_t>(0) == -2);
	}

	SECTION("Doubles") {
		fastgltf::Accessor accessor;

		accessor.updateBoundsToInclude(fastgltf::math::f64vec3(1.0, 2.0, 3.0));
		accessor.updateBoundsToInclude(fastgltf::math::f64vec3(2.0, 3.0, -4.0));
		accessor.updateBoundsToInclude(fastgltf::math::f64vec3(0.0, 0.0, 0.0));

		REQUIRE(accessor.max.has_value());
		REQUIRE(accessor.max->size() == 3);
		REQUIRE(accessor.max->isType<double>());
		REQUIRE(!accessor.max->isType<std::int64_t>());
		REQUIRE(accessor.max->get<double>(0) == 2.0);
		REQUIRE(accessor.max->get<double>(1) == 3.0);
		REQUIRE(accessor.max->get<double>(2) == 3.0);

		REQUIRE(accessor.min.has_value());
		REQUIRE(accessor.min->size() == 3);
		REQUIRE(accessor.min->isType<double>());
		REQUIRE(!accessor.min->isType<std::int64_t>());
		REQUIRE(accessor.min->get<double>(0) == 0.0);
		REQUIRE(accessor.min->get<double>(1) == 0.0);
		REQUIRE(accessor.min->get<double>(2) == -4.0);
	}

	SECTION("Integers") {
		fastgltf::Accessor accessor;

		accessor.updateBoundsToInclude(fastgltf::math::s64vec3(1, 2, 3));
		accessor.updateBoundsToInclude(fastgltf::math::s64vec3(2, 3, -4));
		accessor.updateBoundsToInclude(fastgltf::math::s64vec3(0, 0, 0));

		REQUIRE(accessor.max.has_value());
		REQUIRE(accessor.max->size() == 3);
		REQUIRE(!accessor.max->isType<double>());
		REQUIRE(accessor.max->isType<std::int64_t>());
		REQUIRE(accessor.max->get<std::int64_t>(0) == 2);
		REQUIRE(accessor.max->get<std::int64_t>(1) == 3);
		REQUIRE(accessor.max->get<std::int64_t>(2) == 3);

		REQUIRE(accessor.min.has_value());
		REQUIRE(accessor.min->size() == 3);
		REQUIRE(!accessor.min->isType<double>());
		REQUIRE(accessor.min->isType<std::int64_t>());
		REQUIRE(accessor.min->get<std::int64_t>(0) == 0);
		REQUIRE(accessor.min->get<std::int64_t>(1) == 0);
		REQUIRE(accessor.min->get<std::int64_t>(2) == -4);
	}
}
