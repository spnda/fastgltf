#include <array>
#include <cctype>
#include <cstdlib>

#include <fastgltf/uri.hpp>

namespace fg = fastgltf;
namespace fs = std::filesystem;

namespace fastgltf::internal {
	[[nodiscard]] static constexpr bool isAlpha(const char c) noexcept {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}
	[[nodiscard]] static constexpr bool isSchemeChar(const char c) noexcept {
		return isAlpha(c) || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
	}

	URIComponents parseURI(const std::string_view str) noexcept {
		URIComponents c;
		if (str.empty()) {
			c._valid = false;
			return c;
		}

		std::size_t idx = 0;

		// Check the URI scheme defined as ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
		if (isAlpha(str[0])) {
			std::size_t i = 1;
			while (i < str.size() && isSchemeChar(str[i])) {
				++i;
			}
			if (i < str.size() && str[i] == ':') {
				c._scheme = { .pos = 0, .len = i };
				idx = i + 1;
			}
		}

		if (startsWith(str.substr(idx), "//")) {
			// URI has an authority part.
			idx += 2;
			auto authEnd = str.find_first_of("/?#", idx);
			if (authEnd == std::string_view::npos)
				authEnd = str.size();
			const auto authority = str.substr(idx, authEnd - idx);

			if (const auto at = authority.rfind('@');
				at != std::string_view::npos) {
				c._userinfo = { .pos = idx, .len = at };
				idx += at + 1;
			}

			auto hostEnd = authEnd;
			if (idx < authEnd && str[idx] == '[') {
				const auto bracket = str.find_first_of(']', idx);
				if (bracket == std::string_view::npos || bracket >= authEnd) {
					c._valid = false;
					return c;
				}
				hostEnd = bracket + 1;
				if (hostEnd != authEnd && str[hostEnd] != ':') {
					c._valid = false;
					return c;
				}
			} else if (const auto colon = str.find_first_of(':', idx); colon < authEnd) {
				hostEnd = colon;
			}

			c._host = { .pos = idx, .len = hostEnd - idx };
			if (hostEnd < authEnd)
				c._port = { .pos = hostEnd + 1, .len = authEnd - hostEnd - 1 };

			idx = authEnd;
		}

		if (get(str, c._scheme) == "data") {
			// The data scheme is just followed by a mime and then bytes.
			// Also, let's avoid all the find and substr on very large data strings
			// which can be multiple MB.
			c._path = { .pos = idx, .len = str.size() - idx };
		} else if (idx == str.size()) {
			c._path = { .pos = idx, .len = 0 };
		} else {
			// Parse the path.
			auto questionIdx = str.find('?', idx);
			auto hashIdx = str.find('#', idx);
			if (questionIdx != std::string_view::npos) {
				c._path = { .pos = idx, .len = questionIdx - idx };

				if (hashIdx == std::string_view::npos) {
					++questionIdx;
					c._query = { .pos = questionIdx, .len = str.size() - questionIdx };
				} else {
					++questionIdx;
					c._query = { .pos = questionIdx, .len = hashIdx - questionIdx };
					++hashIdx;
					c._fragment = { .pos = hashIdx, .len = str.size() - hashIdx };
				}
			} else if (hashIdx != std::string_view::npos) {
				c._path = { .pos = idx, .len = hashIdx - idx };
				++hashIdx;
				c._fragment = { .pos = hashIdx, .len = str.size() - hashIdx };
			} else {
				c._path = { .pos = idx, .len = str.size() - idx };
			}
		}

		c._valid = true;
		return c;
	}
}

std::string fg::decodePercents(const std::string_view x) {
	auto ret = std::string(x);
	for (std::size_t i = 0; i < ret.size(); ++i) {
		if (ret[i] != '%')
			continue;

		if (i + 2 >= ret.size() || !std::isxdigit(static_cast<unsigned char>(ret[i + 1])) ||
			!std::isxdigit(static_cast<unsigned char>(ret[i + 2])))
			continue;

		// Read the next two chars and store them
		std::array<char, 3> chars = { ret[i + 1], ret[i + 2] };
		ret[i] = static_cast<char>(std::strtoul(chars.data(), nullptr, 16));
		ret.erase(i + 1, 2);
	}
	return ret;
}

fg::URIView::URIView(const std::string_view uri) noexcept : _view(uri), _components(internal::parseURI(_view)) {}

fs::path fg::URIView::fspath() const {
	if (!isLocalPath())
		return {};

	const auto raw = path();
	const auto u8 = [](const std::string_view s) {
		return std::u8string_view(reinterpret_cast<const char8_t*>(s.data()), s.size());
	};

	// there's no percent in the path, no need to decode and potentially allocate twice
	if (raw.find('%') == std::string_view::npos)
		return u8(raw);

	if constexpr (std::is_same_v<fs::path::value_type, std::string::value_type>) {
		std::string decodedPath = decodePercents(raw);
		return std::move(decodedPath);
	} else {
		const auto str = decodePercents(raw);
		return u8(str);
	}
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
