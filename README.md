# fastgltf

A superfast SIMD powered glTF 2.0 parser written in C++17 with minimal dependencies. Unlike other
glTF parsers, it does not automatically load textures and load GLB binaries to allow the user to
optimise to their liking. It does, however, load embedded data and also decodes base64 encoded
buffers.

By utilising simdjson, this library can take advantage of SSE4, AVX2, AVX512, and ARM Neon.

## Usage

The project uses a fairly simple CMake 3.10 script, which you can simply add as a subdirectory.

```cpp
#include <fastgltf_parser.hpp>
#include <fastgltf_types.hpp>

void load(std::filesystem::path path) {
    // Creates a Parser instance. Optimally, you should reuse
    // this across loads, but don't use it across threads.
    fastgltf::Parser parser;
    
    // This loads the glTF file.
    parser.loadGLTF(path, fastgltf::Options::None);
    
    // You obtain the asset with this call. This can only be done once.
    std::unique_ptr<fastgltf::Asset> asset = parser.getParsedAsset();
}
```

All the nodes, meshes, buffers, textures, ... can now be accessed through the `fastgltf::Asset`
type. References in between objects are done with a single `size_t`, which is used to index into
the various vectors in the asset.

## Performance

The following shows a graph comparing the parse and load times of the embedded version of the
[2CylinderEngine asset](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/2CylinderEngine).
The times are measured using the Catch2 frameworks' benchmark tools. This is a good example, as the
model uses a very big base64 encoded data buffer for which fastgltf provides an AVX2 decoder.
fastgltf turns out to be over **5.4x faster** than tinygltf, and **1.46x faster** than cgltf.

![](https://cdn.discordapp.com/attachments/442748131898032138/1018975326384160838/Mean_time_parsing_2CylinderEngine_ms_1.png)

## Acknowledgments

The major speedup comes from the awesome [`simdjson`](https://github.com/simdjson/simdjson) JSON
library which can use a multitude of different SIMD intrinsics to increase JSON decoding speed. The
library really shows off at massive JSON files, by parsing around 8GB/s.

The SIMD-based base64 decoding algorithms come from
[Wojciech Muła](http://0x80.pl/notesen/2016-01-17-sse-base64-decoding.html#avx2-version), which can
usually nearly triple the decoding speed with AVX2.

## License

The **fastgltf** library is licensed under the MIT License.

----

Third party licenses:
- [simdjson](https://github.com/simdjson/simdjson): Licensed under Apache 2.0.
