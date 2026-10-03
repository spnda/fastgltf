#include <functional>

#include <fastgltf/uri.hpp>

#include "fuzz_helper.hpp"

namespace {
	// Checks that part is a substring of str
	bool inBounds(const std::string_view str, const std::string_view part) {
		if (part.empty())
			return true;

		// the comparison functors have strict total order, while pointer comparisons are only partial
		return std::less_equal()(str.data(), part.data()) &&
			std::less_equal()(part.data() + part.size(), str.data() + str.size());
	}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) {
	const std::string_view str(reinterpret_cast<const char*>(data), size);
	const fastgltf::URIView view(str);
	const fastgltf::URI uri(str);

	FUZZ_CHECK(inBounds(str, view.scheme()));
	FUZZ_CHECK(inBounds(str, view.userinfo()));
	FUZZ_CHECK(inBounds(str, view.host()));
	FUZZ_CHECK(inBounds(str, view.port()));
	FUZZ_CHECK(inBounds(str, view.path()));
	FUZZ_CHECK(inBounds(str, view.query()));
	FUZZ_CHECK(inBounds(str, view.fragment()));

	FUZZ_CHECK(inBounds(uri.string(), uri.scheme()));
	FUZZ_CHECK(inBounds(uri.string(), uri.userinfo()));
	FUZZ_CHECK(inBounds(uri.string(), uri.host()));
	FUZZ_CHECK(inBounds(uri.string(), uri.port()));
	FUZZ_CHECK(inBounds(uri.string(), uri.path()));
	FUZZ_CHECK(inBounds(uri.string(), uri.query()));
	FUZZ_CHECK(inBounds(uri.string(), uri.fragment()));

	FUZZ_CHECK(uri.valid() == view.valid());
	FUZZ_CHECK(uri.scheme() == view.scheme());
	FUZZ_CHECK(uri.userinfo() == view.userinfo());
	FUZZ_CHECK(uri.host() == view.host());
	FUZZ_CHECK(uri.port() == view.port());
	FUZZ_CHECK(uri.path() == view.path());
	FUZZ_CHECK(uri.query() == view.query());
	FUZZ_CHECK(uri.fragment() == view.fragment());

	FUZZ_CHECK(fastgltf::decodePercents(str).size() <= str.size());
	(void)view.fspath();
	return 0;
}
