#include <cstdint>

#include <fastgltf/parser.hpp>

#include "fuzz_helper.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) {
	auto buffer = fastgltf::GltfDataBuffer::FromBytes(
		reinterpret_cast<const std::byte*>(data), size);
	FUZZ_CHECK(buffer.has_value()); // This should not fail unless OOM

	auto asset = fastgltf::Parser().loadGltfJson(buffer.get(), {});
	(void)asset;
	return 0;
}
