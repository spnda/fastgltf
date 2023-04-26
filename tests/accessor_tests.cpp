#include <cstdlib>
#include <random>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "base64_decode.hpp"
#include "fastgltf_parser.hpp"
#include "fastgltf_types.hpp"
#include "fastgltf_tools.hpp"
#include "gltf_path.hpp"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

template<>
struct fastgltf::ElementTraits<glm::vec3> {
	using element_type = glm::vec3;
	using component_type = float;
	static constexpr auto type = AccessorType::Vec3;
	static constexpr auto componentType = ComponentType::Float;
};

static const std::byte* getBufferData(const fastgltf::Buffer& buffer) {
	const std::byte* result = nullptr;

	std::visit(overloaded {
		[](auto&) {},
		[&](const fastgltf::sources::Vector& vec) {
			result = reinterpret_cast<const std::byte*>(vec.bytes.data());
		},
		[&](const fastgltf::sources::ByteView& bv) {
			result = bv.bytes.data();
		},
	}, buffer.data);
	
	return result;
}

TEST_CASE("Test accessor ", "[gltf-tools]") {
    auto lightsLamp = sampleModels / "2.0" / "LightsPunctualLamp" / "glTF";
    fastgltf::GltfDataBuffer jsonData;
    REQUIRE(jsonData.loadFromFile(lightsLamp / "LightsPunctualLamp.gltf"));

    fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual);
    auto model = parser.loadGLTF(&jsonData, lightsLamp, fastgltf::Options::LoadExternalBuffers);
    REQUIRE(parser.getError() == fastgltf::Error::None);
    REQUIRE(model->parse() == fastgltf::Error::None);
    //REQUIRE(model->parse(fastgltf::Category::Buffers | fastgltf::Category::BufferViews
			//| fastgltf::Category::Accessors) == fastgltf::Error::None);
    REQUIRE(model->validate() == fastgltf::Error::None);

    auto asset = model->getParsedAsset();
    REQUIRE(asset->accessors.size() == 15);
    auto& accessors = asset->accessors;

    {
        auto& firstAccessor = accessors[0];
		REQUIRE(firstAccessor.type == fastgltf::AccessorType::Scalar);
		REQUIRE(firstAccessor.componentType == fastgltf::ComponentType::UnsignedShort);

		REQUIRE(firstAccessor.bufferViewIndex.has_value());
		auto& view = asset->bufferViews[*firstAccessor.bufferViewIndex];

		auto* bufferData = getBufferData(asset->buffers[view.bufferIndex]);
		REQUIRE(bufferData != nullptr);

		auto* checkData = reinterpret_cast<const unsigned short*>(bufferData + view.byteOffset
				+ firstAccessor.byteOffset);

		REQUIRE(*checkData == fastgltf::getAccessorElement<unsigned short>(*asset, firstAccessor, 0));
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
		REQUIRE(*checkData == fastgltf::getAccessorElement<glm::vec3>(*asset, secondAccessor, 0));
	}
}

