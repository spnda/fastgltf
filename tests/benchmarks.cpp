#include <fstream>
#include <random>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <simdjson.h>

#include <fastgltf/core.hpp>
#include <fastgltf/base64.hpp>
#include <fastgltf/crc32.hpp>
#include "gltf_path.hpp"

constexpr auto benchmarkOptions = fastgltf::Options::DontRequireValidAssetMember;

#ifdef HAS_RAPIDJSON
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#endif

#ifdef HAS_TINYGLTF
// We don't want tinygltf to load/write images.
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_FS
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

bool tinygltf_FileExistsFunction([[maybe_unused]] const std::string& filename, [[maybe_unused]] void* user) {
	return true;
}

std::string tinygltf_ExpandFilePathFunction(const std::string& filePath, [[maybe_unused]] void* user) {
	return filePath;
}

bool tinygltf_ReadWholeFileFunction(std::vector<unsigned char>* data, std::string*, const std::string&, void*) {
	// tinygltf checks if size == 1. It also checks if the size is correct for glb files, but
	// well ignore that for now.
	data->resize(1);
	return true;
}

bool tinygltf_LoadImageData(tinygltf::Image *image, const int image_idx, std::string *err,
				   std::string *warn, int req_width, int req_height,
				   const unsigned char *bytes, int size, void *user_data) {
	return true;
}

void setTinyGLTFCallbacks(tinygltf::TinyGLTF& gltf) {
	gltf.SetFsCallbacks({
		tinygltf_FileExistsFunction,
		tinygltf_ExpandFilePathFunction,
		tinygltf_ReadWholeFileFunction,
		nullptr, nullptr,
	});
	gltf.SetImageLoader(tinygltf_LoadImageData, nullptr);
}
#endif

#ifdef HAS_TINYGLTF_V3
#include "tiny_gltf_v3.h"
#endif

#ifdef HAS_CGLTF
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#endif

#ifdef HAS_GLTFRS
#include "rust/cxx.h"
#include "gltf-rs-bridge/lib.h"
#endif

#ifdef HAS_ASSIMP
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/Base64.hpp>
#endif

fastgltf::static_vector<std::uint8_t> readFileAsBytes(const std::filesystem::path& filePath) {
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);
	if (!file.is_open())
		throw std::runtime_error(std::string { "Failed to open file: " } + filePath.string());

	auto fileSize = file.tellg();
	fastgltf::static_vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
	file.seekg(0, std::ifstream::beg);
	file.read(reinterpret_cast<char*>(bytes.data()), fileSize);
	file.close();
	return bytes;
}

TEST_CASE("Benchmark loading of NewSponza", "[!benchmark][gltf-benchmark]") {
	if (!std::filesystem::exists(intelSponza / "NewSponza_Main_glTF_002.gltf")) {
		// NewSponza is not part of gltf-Sample-Models, and therefore not always available.
		SKIP("Intel's NewSponza (GLTF) is required for this benchmark.");
	}

	fastgltf::Parser parser;
#ifdef HAS_TINYGLTF
	tinygltf::TinyGLTF tinygltf;
	tinygltf::Model model;
	std::string warn, err;
#endif
#ifdef HAS_TINYGLTF_V3
	tg3_parse_options opts;
	tg3_error_stack errors;
	tg3_model tg3_model;

	tg3_parse_options_init(&opts);
	tg3_error_stack_init(&errors);
#endif

	auto bytes = readFileAsBytes(intelSponza / "NewSponza_Main_glTF_002.gltf");
	auto jsonData = fastgltf::GltfDataBuffer::FromBytes(
			reinterpret_cast<const std::byte*>(bytes.data()), bytes.size());
	REQUIRE(jsonData.error() == fastgltf::Error::None);

	BENCHMARK("Parse NewSponza") {
		return parser.loadGltfJson(jsonData.get(), intelSponza, benchmarkOptions);
	};

#ifdef HAS_TINYGLTF
	setTinyGLTFCallbacks(tinygltf);
	BENCHMARK("Parse NewSponza with tinygltf") {
		return tinygltf.LoadASCIIFromString(&model, &err, &warn, reinterpret_cast<char*>(bytes.data()), bytes.size(), intelSponza.string());
	};
#endif

#ifdef HAS_TINYGLTF_V3
	BENCHMARK("Parse NewSponza with tinygltf v3") {
		return tg3_parse_auto(&tg3_model, &errors, bytes.data(), bytes.size(), "", 0, &opts);
	};
#endif

#ifdef HAS_CGLTF
	BENCHMARK("Parse NewSponza with cgltf") {
		cgltf_options options = {};
		cgltf_data* data = nullptr;
		cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &data);
		REQUIRE(result == cgltf_result_success);
		cgltf_free(data);
		return result;
	};
#endif

#ifdef HAS_GLTFRS
	BENCHMARK("Parse NewSponza with gltf-rs") {
		auto slice = rust::Slice<const std::uint8_t>(reinterpret_cast<std::uint8_t*>(bytes.data()), bytes.size());
		return rust::gltf::run(slice);
	};
#endif

#ifdef HAS_ASSIMP
	BENCHMARK("Parse NewSponza with assimp") {
		return aiImportFileFromMemory(reinterpret_cast<const char*>(bytes.data()), bytes.size(), 0, nullptr);
	};
#endif

#ifdef HAS_TINYGLTF_V3
	tg3_model_free(&tg3_model);
	tg3_error_stack_free(&errors);
#endif
}

TEST_CASE("Benchmark base64 decoding from glTF file", "[!benchmark][gltf-benchmark]") {
	fastgltf::Parser parser;
#ifdef HAS_TINYGLTF
	tinygltf::TinyGLTF tinygltf;
	tinygltf::Model model;
	std::string warn, err;
#endif
#ifdef HAS_TINYGLTF_V3
	tg3_parse_options opts;
	tg3_error_stack errors;
	tg3_model tg3_model;

	tg3_parse_options_init(&opts);
	tg3_error_stack_init(&errors);
#endif

	auto cylinderEngine = sampleAssets / "Models" / "MetalRoughSpheres" / "glTF-Embedded";
	auto bytes = readFileAsBytes(cylinderEngine / "MetalRoughSpheres.gltf");
	auto jsonData = fastgltf::GltfDataBuffer::FromBytes(
			reinterpret_cast<const std::byte*>(bytes.data()), bytes.size());
	REQUIRE(jsonData.error() == fastgltf::Error::None);

	BENCHMARK("Parse MetalRoughSpheres and decode base64") {
		return parser.loadGltfJson(jsonData.get(), cylinderEngine, benchmarkOptions);
	};

#ifdef HAS_TINYGLTF
	setTinyGLTFCallbacks(tinygltf);
	BENCHMARK("MetalRoughSpheres decode with tinygltf") {
		return tinygltf.LoadASCIIFromString(&model, &err, &warn, reinterpret_cast<char*>(bytes.data()), bytes.size(), cylinderEngine.string());
	};
#endif

#ifdef HAS_TINYGLTF_V3
	BENCHMARK("MetalRoughSpheres decode with tinygltf v3") {
		return tg3_parse_auto(&tg3_model, &errors, bytes.data(), bytes.size(), "", 0, &opts);
	};
#endif

#ifdef HAS_CGLTF
	BENCHMARK("MetalRoughSpheres decode with cgltf") {
		cgltf_options options = {};
		cgltf_data* data = nullptr;
		auto filePath = cylinderEngine.string();
		cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &data);
		REQUIRE(result == cgltf_result_success);
		result = cgltf_load_buffers(&options, data, filePath.c_str());
		cgltf_free(data);
		return result;
	};
#endif

#ifdef HAS_GLTFRS
	BENCHMARK("MetalRoughSpheres with gltf-rs") {
		auto slice = rust::Slice<const std::uint8_t>(reinterpret_cast<std::uint8_t*>(bytes.data()), bytes.size());
		return rust::gltf::run(slice);
	};
#endif

#ifdef HAS_ASSIMP
	BENCHMARK("MetalRoughSpheres with assimp") {
		const auto* scene = aiImportFileFromMemory(reinterpret_cast<const char*>(bytes.data()), bytes.size(), 0, nullptr);
		REQUIRE(scene != nullptr);
		return scene;
	};
#endif

#ifdef HAS_TINYGLTF_V3
	tg3_model_free(&tg3_model);
	tg3_error_stack_free(&errors);
#endif
}

TEST_CASE("Benchmark raw JSON parsing", "[!benchmark][gltf-benchmark]") {
	fastgltf::Parser parser;
#ifdef HAS_TINYGLTF
	tinygltf::TinyGLTF tinygltf;
	tinygltf::Model model;
	std::string warn, err;
#endif
#ifdef HAS_TINYGLTF_V3
	tg3_parse_options opts;
	tg3_error_stack errors;
	tg3_model tg3_model;

	tg3_parse_options_init(&opts);
	tg3_error_stack_init(&errors);
#endif

	auto sponzaPath = sampleAssets / "Models" / "Sponza" / "glTF";
	auto bytes = readFileAsBytes(sponzaPath / "Sponza.gltf");
	auto jsonData = fastgltf::GltfDataBuffer::FromBytes(
			reinterpret_cast<const std::byte*>(bytes.data()), bytes.size());
	REQUIRE(jsonData.error() == fastgltf::Error::None);

	BENCHMARK("Parse Sponza.gltf") {
		return parser.loadGltfJson(jsonData.get(), sponzaPath, benchmarkOptions);
	};

#ifdef HAS_TINYGLTF
	setTinyGLTFCallbacks(tinygltf);
	BENCHMARK("Parse Sponza.gltf with tinygltf") {
		return tinygltf.LoadASCIIFromString(&model, &err, &warn, reinterpret_cast<char*>(bytes.data()), bytes.size(), sponzaPath.string());
	};
#endif

#ifdef HAS_TINYGLTF_V3
	BENCHMARK("Parse Sponza.gltf with tinygltf v3") {
		return tg3_parse_auto(&tg3_model, &errors, bytes.data(), bytes.size(), "", 0, &opts);
	};
#endif

#ifdef HAS_CGLTF
	BENCHMARK("Parse Sponza.gltf with cgltf") {
		cgltf_options options = {};
		cgltf_data* data = nullptr;
		auto filePath = sponzaPath.string();
		cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &data);
		REQUIRE(result == cgltf_result_success);
		cgltf_free(data);
		return result;
	};
#endif

#ifdef HAS_GLTFRS
	BENCHMARK("Parse Sponza.gltf with gltf-rs") {
		auto slice = rust::Slice<const std::uint8_t>(reinterpret_cast<std::uint8_t*>(bytes.data()), bytes.size());
		return rust::gltf::run(slice);
	};
#endif

#ifdef HAS_ASSIMP
	BENCHMARK("Parse Sponza.gltf with assimp") {
		return aiImportFileFromMemory(reinterpret_cast<const char*>(bytes.data()), bytes.size(), 0, nullptr);
	};
#endif

#ifdef HAS_TINYGLTF_V3
	tg3_model_free(&tg3_model);
	tg3_error_stack_free(&errors);
#endif
}

TEST_CASE("Benchmark massive gltf file", "[!benchmark][gltf-benchmark]") {
	if (!std::filesystem::exists(bistroPath / "bistro.gltf")) {
		// Bistro is not part of gltf-Sample-Models, and therefore not always available.
		SKIP("Amazon's Bistro (GLTF) is required for this benchmark.");
	}

	fastgltf::Parser parser(fastgltf::Extensions::KHR_mesh_quantization);
#ifdef HAS_TINYGLTF
	tinygltf::TinyGLTF tinygltf;
	tinygltf::Model model;
	std::string warn, err;
#endif
#ifdef HAS_TINYGLTF_V3
	tg3_parse_options opts;
	tg3_error_stack errors;
	tg3_model tg3_model;

	tg3_parse_options_init(&opts);
	tg3_error_stack_init(&errors);
#endif

	auto bytes = readFileAsBytes(bistroPath / "bistro.gltf");
	auto jsonData = fastgltf::GltfDataBuffer::FromBytes(
			reinterpret_cast<const std::byte*>(bytes.data()), bytes.size());
	REQUIRE(jsonData.error() == fastgltf::Error::None);

	BENCHMARK("Parse Bistro") {
		return parser.loadGltfJson(jsonData.get(), bistroPath, benchmarkOptions);
	};

#ifdef HAS_TINYGLTF
	setTinyGLTFCallbacks(tinygltf);
	BENCHMARK("Parse Bistro with tinygltf") {
		return tinygltf.LoadASCIIFromString(&model, &err, &warn, reinterpret_cast<char*>(bytes.data()), bytes.size(), bistroPath.string());
	};
#endif

#ifdef HAS_TINYGLTF_V3
	BENCHMARK("Parse Bistro with tinygltf v3") {
		return tg3_parse_auto(&tg3_model, &errors, bytes.data(), bytes.size(), "", 0, &opts);
	};
#endif

#ifdef HAS_CGLTF
	BENCHMARK("Parse Bistro with cgltf") {
		cgltf_options options = {};
		cgltf_data* data = nullptr;
		auto filePath = bistroPath.string();
		cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &data);
		REQUIRE(result == cgltf_result_success);
		cgltf_free(data);
		return result;
	};
#endif

#ifdef HAS_GLTFRS
	BENCHMARK("Parse Bistro with gltf-rs") {
		auto slice = rust::Slice<const std::uint8_t>(reinterpret_cast<std::uint8_t*>(bytes.data()), bytes.size());
		return rust::gltf::run(slice);
	};
#endif

#ifdef HAS_ASSIMP
	BENCHMARK("Parse Bistro with assimp") {
		return aiImportFileFromMemory(reinterpret_cast<const char*>(bytes.data()), bytes.size(), 0, nullptr);
	};
#endif

#ifdef HAS_TINYGLTF_V3
	tg3_model_free(&tg3_model);
	tg3_error_stack_free(&errors);
#endif
}

TEST_CASE("Compare parsing performance with minified documents", "[!benchmark][gltf-benchmark]") {
	auto sponzaPath = sampleAssets / "Models" / "Sponza" / "glTF";
	auto bytes = readFileAsBytes(sponzaPath / "Sponza.gltf");
	auto jsonData = fastgltf::GltfDataBuffer::FromBytes(
			reinterpret_cast<const std::byte*>(bytes.data()), bytes.size());
	REQUIRE(jsonData.error() == fastgltf::Error::None);

	// Create a minified JSON string
	std::vector<uint8_t> minified(bytes.size());
	size_t dstLen = 0;
	auto result = simdjson::minify(reinterpret_cast<const char*>(bytes.data()), bytes.size(),
								   reinterpret_cast<char*>(minified.data()), dstLen);
	REQUIRE(result == simdjson::SUCCESS);
	minified.resize(dstLen);

	// For completeness, benchmark minifying the JSON
	BENCHMARK("Minify Sponza.gltf") {
		auto result = simdjson::minify(reinterpret_cast<const char*>(bytes.data()), bytes.size(),
									   reinterpret_cast<char*>(minified.data()), dstLen);
		REQUIRE(result == simdjson::SUCCESS);
		return result;
	};

	auto minifiedJsonData = fastgltf::GltfDataBuffer::FromBytes(
			reinterpret_cast<const std::byte*>(bytes.data()), bytes.size());
	REQUIRE(minifiedJsonData.error() == fastgltf::Error::None);

	fastgltf::Parser parser;
	BENCHMARK("Parse Sponza.gltf with normal JSON") {
		return parser.loadGltfJson(jsonData.get(), sponzaPath, benchmarkOptions);
	};

	BENCHMARK("Parse Sponza.gltf with minified JSON") {
		return parser.loadGltfJson(minifiedJsonData.get(), sponzaPath, benchmarkOptions);
	};
}

TEST_CASE("Small-string CRC32-C benchmark", "[!benchmark][gltf-benchmark][crc-benchmark]") {
	std::mt19937 gen;
	std::uniform_int_distribution<int> dist('a', 'z');
	std::string data(16, '\0');
	for (auto& c : data)
		c = static_cast<char>(dist(gen));

	BENCHMARK("Default 1-byte tabular algorithm") {
		return fastgltf::fallback_crc32c(data);
	};
#if defined(FASTGLTF_IS_X86)
	BENCHMARK("SSE4 hardware algorithm") {
		return fastgltf::sse_crc32c(data);
	};
#elif defined(FASTGLTF_ENABLE_ARMV8_CRC)
	BENCHMARK("ARMv8 hardware CRC32-C algorithm") {
		return fastgltf::armv8_crc32c(data);
	};
#endif
}

TEST_CASE("Large-string CRC32-C benchmark", "[!benchmark][crc-benchmark]") {
	// Every length up to 64 (the tail handling for len % 4 / % 8 matters here), then ~25% steps up to 8K.
	std::vector<std::size_t> lengths;
	for (std::size_t i = 1; i <= 64; ++i)
		lengths.push_back(i);
	for (std::size_t i = 80; i <= 8192; i = i * 5 / 4)
		lengths.push_back(i);

	std::mt19937 gen;
	std::uniform_int_distribution<int> dist('a', 'z');
	std::string data(lengths.back(), '\0');
	for (auto& c : data)
		c = static_cast<char>(dist(gen));

	for (const auto len : lengths) {
		const std::string_view str(data.data(), len);
		const auto suffix = "/" + std::to_string(len);

		BENCHMARK("table" + suffix) {
			return fastgltf::fallback_crc32c(str);
		};
#if defined(FASTGLTF_IS_X86)
		BENCHMARK("hw" + suffix) {
			return fastgltf::sse_crc32c(str);
		};
#elif defined(FASTGLTF_ENABLE_ARMV8_CRC)
		BENCHMARK("hw" + suffix) {
			return fastgltf::armv8_crc32c(str);
		};
#endif
	}
}

TEST_CASE("Compare base64 decoding performance", "[!benchmark][base64-benchmark]") {
	std::string base64Characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	constexpr std::size_t bufferSize = 2 * 1024 * 1024;

	std::vector<std::size_t> lengths;
	for (std::size_t i = 4; i <= 128; i += 4)
		lengths.push_back(i);
	// 25% increments, aligned to 4 bytes
	for (std::size_t i = 160; i < bufferSize; i = (i + i / 4 + 4 - 1) / 4 * 4)
		lengths.push_back(i);
	lengths.push_back(bufferSize);

	// We'll generate a random base64 buffer
	std::random_device device;
	std::mt19937 gen(device());
	std::uniform_int_distribution<std::size_t> distribution(0, base64Characters.size() - 1);
	std::string generatedData;
	generatedData.reserve(bufferSize);
	for (std::size_t i = 0; i < bufferSize; ++i) {
		generatedData.push_back(base64Characters[distribution(gen)]);
	}

	for (const auto len : lengths) {
		const std::string str(generatedData.data(), len);
		const auto suffix = "/" + std::to_string(len);

#ifdef HAS_TINYGLTF
		BENCHMARK("tinygltf" + suffix) {
			return tinygltf::base64_decode(str);
		};
#endif

		const auto padding = fastgltf::base64::getPadding(str);
		const auto outputSize = fastgltf::base64::getDecodedSize(str.size(), padding);
		std::string output;
		output.resize(outputSize);

#ifdef HAS_CGLTF
		cgltf_options options {};
		BENCHMARK("cgltf" + suffix) {
			auto* outputData = output.data();
			return cgltf_load_buffer_base64(&options,
				str.size(), str.data(), reinterpret_cast<void**>(&outputData));
		};
#endif

#ifdef HAS_GLTFRS
		BENCHMARK("gltf-rs" + suffix) {
			auto slice = rust::Slice<const std::uint8_t>(
				reinterpret_cast<std::uint8_t*>(str.data()), str.size());
			return rust::gltf::run_base64(slice);
		};
#endif

#ifdef HAS_ASSIMP
		BENCHMARK("assimp" + suffix) {
			return Assimp::Base64::Decode(str);
		};
#endif

		BENCHMARK("simdutf" + suffix) {
			return fastgltf::base64::decode(str);
		};
	}
}
