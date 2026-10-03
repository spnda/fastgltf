#include <cstdlib>
#include <string_view>

#include <fastgltf/base64.hpp>

#include "fuzz_helper.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) {
	const std::string_view str(reinterpret_cast<const char*>(data), size);
	auto fallback_decoded = fastgltf::base64::fallback_decode(str);

#if defined(FASTGLTF_IS_X86)
	auto sse_decoded = fastgltf::base64::sse4_decode(str);
	FUZZ_CHECK(std::ranges::equal(fallback_decoded, sse_decoded));
	auto avx_decoded = fastgltf::base64::avx2_decode(str);
	FUZZ_CHECK(std::ranges::equal(fallback_decoded, avx_decoded));
#elif FASTGLTF_ENABLE_NEON_BASE64
	auto neon_decoded = fastgltf::base64::neon_decode(str);
	FUZZ_CHECK(std::ranges::equal(fallback_decoded, neon_decoded));
#else
	(void)fallback_decoded;
#endif
	return 0;
}
