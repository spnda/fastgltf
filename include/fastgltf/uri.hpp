/*
 * Copyright (C) 2022 - 2026 Sean Apeler
 * This file is part of fastgltf <https://github.com/spnda/fastgltf>.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef FASTGLTF_URI_HPP
#define FASTGLTF_URI_HPP

#if !defined(FASTGLTF_MODULE)
#include <filesystem>
#include <string>
#include <string_view>
#endif

#include <fastgltf/util.hpp>

namespace fastgltf {
	FASTGLTF_EXPORT class URI;

	namespace internal {
		struct URIRange {
			std::size_t pos = 0;
			std::size_t len = 0;
		};

		struct URIComponents {
			URIRange _scheme;
			URIRange _path;

			URIRange _userinfo;
			URIRange _host;
			URIRange _port;

			URIRange _query;
			URIRange _fragment;

			bool _valid = true;
		};

		[[nodiscard]] inline auto get(const std::string_view view, const URIRange range) noexcept {
			return view.substr(range.pos, range.len);
		}

		[[nodiscard]] URIComponents parseURI(std::string_view str) noexcept;
	}

	std::string decodePercents(std::string_view x);

	/**
	 * Custom URI class for fastgltf's needs. glTF 2.0 only allows two types of URIs:
	 *  (1) Data URIs as specified in RFC 2397.
	 *  (2) Relative paths as specified in RFC 3986.
	 *
	 * See https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#uris for details.
	 * However, the glTF spec allows more broader URIs in client implementations. Therefore,
	 * this supports all types of URIs as defined in RFC 3986.
	 */
	FASTGLTF_EXPORT class URIView {
		friend class URI;

		std::string_view _view;
		internal::URIComponents _components;

		// Used by URI to reuse the parsed component ranges
		URIView(const std::string_view str, const internal::URIComponents& components) noexcept
			: _view(str), _components(components) {}

	public:
		explicit URIView() noexcept = default;
		explicit URIView(std::string_view uri) noexcept;
		URIView(const URIView& other) noexcept = default;

		URIView& operator=(const std::string_view other) noexcept {
			return *this = URIView(other);
		}
		URIView& operator=(const URIView& other) = default;

		[[nodiscard]] auto string() const noexcept {
			return _view;
		}

		[[nodiscard]] auto scheme() const noexcept {
			return internal::get(_view, _components._scheme);
		}
		[[nodiscard]] auto userinfo() const noexcept {
			return internal::get(_view, _components._userinfo);
		}
		[[nodiscard]] auto host() const noexcept {
			return internal::get(_view, _components._host);
		}
		[[nodiscard]] auto port() const noexcept {
			return internal::get(_view, _components._port);
		}
		[[nodiscard]] auto path() const noexcept {
			return internal::get(_view, _components._path);
		}
		[[nodiscard]] auto query() const noexcept {
			return internal::get(_view, _components._query);
		}
		[[nodiscard]] auto fragment() const noexcept {
			return internal::get(_view, _components._fragment);
		}

		[[nodiscard]] auto fspath() const -> std::filesystem::path;
		[[nodiscard]] bool valid() const noexcept {
			return _components._valid;
		}
		[[nodiscard]] bool isLocalPath() const noexcept {
			return scheme().empty() || (scheme() == "file" && host().empty());
		}
		[[nodiscard]] bool isDataUri() const noexcept {
			return scheme() == "data";
		}
	};

	/**
	 * Custom URI class for fastgltf's needs. glTF 2.0 only allows two types of URIs:
	 *  (1) Data URIs as specified in RFC 2397.
	 *  (2) Relative paths as specified in RFC 3986.
	 *
	 * See https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#uris for details.
	 * However, the glTF spec allows more broader URIs in client implementations. Therefore,
	 * this supports all types of URIs as defined in RFC 3986.
	 */
	class URI {
		std::string _uri;
		internal::URIComponents _components;

	public:
		explicit URI() noexcept = default;

		explicit URI(std::string uri) noexcept;
		explicit URI(std::string_view uri) noexcept;
		explicit URI(const URIView& view) noexcept;

		URI(const URI& other) = default;
		URI(URI&& other) noexcept = default;

		URI& operator=(const URI& other) = default;
		URI& operator=(const URIView& other);
		URI& operator=(URI&& other) noexcept = default;;

		operator URIView() const noexcept;

		[[nodiscard]] auto string() const noexcept {
			return std::string_view(_uri);
		}

		[[nodiscard]] auto scheme() const noexcept {
			return internal::get(_uri, _components._scheme);
		}
		[[nodiscard]] auto userinfo() const noexcept {
			return internal::get(_uri, _components._userinfo);
		}
		[[nodiscard]] auto host() const noexcept {
			return internal::get(_uri, _components._host);
		}
		[[nodiscard]] auto port() const noexcept {
			return internal::get(_uri, _components._port);
		}
		[[nodiscard]] auto path() const noexcept {
			return internal::get(_uri, _components._path);
		}
		[[nodiscard]] auto query() const noexcept {
			return internal::get(_uri, _components._query);
		}
		[[nodiscard]] auto fragment() const noexcept {
			return internal::get(_uri, _components._fragment);
		}

		[[nodiscard]] auto fspath() const -> std::filesystem::path;
		[[nodiscard]] bool valid() const noexcept {
			return _components._valid;
		}
		[[nodiscard]] bool isLocalPath() const noexcept {
			return scheme().empty() || (scheme() == "file" && host().empty());
		}
		[[nodiscard]] bool isDataUri() const noexcept {
			return scheme() == "data";
		}
	};
} // namespace fastgltf

#endif
