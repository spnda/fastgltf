#include <catch2/catch_test_macros.hpp>

#include <fastgltf/types.hpp>
#include <fastgltf/core.hpp>

#include "gltf_path.hpp"

TEST_CASE("Simple URIs", "[uri-tests]") {
	SECTION("Empty") {
		const fastgltf::URI uri(std::string_view(""));
		REQUIRE(uri.scheme().empty());
		REQUIRE(uri.path().empty());
	}

	SECTION("Local paths") {
		std::string_view relpath = "path/somewhere.xyz";
		SECTION("Basic local path") {
			const fastgltf::URI uri(relpath);
			REQUIRE(uri.scheme().empty());
			REQUIRE(uri.path() == relpath);
			REQUIRE(uri.isLocalPath());
			REQUIRE(uri.fspath() == relpath);
		}

		std::string_view abspath = "/path/somewhere.xyz";
		SECTION("File scheme path") {
			const std::string_view filePath = "file:/path/somewhere.xyz";
			const fastgltf::URI uri(filePath);
			REQUIRE(uri.scheme() == "file");
			REQUIRE(uri.isLocalPath());
			REQUIRE(uri.path() == abspath);
		}

		SECTION("File scheme localhost path") {
			const std::string_view localhostPath = "file://localhost/path/somewhere.xyz";
			const fastgltf::URI uri(localhostPath);
			REQUIRE(uri.scheme() == "file");
			REQUIRE(uri.host() == "localhost");
			REQUIRE(uri.path() == abspath);
			REQUIRE(!uri.isLocalPath());
		}

		SECTION("Empty authority") {
			const fastgltf::URI uri(std::string_view("file:///"));
			REQUIRE(uri.scheme() == "file");
			REQUIRE(uri.isLocalPath());
			REQUIRE(uri.path() == "/");
		}
	}

	SECTION("Simple remote URI") {
		SECTION("Basic") {
			const std::string_view url = "https://example.com";
			const fastgltf::URI uri(url);
			REQUIRE(uri.scheme() == "https");
			REQUIRE(uri.host() == "example.com");
			REQUIRE(uri.path().empty());
			REQUIRE(!uri.isLocalPath());
		}

		SECTION("With path") {
			const std::string_view url = "https://example.com/path/somewhere";
			const fastgltf::URI uri(url);
			REQUIRE(uri.scheme() == "https");
			REQUIRE(uri.host() == "example.com");
			REQUIRE(uri.path() == "/path/somewhere");
			REQUIRE(!uri.isLocalPath());
		}

		SECTION("Ports") {
			const fastgltf::URI uri(std::string_view("https://host:80"));
			REQUIRE(uri.scheme() == "https");
			REQUIRE(uri.host() == "host");
			REQUIRE(uri.port() == "80");
		}

		SECTION("Basic userinfo") {
			const fastgltf::URI uri(std::string_view("https://user@host"));
			REQUIRE(uri.scheme() == "https");
			REQUIRE(uri.userinfo() == "user");
			REQUIRE(uri.host() == "host");
		}

		SECTION("Long userinfo") {
			const fastgltf::URI uri(std::string_view("https://verylongusername@host"));
			REQUIRE(uri.scheme() == "https");
			REQUIRE(uri.userinfo() == "verylongusername");
			REQUIRE(uri.host() == "host");
		}

		SECTION("Basic query") {
			const fastgltf::URI uri(std::string_view("https://host?q=1"));
			REQUIRE(uri.scheme() == "https");
			REQUIRE(uri.host() == "host");
			REQUIRE(uri.query() == "q=1");
		}

		SECTION("Paths with colons") {
			const fastgltf::URI uri(std::string_view("https://host/a:b"));
			REQUIRE(uri.scheme() == "https");
			REQUIRE(uri.host() == "host");
			REQUIRE(uri.path() == "/a:b");
		}
	}
}

TEST_CASE("Test generic URIs", "[uri-tests]") {
	// These are a bunch of example URIs from https://en.wikipedia.org/wiki/Uniform_Resource_Identifier#Example_URIs
	SECTION("Example 1") {
		const fastgltf::URI uri(std::string_view("https://john.doe@www.example.com:1234/forum/questions/?tag=networking&order=newest#top"));
		REQUIRE(uri.scheme() == "https");
		REQUIRE(uri.userinfo() == "john.doe");
		REQUIRE(uri.host() == "www.example.com");
		REQUIRE(uri.port() == "1234");
		REQUIRE(uri.path() == "/forum/questions/");
		REQUIRE(uri.query() == "tag=networking&order=newest");
		REQUIRE(uri.fragment() == "top");
	}

	SECTION("Example 2") {
		const fastgltf::URI uri(std::string_view(
			"https://john.doe@www.example.com:1234/forum/questions/?tag=networking&order=newest#:~:text=whatever"));
		REQUIRE(uri.scheme() == "https");
		REQUIRE(uri.userinfo() == "john.doe");
		REQUIRE(uri.host() == "www.example.com");
		REQUIRE(uri.port() == "1234");
		REQUIRE(uri.path() == "/forum/questions/");
		REQUIRE(uri.query() == "tag=networking&order=newest");
		REQUIRE(uri.fragment() == ":~:text=whatever");
	}

	SECTION("Example 3") {
		const fastgltf::URI uri(std::string_view("ldap://[2001:db8::7]/c=GB?objectClass?one"));
		REQUIRE(uri.scheme() == "ldap");
		REQUIRE(uri.host() == "[2001:db8::7]");
		REQUIRE(uri.path() == "/c=GB");
		REQUIRE(uri.query() == "objectClass?one");
	}

	SECTION("Example 4") {
		const fastgltf::URI uri(std::string_view("mailto:John.Doe@example.com"));
		REQUIRE(uri.scheme() == "mailto");
		REQUIRE(uri.path() == "John.Doe@example.com");
	}

	SECTION("Example 5") {
		const fastgltf::URI uri(std::string_view("news:comp.infosystems.www.servers.unix"));
		REQUIRE(uri.scheme() == "news");
		REQUIRE(uri.path() == "comp.infosystems.www.servers.unix");
	}

	SECTION("Example 6") {
		const fastgltf::URI uri(std::string_view("tel:+1-816-555-1212"));
		REQUIRE(uri.scheme() == "tel");
		REQUIRE(uri.path() == "+1-816-555-1212");
	}

	SECTION("Example 7") {
		const fastgltf::URI uri(std::string_view("telnet://192.0.2.16:80/"));
		REQUIRE(uri.scheme() == "telnet");
		REQUIRE(uri.host() == "192.0.2.16");
		REQUIRE(uri.port() == "80");
		REQUIRE(uri.path() == "/");
	}

	SECTION("Example 8") {
		const fastgltf::URI uri(std::string_view("urn:oasis:names:specification:docbook:dtd:xml:4.1.2"));
		REQUIRE(uri.scheme() == "urn");
		REQUIRE(uri.path() == "oasis:names:specification:docbook:dtd:xml:4.1.2");
	}
}

TEST_CASE("Percent decoding", "[uri-tests]") {
	SECTION("Decode percent characters") {
		// All reserved characters as per RFC 3986 section 2.2 Reserved Characters (January 2005)
		const std::string_view input = "%20%21%22%23%24%25%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D";
		const auto decoded = fastgltf::decodePercents(input);
		REQUIRE(decoded == " !\"#$%&'()*+,/:;=?@[]");
	}

	SECTION("Decoding behavior") {
		const std::string_view path = "a%23%3Ab.png";
		const fastgltf::URI uri(path);
		REQUIRE(uri.path() == "a%23%3Ab.png");
		REQUIRE(uri.fspath() == "a#:b.png");
	}

	SECTION("Only decode valid characters") {
		const std::string_view input = "100%test.png";
		REQUIRE(fastgltf::decodePercents(input) == input);
	}

	SECTION("Don't out of bounds") {
		const std::string_view input = "100%";
		REQUIRE(fastgltf::decodePercents(input) == input);
	}
}

TEST_CASE("Data URI parsing", "[uri-tests]") {
	// This example base64 data is from an example on https://en.wikipedia.org/wiki/Data_URI_scheme.
	const std::string_view data = "data:image/png;base64,iVBORw0KGgoAAA"
							"ANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4"
							"//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU"
							"5ErkJggg==";
	const fastgltf::URI uri(data);
	REQUIRE(uri.scheme() == "data");
	REQUIRE(uri.path() == data.substr(5));
}

TEST_CASE("URI copy and move behavior", "[uri-tests]") {
	const std::string_view data = "test.bin";
	SECTION("Copy semantics") {
		fastgltf::URI uri(data);
		REQUIRE(uri.path() == data);
		fastgltf::URI uri2(uri);
		REQUIRE(uri2.string().data() != uri.string().data());
		REQUIRE(uri2.path() == data);
	}

	SECTION("Move semantics") {
		fastgltf::URI uri;
		{
			fastgltf::URI uri2(data);
			uri = std::move(uri2);
			REQUIRE(uri2.string().empty());
		}
		// Test that the values were copied over and that the string views are still valid.
		REQUIRE(uri.string() == data);
		REQUIRE(uri.path() == uri.string());
	}
}

TEST_CASE("Validate escaped/percent-encoded URI", "[uri-tests]") {
	const std::string_view gltfString = R"({"images": [{"uri": "grande_sph\u00E8re.png"}]})";
	auto dataBuffer = fastgltf::GltfDataBuffer::FromBytes(
			reinterpret_cast<const std::byte*>(gltfString.data()),
			gltfString.size());
	REQUIRE(dataBuffer.error() == fastgltf::Error::None);

	fastgltf::Parser parser;
	auto asset = parser.loadGltfJson(dataBuffer.get(), "", fastgltf::Options::DontRequireValidAssetMember);
	REQUIRE(asset.error() == fastgltf::Error::None);

	REQUIRE(asset->images.size() == 1);
	auto escaped = std::get<fastgltf::sources::URI>(asset->images.front().data);

	// This only tests wether the default ctor of fastgltf::URI can handle percent-encoding correctly.
	const fastgltf::URI original(std::string_view("grande_sphère.png"));
	const fastgltf::URI encoded(std::string_view("grande_sph%C3%A8re.png"));
	REQUIRE(original.fspath() == escaped.uri.fspath());
	REQUIRE(original.fspath() == encoded.fspath());
}

TEST_CASE("Test percent-encoded URIs in glTF", "[uri-tests]") {
	auto boxWithSpaces = sampleAssets / "Models" / "Box With Spaces" / "glTF";

	fastgltf::GltfFileStream jsonData(boxWithSpaces / "Box With Spaces.gltf");
	REQUIRE(jsonData.isOpen());

	fastgltf::Parser parser;
	auto asset = parser.loadGltfJson(jsonData, boxWithSpaces);
	REQUIRE(asset.error() == fastgltf::Error::None);
	REQUIRE(fastgltf::validate(asset.get()) == fastgltf::Error::None);

	REQUIRE(asset->images.size() == 3);

	auto* image0 = std::get_if<fastgltf::sources::URI>(&asset->images[0].data);
	REQUIRE(image0 != nullptr);
	REQUIRE(image0->uri.fspath() == "Normal Map.png");

	auto* image1 = std::get_if<fastgltf::sources::URI>(&asset->images[1].data);
	REQUIRE(image1 != nullptr);
	REQUIRE(image1->uri.fspath() == "glTF Logo With Spaces.png");

	auto* image2 = std::get_if<fastgltf::sources::URI>(&asset->images[2].data);
	REQUIRE(image2 != nullptr);
	REQUIRE(image2->uri.fspath() == "Roughness Metallic.png");

	auto* buffer0 = std::get_if<fastgltf::sources::URI>(&asset->buffers[0].data);
	REQUIRE(buffer0 != nullptr);
	REQUIRE(buffer0->uri.fspath() == "Box With Spaces.bin");
}
