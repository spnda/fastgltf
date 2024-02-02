#include <catch2/catch_test_macros.hpp>

#include <glm/vec3.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/ext/scalar_constants.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include "gltf_path.hpp"

static const std::byte* getBufferData(const fastgltf::Buffer& buffer) {
	const std::byte* result = nullptr;

	std::visit(fastgltf::visitor {
		[](auto&) {},
		[&](const fastgltf::sources::Array& vec) {
			result = reinterpret_cast<const std::byte*>(vec.bytes.data());
		},
		[&](const fastgltf::sources::ByteView& bv) {
			result = bv.bytes.data();
		},
	}, buffer.data);
	
	return result;
}

TEST_CASE("Test data type conversion", "[gltf-tools]") {
	// normalized int-to-float and normalized float-to-int
	for (auto i = std::numeric_limits<std::int8_t>::min(); i < std::numeric_limits<std::int8_t>::max(); ++i) {
		auto converted = fastgltf::internal::convertComponent<float>(i, true);
		REQUIRE(glm::epsilonEqual<float>(converted, fastgltf::max<float>(i / 127.0f, -1), glm::epsilon<float>()));
		REQUIRE(fastgltf::internal::convertComponent<std::int8_t>(converted, true) == std::round(converted * 127.0f));
	}
	for (auto i = std::numeric_limits<std::uint8_t>::min(); i < std::numeric_limits<std::uint8_t>::max(); ++i) {
		auto converted = fastgltf::internal::convertComponent<float>(i, true);
		REQUIRE(glm::epsilonEqual<float>(converted, i / 255.0f, glm::epsilon<float>()));
		REQUIRE(fastgltf::internal::convertComponent<std::uint8_t>(converted, true) == std::round(converted * 255.0f));
	}
	for (auto i = std::numeric_limits<std::int16_t>::min(); i < std::numeric_limits<std::int16_t>::max(); ++i) {
		auto converted = fastgltf::internal::convertComponent<float>(i, true);
		REQUIRE(glm::epsilonEqual<float>(converted, fastgltf::max<float>(i / 32767.0f, -1), glm::epsilon<float>()));
		REQUIRE(fastgltf::internal::convertComponent<std::int16_t>(converted, true) == std::round(converted * 32767.0f));
	}
	for (auto i = std::numeric_limits<std::uint16_t>::min(); i < std::numeric_limits<std::uint16_t>::max(); ++i) {
		auto converted = fastgltf::internal::convertComponent<float>(i, true);
		REQUIRE(glm::epsilonEqual<float>(converted, i / 65535.0f, glm::epsilon<float>()));
		REQUIRE(fastgltf::internal::convertComponent<std::uint16_t>(converted, true) == std::round(converted * 65535.0f));
	}
}

TEST_CASE("Test little-endian correctness", "[gltf-tools]") {
    // The test here is merely to verify that the internal deserialization functions correctly treat
    // the input bytes as little-endian, regardless of system endianness.
    // This test is effectively useless on little endian systems, but it should still make sense to keep it.
    std::array<std::byte, 4> integer {{ std::byte(0x0A), std::byte(0x0B), std::byte(0x0C), std::byte(0x0D) }};
    auto deserialized = fastgltf::internal::deserializeComponent<std::uint32_t>(integer.data(), 0);
    REQUIRE(deserialized == 0x0D0C0B0A);
}

TEST_CASE("Test matrix data padding", "[gltf-tools]") {
    // First a case that doesn't require any padding
    std::array<std::uint16_t, 4> unpaddedMat2 {{
       1, 2,
       3, 4
    }};
    REQUIRE(fastgltf::getElementByteSize(fastgltf::AccessorType::Mat2, fastgltf::ComponentType::UnsignedShort) == unpaddedMat2.size() * sizeof(std::uint16_t));
    auto umat2 = fastgltf::internal::getAccessorElementAt<glm::mat2x2>(
            fastgltf::ComponentType::UnsignedShort,
            reinterpret_cast<const std::byte*>(unpaddedMat2.data()));
    REQUIRE(umat2[0] == glm::vec2(1, 2));
    REQUIRE(umat2[1] == glm::vec2(3, 4));

    // This will simulate a padded 2x2 matrix with the correct 4-byte padding per column
    std::array<std::uint8_t, 8> paddedMat2 {{
        1, 2, 0, 0,
        3, 4, 0, 0
    }};
    REQUIRE(fastgltf::getElementByteSize(fastgltf::AccessorType::Mat2, fastgltf::ComponentType::UnsignedByte) == paddedMat2.size());
    auto mat2 = fastgltf::internal::getAccessorElementAt<glm::mat2x2>(
            fastgltf::ComponentType::UnsignedByte,
            reinterpret_cast<const std::byte*>(paddedMat2.data()));
    REQUIRE(mat2[0] == glm::vec2(1, 2));
    REQUIRE(mat2[1] == glm::vec2(3, 4));

    std::array<std::uint8_t, 12> paddedMat3 {{
        1, 2, 3, 0,
        4, 5, 6, 0,
        7, 8, 9, 0
    }};
    REQUIRE(fastgltf::getElementByteSize(fastgltf::AccessorType::Mat3, fastgltf::ComponentType::UnsignedByte) == paddedMat3.size());
    auto mat3 = fastgltf::internal::getAccessorElementAt<glm::mat3x3>(
            fastgltf::ComponentType::UnsignedByte,
            reinterpret_cast<const std::byte*>(paddedMat3.data()));
    REQUIRE(mat3[0] == glm::vec3(1, 2, 3));
    REQUIRE(mat3[1] == glm::vec3(4, 5, 6));
    REQUIRE(mat3[2] == glm::vec3(7, 8, 9));

    // This now uses 16-bit shorts for the component types.
    std::array<std::uint16_t, 12> padded2BMat3 {{
        1, 2, 3, 0,
        4, 5, 6, 0,
        7, 8, 9, 0
    }};
    REQUIRE(fastgltf::getElementByteSize(fastgltf::AccessorType::Mat3, fastgltf::ComponentType::UnsignedShort) == paddedMat3.size() * sizeof(std::uint16_t));
    auto mat3_2 = fastgltf::internal::getAccessorElementAt<glm::mat3x3>(
            fastgltf::ComponentType::UnsignedShort,
            reinterpret_cast<const std::byte*>(padded2BMat3.data()));
    REQUIRE(mat3_2[0] == glm::vec3(1, 2, 3));
    REQUIRE(mat3_2[1] == glm::vec3(4, 5, 6));
    REQUIRE(mat3_2[2] == glm::vec3(7, 8, 9));
}

TEST_CASE("Test accessor", "[gltf-tools]") {
    auto lightsLamp = sampleModels / "2.0" / "LightsPunctualLamp" / "glTF";
    fastgltf::GltfDataBuffer jsonData;
    REQUIRE(jsonData.loadFromFile(lightsLamp / "LightsPunctualLamp.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual);
    auto asset = parser.loadGltfJson(&jsonData, lightsLamp, fastgltf::Options::LoadExternalBuffers,
								 fastgltf::Category::Buffers | fastgltf::Category::BufferViews | fastgltf::Category::Accessors);
    REQUIRE(asset.error() == fastgltf::Error::None);

    REQUIRE(asset->accessors.size() == 15);
    auto& accessors = asset->accessors;

    SECTION("getAccessorElement<std::uint16_t>") {
        auto& firstAccessor = accessors[0];
		REQUIRE(firstAccessor.type == fastgltf::AccessorType::Scalar);
		REQUIRE(firstAccessor.componentType == fastgltf::ComponentType::UnsignedShort);

		REQUIRE(firstAccessor.bufferViewIndex.has_value());
		auto& view = asset->bufferViews[*firstAccessor.bufferViewIndex];

		auto* bufferData = getBufferData(asset->buffers[view.bufferIndex]);
		REQUIRE(bufferData != nullptr);

		auto* checkData = reinterpret_cast<const std::uint16_t*>(bufferData + view.byteOffset
				+ firstAccessor.byteOffset);

		REQUIRE(*checkData == fastgltf::getAccessorElement<std::uint16_t>(asset.get(), firstAccessor, 0));
    }

	{
        auto& secondAccessor = accessors[1];
		REQUIRE(secondAccessor.type == fastgltf::AccessorType::Vec3);
		REQUIRE(secondAccessor.componentType == fastgltf::ComponentType::Float);

		REQUIRE(secondAccessor.bufferViewIndex.has_value());
		auto& view = asset->bufferViews[*secondAccessor.bufferViewIndex];

		auto* bufferData = getBufferData(asset->buffers[view.bufferIndex]);
		REQUIRE(bufferData != nullptr);

		auto* checkData = reinterpret_cast<const glm::vec3*>(bufferData + view.byteOffset
				+ secondAccessor.byteOffset);

		SECTION("getAccessorElement<glm::vec3>") {
			REQUIRE(*checkData == fastgltf::getAccessorElement<glm::vec3>(asset.get(), secondAccessor, 0));
		}

		SECTION("iterateAccessor") {
			auto dstCopy = std::make_unique<glm::vec3[]>(secondAccessor.count);
			std::size_t i = 0;

			fastgltf::iterateAccessor<glm::vec3>(asset.get(), secondAccessor, [&](auto&& v3) {
				dstCopy[i++] = std::forward<glm::vec3>(v3);
			});

			REQUIRE(std::memcmp(dstCopy.get(), checkData, secondAccessor.count * sizeof(glm::vec3)) == 0);
		}

		SECTION("copyFromAccessor") {
			auto dstCopy = std::make_unique<glm::vec3[]>(secondAccessor.count);
			fastgltf::copyFromAccessor<glm::vec3>(asset.get(), secondAccessor, dstCopy.get());
			REQUIRE(std::memcmp(dstCopy.get(), checkData, secondAccessor.count * sizeof(glm::vec3)) == 0);
		}

		SECTION("Iterator test") {
			auto dstCopy = std::make_unique<glm::vec3[]>(secondAccessor.count);
			auto accessor = fastgltf::iterateAccessor<glm::vec3>(asset.get(), secondAccessor);
			for (auto it = accessor.begin(); it != accessor.end(); ++it) {
				dstCopy[std::distance(accessor.begin(), it)] = *it;
			}
			REQUIRE(std::memcmp(dstCopy.get(), checkData, secondAccessor.count * sizeof(glm::vec3)) == 0);
		}
	}
}

TEST_CASE("Test sparse accessor", "[gltf-tools]") {
    auto simpleSparseAccessor = sampleModels / "2.0" / "SimpleSparseAccessor" / "glTF";
    auto jsonData = std::make_unique<fastgltf::GltfDataBuffer>();
    REQUIRE(jsonData->loadFromFile(simpleSparseAccessor / "SimpleSparseAccessor.gltf"));

    fastgltf::Parser parser;
    auto asset = parser.loadGltfJson(jsonData.get(), simpleSparseAccessor, fastgltf::Options::LoadExternalBuffers,
								 fastgltf::Category::Buffers | fastgltf::Category::BufferViews | fastgltf::Category::Accessors);
    REQUIRE(asset.error() == fastgltf::Error::None);

    REQUIRE(asset->accessors.size() == 2);
    REQUIRE(!asset->accessors[0].sparse.has_value());
    REQUIRE(asset->accessors[1].sparse.has_value());
    auto& sparse = asset->accessors[1].sparse.value();
    REQUIRE(sparse.count == 3);
    REQUIRE(sparse.indicesBufferView == 2);
    REQUIRE(sparse.indicesByteOffset == 0);
    REQUIRE(sparse.valuesBufferView == 3);
    REQUIRE(sparse.valuesByteOffset == 0);
    REQUIRE(sparse.indexComponentType == fastgltf::ComponentType::UnsignedShort);

	auto& secondAccessor = asset->accessors[1];
	auto& viewIndices = asset->bufferViews[secondAccessor.sparse->indicesBufferView];
	auto& viewValues = asset->bufferViews[secondAccessor.sparse->valuesBufferView];

	auto& viewData = asset->bufferViews[*secondAccessor.bufferViewIndex];
	auto* bufferData = getBufferData(asset->buffers[viewData.bufferIndex]) + viewData.byteOffset
			+ secondAccessor.byteOffset;
	auto dataStride = viewData.byteStride ? *viewData.byteStride
			: fastgltf::getElementByteSize(secondAccessor.type, secondAccessor.componentType);

	auto* dataIndices = reinterpret_cast<const std::uint16_t*>(getBufferData(asset->buffers[viewIndices.bufferIndex])
			+ viewIndices.byteOffset + secondAccessor.sparse->indicesByteOffset);
	auto* dataValues = reinterpret_cast<const glm::vec3*>(getBufferData(asset->buffers[viewValues.bufferIndex])
			+ viewValues.byteOffset + secondAccessor.sparse->valuesByteOffset);

	auto checkValues = std::make_unique<glm::vec3[]>(secondAccessor.count);

	for (std::size_t i = 0, sparseIndex = 0; i < secondAccessor.count; ++i) {
		if (sparseIndex < secondAccessor.sparse->count && dataIndices[sparseIndex] == i) {
			checkValues[i] = dataValues[sparseIndex];
			++sparseIndex;
		} else {
			checkValues[i] = *reinterpret_cast<const glm::vec3*>(bufferData + dataStride * i);
		}
	}

	SECTION("getAccessorElement") {
		for (std::size_t i = 0; i < secondAccessor.count; ++i) {
			REQUIRE(checkValues[i] == fastgltf::getAccessorElement<glm::vec3>(asset.get(), secondAccessor, i));
		}
	}

	SECTION("iterateAccessor") {
		auto dstCopy = std::make_unique<glm::vec3[]>(secondAccessor.count);
		std::size_t i = 0;

		fastgltf::iterateAccessor<glm::vec3>(asset.get(), secondAccessor, [&](auto&& v3) {
			dstCopy[i++] = std::forward<glm::vec3>(v3);
		});

		REQUIRE(std::memcmp(dstCopy.get(), checkValues.get(), secondAccessor.count * sizeof(glm::vec3)) == 0);
	}

	SECTION("iterateAccessor with idx") {
		auto dstCopy = std::make_unique<glm::vec3[]>(secondAccessor.count);

		fastgltf::iterateAccessorWithIndex<glm::vec3>(asset.get(), secondAccessor, [&](auto&& v3, std::size_t i) {
			dstCopy[i] = std::forward<glm::vec3>(v3);
		});

		REQUIRE(std::memcmp(dstCopy.get(), checkValues.get(), secondAccessor.count * sizeof(glm::vec3)) == 0);
	}

	SECTION("copyFromAccessor") {
		auto dstCopy = std::make_unique<glm::vec3[]>(secondAccessor.count);
		fastgltf::copyFromAccessor<glm::vec3>(asset.get(), secondAccessor, dstCopy.get());
		REQUIRE(std::memcmp(dstCopy.get(), checkValues.get(), secondAccessor.count * sizeof(glm::vec3)) == 0);
	}

	SECTION("Iterator test") {
		auto dstCopy = std::make_unique<glm::vec3[]>(secondAccessor.count);
		auto accessor = fastgltf::iterateAccessor<glm::vec3>(asset.get(), secondAccessor);
		for (auto it = accessor.begin(); it != accessor.end(); ++it) {
			dstCopy[std::distance(accessor.begin(), it)] = *it;
		}
		REQUIRE(std::memcmp(dstCopy.get(), checkValues.get(), secondAccessor.count * sizeof(glm::vec3)) == 0);
	}
}
