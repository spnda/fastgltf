# fastgltf

A superfast SIMD powered glTF 2.0 parser written in C++17 with minimal dependencies. Unlike other
glTF parsers, it does not automatically load textures and load GLB binaries to allow the user to
optimise to their liking. It does, however, load embedded data and also decodes base64 encoded
buffers.

By utilising simdjson, this library can take advantage of SSE4, AVX2, AVX512, and ARM Neon.

## Features

fastgltf supports glTF 2.0:
- [x] glTF JSON files
- [x] GLB binary files
- [x] Scenes, nodes, meshes
- [x] Accessors, buffer views, buffers
- [x] Materials, textures, images, samplers
- [ ] Animations
- [ ] Skins
- [ ] Cameras
- [ ] Extra data

fastgltf supports a number of extensions:
- [x] KHR_texture_basisu
- [x] KHR_texture_transform
- [x] MSFT_texture_dds

## Usage

The project uses a fairly simple CMake 3.24 script, which you can simply add as a subdirectory.

```cpp
#include <fastgltf_parser.hpp>
#include <fastgltf_types.hpp>

void load(std::filesystem::path path) {
    // Creates a Parser instance. Optimally, you should reuse
    // this across loads, but don't use it across threads.
    fastgltf::Parser parser;

    // The JsonData class is designed for re-usability of the same JSON string. It contains
    // utility functions to load data from a std::filesystem::path or copy from an existing buffer.
    // Note that it has to outlive the process of every parsing function you call.
    auto data = std::make_unique<fastgltf::JsonData>(path);

    // This loads the glTF file into the gltf object and parses the JSON.
    auto gltf = parser.loadGLTF(data.get(), fastgltf::Options::None);

    // With this call to parseBuffers you parse all buffer objects in the JSON data. loadGLTF does
    // not fully serialise the JSON data, which allows you to selectively load based on your needs.
    // Note that there is a parse* function for every datatype stored in a glTF file, e.g.
    // buffers, bufferViews, accessors, meshes, cameras, ...
    if (gltf->parseBuffers() != fastgltf::Error::None) {
        // error
    }

    // You obtain the asset with this call. This can only be done once.
    std::unique_ptr<fastgltf::Asset> asset = gltf->getParsedAsset();
}
```

All the nodes, meshes, buffers, textures, ... can now be accessed through the `fastgltf::Asset`
type. References in between objects are done with a single `size_t`, which is used to index into
the various vectors in the asset.

## Performance

In the following chapter I'll show some graphs on how fastgltf compares to the two most used glTF
libraries, cgltf and tinygltf. I've disabled loading of images and buffers to only compare the
JSON parsing and serialization of the glTF data. I create these graphs using a spreadsheet that you
can find [here](https://docs.google.com/spreadsheets/d/1ocdHGoty-rF0N46ZlAlswzcPHVRsqG_tncy8paD3iMY/edit?usp=sharing).
These numbers were tested using Catch2's benchmark tool on a Ryzen 5800X with 32GB of RAM.

First of I compared the performance with embedded buffers that are encoded with base64. This uses
the [2CylinderEngine asset](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/2CylinderEngine)
which contains a 1.7MB embedded buffer. fastgltf includes a optimised base64 decoding algorithm
that can take advantage of SSE4 and AVX2. With this asset, fastlgtf is **2.1 times faster** than
cgltf and **7.3 times faster** than tinygltf using RapidJSON.

![](https://cdn.discordapp.com/attachments/1019965526434394173/1025554414884364348/Mean_time_parsing_2CylinderEngine_ms_5.png)

[Buggy.gltf](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Buggy) is another
excellent test subject, as it's a 15k line long JSON. This shows the raw serialization speed of
all the parsers. In this case fastgltf is **2.5 times faster** than tinygltf and **1.9** times faster
than cgltf.

![](https://cdn.discordapp.com/attachments/442748131898032138/1025556349465145405/Mean_time_parsing_Buggy.gltf_ms_2.png)

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

Libraries embedded in fastgltf:
- [simdjson](https://github.com/simdjson/simdjson): Licensed under Apache 2.0.

Libraries used in examples and tests:
- [Catch2](https://github.com/catchorg/Catch2): Licensed under BSL-1.0.
- [glad](https://github.com/Dav1dde/glad): Licensed under MIT.
- [glfw](https://github.com/glfw/glfw): Licensed under Zlib.
- [glm](https://github.com/g-truc/glm): Licensed under MIT.
