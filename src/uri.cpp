#include <fastgltf/uri.hpp>

namespace fg = fastgltf;
namespace fs = std::filesystem;

namespace fastgltf::internal {
	URIComponents parseURI(const std::string_view str) noexcept {
		URIComponents c;
		if (str.empty()) {
			c._valid = false;
			return c;
		}

		std::size_t idx = 0;
		auto firstColon = str.find(':');
		if (firstColon != std::string::npos) {
			// URI has a scheme.
			if (firstColon == 0) {
				// Empty scheme is invalid
				c._valid = false;
				return c;
			}
			c._scheme = { .pos = 0, .len = firstColon };
			idx = firstColon + 1;
		}

		if (startsWith(str.substr(idx), "//")) {
			// URI has an authority part.
			idx += 2;
			auto nextSlash = str.find('/', idx);
			auto userInfo = str.find('@', idx);
			if (userInfo != std::string::npos && userInfo < nextSlash) {
				c._userinfo = { .pos = idx, .len = userInfo - idx };
				idx += c._userinfo.len + 1;
			}

			auto hostEnd = nextSlash - 1;
			std::size_t portColon;
			if (str[idx] == '[') {
				hostEnd = str.find(']', idx);
				if (hostEnd == std::string::npos) {
					c._valid = false;
					return c;
				}
				// IPv6 addresses are made up of colons, so we need to search after its address.
				// This will just be hostEnd + 1 or std::string::npos.
				portColon = str.find(':', hostEnd);
			} else {
				portColon = str.find(':', idx);
			}

			if (portColon != std::string::npos) {
				c._host = { .pos = idx, .len = portColon - idx };
				++portColon; // We don't want to include the colon in the port string.
				c._port = { .pos = portColon, .len = nextSlash - portColon };
			} else {
				++idx;
				c._host = { .pos = idx, .len = hostEnd - idx };
			}

			idx = nextSlash; // Path includes this slash
		}

		if (get(str, c._scheme) == "data") {
			// The data scheme is just followed by a mime and then bytes.
			// Also, let's avoid all the find and substr on very large data strings
			// which can be multiple MB.
			c._path = { .pos = idx, .len = str.size() - idx };
		} else {
			// Parse the path.
			auto questionIdx = str.find('?', idx);
			auto hashIdx = str.find('#', idx);
			if (questionIdx != std::string::npos) {
				c._path = { .pos = idx, .len = questionIdx - idx };

				if (hashIdx == std::string::npos) {
					++questionIdx;
					c._query = { .pos = questionIdx, .len = str.size() - questionIdx };
				} else {
					++questionIdx;
					c._query = { .pos = questionIdx, .len = hashIdx - questionIdx };
					++hashIdx;
					c._fragment = { .pos = hashIdx, .len = str.size() - hashIdx };
				}
			} else if (hashIdx != std::string::npos) {
				c._path = { .pos = idx, .len = hashIdx - idx };
				++hashIdx;
				c._fragment = { .pos = hashIdx, .len = hashIdx - idx };
			} else {
				c._path = { .pos = idx, .len = str.size() - idx };
			}
		}

		return c;
	}
}

std::string fg::decodePercents(const std::string_view x) {
	auto ret = std::string(x);
	for (std::size_t i = 0; i < x.size(); ++i) {
		if (x[i] != '%')
			continue;

		// Read the next two chars and store them
		std::array<char, 3> chars = {x[i + 1], x[i + 2]};
		ret[i] = static_cast<char>(std::strtoul(chars.data(), nullptr, 16));
		ret.erase(i + 1, 2);
	}
	return ret;
}

fg::URIView::URIView(const std::string_view uri) noexcept : _view(uri), _components(internal::parseURI(_view)) {}

fs::path fg::URIView::fspath() const {
	if (!isLocalPath())
		return {};

	std::string decodedPath = decodePercents(path());
	return std::move(decodedPath);
}

fg::URI::URI(std::string uri) noexcept : _uri(std::move(uri)), _components(internal::parseURI(_uri)) {}

fg::URI::URI(const std::string_view uri) noexcept : _uri(uri), _components(internal::parseURI(_uri)) {}

fg::URI::URI(const URIView& view) noexcept : _uri(view._view), _components(view._components) {}


fg::URI& fg::URI::operator=(const URIView& other) {
	_uri = other._view;
	_components = other._components;
	return *this;
}

fg::URI::operator fg::URIView() const noexcept {
	return { _uri, _components };
}

fs::path fg::URI::fspath() const {
	if (!isLocalPath())
		return {};

	std::string decodedPath = decodePercents(path());
	return std::move(decodedPath);
}
