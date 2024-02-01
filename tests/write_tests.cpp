#include <algorithm>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include <fastgltf/core.hpp>
#include "gltf_path.hpp"

TEST_CASE("Test stringifyExtensionBits function", "[write-tests]") {
	// Just used random extensions here to see if the stringify function works
	auto exts1 = fastgltf::stringifyExtensionBits(fastgltf::Extensions::KHR_lights_punctual | fastgltf::Extensions::EXT_meshopt_compression);
	REQUIRE(exts1.end() != std::find_if(exts1.begin(), exts1.end(), [](decltype(exts1)::value_type& ext) {
		return ext == fastgltf::extensions::KHR_lights_punctual;
	}));
	REQUIRE(exts1.end() != std::find_if(exts1.begin(), exts1.end(), [](decltype(exts1)::value_type& ext) {
		return ext == fastgltf::extensions::EXT_meshopt_compression;
	}));
	REQUIRE(exts1.end() == std::find_if(exts1.begin(), exts1.end(), [](decltype(exts1)::value_type& ext) {
		return ext == fastgltf::extensions::EXT_mesh_gpu_instancing;
	}));
}

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
	auto cubePath = sampleModels / "2.0" / "Cube" / "glTF";
	auto cubeJsonData = std::make_unique<fastgltf::GltfDataBuffer>();
	REQUIRE(cubeJsonData->loadFromFile(cubePath / "Cube.gltf"));

	fastgltf::Parser parser;
	auto cube = parser.loadGltfJson(cubeJsonData.get(), cubePath);
	REQUIRE(cube.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(cube.get()) == fastgltf::Error::None);

	fastgltf::Exporter exporter;
	auto expected = exporter.writeGltfJson(cube.get());
    REQUIRE(expected.error() == fastgltf::Error::None);

	fastgltf::GltfDataBuffer cube2JsonData;
	cube2JsonData.copyBytes(reinterpret_cast<const uint8_t*>(expected.get().output.data()),
                            expected.get().output.size());
	auto cube2 = parser.loadGltfJson(&cube2JsonData, cubePath);
	REQUIRE(cube2.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(cube2.get()) == fastgltf::Error::None);
}

TEST_CASE("Rewrite read glTF with multiple material extensions", "[write-tests]") {
	auto dishPath = sampleModels / "2.0" / "IridescentDishWithOlives" / "glTF";
	fastgltf::GltfDataBuffer dishJsonData;
	REQUIRE(dishJsonData.loadFromFile(dishPath / "IridescentDishWithOlives.gltf"));

	static constexpr auto requiredExtensions = fastgltf::Extensions::KHR_materials_ior |
		fastgltf::Extensions::KHR_materials_iridescence |
		fastgltf::Extensions::KHR_materials_transmission |
		fastgltf::Extensions::KHR_materials_volume;

	fastgltf::Parser parser(requiredExtensions);
	auto dish = parser.loadGltfJson(&dishJsonData, dishPath);
	REQUIRE(dish.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(dish.get()) == fastgltf::Error::None);

	fastgltf::Exporter exporter;
	auto expected = exporter.writeGltfJson(dish.get());
	REQUIRE(expected.error() == fastgltf::Error::None);

	fastgltf::GltfDataBuffer exportedDishJsonData;
	exportedDishJsonData.copyBytes(reinterpret_cast<const uint8_t*>(expected.get().output.data()),
								   expected.get().output.size());
	auto exportedDish = parser.loadGltfJson(&exportedDishJsonData, dishPath);
	REQUIRE(exportedDish.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(exportedDish.get()) == fastgltf::Error::None);
}

TEST_CASE("Try writing a glTF with all buffers and images", "[write-tests]") {
    auto cubePath = sampleModels / "2.0" / "Cube" / "glTF";

    fastgltf::GltfDataBuffer gltfDataBuffer;
    gltfDataBuffer.loadFromFile(cubePath / "Cube.gltf");

    fastgltf::Parser parser;
    auto options = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;
    auto cube = parser.loadGltfJson(&gltfDataBuffer, cubePath, options);
    REQUIRE(cube.error() == fastgltf::Error::None);

    std::filesystem::create_directory(path / "export");
    fastgltf::FileExporter exporter;
    auto error = exporter.writeGltfJson(cube.get(), path / "export" / "cube.gltf",
                                        fastgltf::ExportOptions::PrettyPrintJson);
    REQUIRE(error == fastgltf::Error::None);
}

TEST_CASE("Try writing a GLB with all buffers and images", "[write-tests]") {
    auto cubePath = sampleModels / "2.0" / "Cube" / "glTF";

    fastgltf::GltfDataBuffer gltfDataBuffer;
    gltfDataBuffer.loadFromFile(cubePath / "Cube.gltf");

    fastgltf::Parser parser;
    auto options = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;
    auto cube = parser.loadGltfJson(&gltfDataBuffer, cubePath, options);
    REQUIRE(cube.error() == fastgltf::Error::None);

    std::filesystem::create_directory(path / "export_glb");
    fastgltf::FileExporter exporter;
    auto error = exporter.writeGltfBinary(cube.get(), path / "export_glb" / "cube.glb");
    REQUIRE(error == fastgltf::Error::None);
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
		fastgltf::GltfDataBuffer gltfDataBuffer;
		gltfDataBuffer.loadFromFile(epath);
		auto model = parser.loadGltf(&gltfDataBuffer, epath.parent_path());
		if (model.error() == fastgltf::Error::UnsupportedVersion || model.error() == fastgltf::Error::UnknownRequiredExtension)
			continue; // Skip any glTF 1.0 or 0.x files or glTFs with unsupported extensions.

		REQUIRE(model.error() == fastgltf::Error::None);

		REQUIRE(fastgltf::validate(model.get()) == fastgltf::Error::None);

		// Re-export the glTF as an in-memory JSON
		fastgltf::Exporter exporter;
		auto exported = exporter.writeGltfJson(model.get());
		REQUIRE(exported.error() == fastgltf::Error::None);

		// Parse the re-generated glTF and validate
		auto& exportedJson = exported.get().output;
		fastgltf::GltfDataBuffer regeneratedJson;
		regeneratedJson.copyBytes(reinterpret_cast<const uint8_t*>(exportedJson.data()), exportedJson.size());
		auto regeneratedModel = parser.loadGltf(&regeneratedJson, epath.parent_path());
		REQUIRE(regeneratedModel.error() == fastgltf::Error::None);

		REQUIRE(fastgltf::validate(regeneratedModel.get()) == fastgltf::Error::None);

		++testedAssets;
	}
	std::printf("Successfully tested fastgltf exporter on %llu assets.", testedAssets);
}
