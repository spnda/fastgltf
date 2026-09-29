import fastgltf;

#include <cstddef>
#include <cstdio>

int main() {
	fastgltf::Parser parser;

	constexpr char json[] = R"({"asset": {"version": "2.0"}})";
	auto data = fastgltf::GltfDataBuffer::FromBytes(reinterpret_cast<const std::byte*>(json), sizeof(json) - 1);
	if (data.error() != fastgltf::Error::None)
		return 1;

	auto asset = parser.loadGltfJson(data.get(), {});
	if (asset.error() != fastgltf::Error::None) {
		std::printf("%s\n", fastgltf::getErrorMessage(asset.error()).data());
		return 1;
	}
	return fastgltf::validate(asset.get()) == fastgltf::Error::None ? 0 : 1;
}
