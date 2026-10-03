#include <cstdlib>
#include <span>

#include <fastgltf/base64.hpp>

#include "fuzz_helper.hpp"

namespace {
	template <typename DecodeFunc>
	void encodeDecode(const std::span<const std::uint8_t> data, DecodeFunc* decode) {
		const std::string encoded = fastgltf::base64::encode(data.data(), data.size());
		const fastgltf::static_vector decoded = decode(encoded);
		FUZZ_CHECK(std::ranges::equal(data, decoded));
	}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) {
	encodeDecode({ data, size }, fastgltf::base64::fallback_decode);

#if defined(FASTGLTF_IS_X86)
	encodeDecode({ data, size }, fastgltf::base64::sse4_decode);
	encodeDecode({ data, size }, fastgltf::base64::avx2_decode);
#elif FASTGLTF_ENABLE_NEON_BASE64
	encodeDecode({ data, size }, fastgltf::base64::neon_decode);
#endif
	return 0;
}
