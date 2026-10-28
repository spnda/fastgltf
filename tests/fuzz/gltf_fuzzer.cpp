#include <cstdint>

#include <fastgltf/core.hpp>

#include "fuzz_helper.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) {
	auto buffer = fastgltf::GltfDataBuffer::FromBytes(
		reinterpret_cast<const std::byte*>(data), size);
	FUZZ_CHECK(buffer.has_value()); // This should not fail unless OOM

	constexpr auto fuzzable_options =
		fastgltf::Options::AllowDouble |
		fastgltf::Options::DecomposeNodeMatrices |
		fastgltf::Options::GenerateMeshIndices;
	constexpr auto all_extensions = ~fastgltf::Extensions::None;
	auto asset = fastgltf::Parser(all_extensions).loadGltf(buffer.get(), {}, fuzzable_options);
	if (asset.has_error())
		return 0;

	auto exported = fastgltf::Exporter().writeGltfJson(asset.get(), fastgltf::ExportOptions::ValidateAsset);
	if (exported.has_error())
		return 0;

	auto exported_data = fastgltf::GltfDataBuffer::FromBytes(
		reinterpret_cast<const std::byte*>(exported->output.data()), exported->output.size());
	auto reparsed = fastgltf::Parser(all_extensions).loadGltfJson(exported_data.get(), {}, fuzzable_options);
	FUZZ_CHECK(reparsed.has_value()); // Otherwise the parser, exporter, or validator should've failed.

	return 0;
}
