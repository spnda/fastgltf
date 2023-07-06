# fastgltf

**fastgltf** is a speed and usability focused glTF 2.0 parser written in modern C++17 with minimal dependencies.
It uses SIMD in various areas to decrease the time the application spends parsing and loading glTF data.
By taking advantage of modern C++17 (and optionally C++20) it also provides easy and safe access to the properties and data.

The parser supports the entirety of glTF 2.0 specification, including many extensions.
By default, fastgltf will only do the absolute minimum to work with a glTF model.
However, it brings many additional features to ease working with the data,
including accessor tools, the ability to directly write to mapped GPU buffers, and decomposing transform matrices.

## Why use fastgltf?

There are many other options for working with glTF in C and C++, including the two most popular libraries
[tinygltf](https://github.com/syoyo/tinygltf) and [cgltf](https://github.com/jkuhlmann/cgltf).
These have been around for years and support virtually everything you need, so why would you even switch?

This table includes a quick overview of a comparison of the general quality-of-life features of the popular
glTF libraries.

|                                  | cgltf | tinygltf | fastgltf |
|:---------------------------------|:-----:|:--------:|:--------:|
| glTF 2.0 reading                 |  ✔️   |    ✔️    |    ✔️    |
| glTF 2.0 writing                 |  ✔️   |    ✔️    |    ❌     |
| Extension support                |  ✔️   |   🟡¹    |    ✔️    |
| Image decoding (PNG, JPEG, ...)  |  ✔️   |    ✔️    |    ❌     |
| Built-in Draco decompression     |   ❌   |    ✔️    |    ❌     |
| Memory callbacks                 |  ✔️   |    ❌     |   🟡²    |
| Android asset functionality      |   ❌   |    ✔️    |    ✔️    |
| Accessor utilities               |  ✔️   |    ❌     |    ✔️    |
| Sparse accessor utilities        |  🟡³  |    ❌     |    ✔️    |
| Matrix accessor utilities        |  🟡³  |    ❌     |    ✔️    |
| Transform matrices decomposition |   ❌   |    ❌     |    ✔️    |

¹ tinygltf does provide the JSON structure for extension data, but leaves the deserialization for you to do.  
² fastgltf allows the user to allocate memory for buffers and images.
It does not provide any mechanism for controlling all the heap allocations the library performs.  
³ cgltf supports sparse accessors and matrix data only with some accessor functions, but not all.  

A more detailed list of all features fastgltf supports can be found [here](#features).
You can read more about the accessor utilities from fastgltf [here](#accessor-tools).

fastgltf follows C++'s concept of "you don't pay for what you don't use" by only doing the absolute minimum by default.
Without specifying any options, fastgltf will only parse the specified parts of the glTF JSON.
For buffers and images, fastgltf will by default only either give you buffers,
when the buffer or image data is embedded within the glTF, or just the plain old URIs.
Still, fastgltf offers various options that will let the library load buffers and images into memory,
which can be controlled with the memory map/unmap callbacks.
These can also be used for mapping GPU buffers so that fastgltf will write or decode base64 data directly into GPU memory.

By using modern C++ features, the code that reads data and properties from the glTF becomes simpler and vastly more descriptive,
which is a big aspect of guaranteeing code-correctness.
A big factor for this improvement is the use of types which enforce certain properties about the data, like e.g. `std::variant` or `std::optional`.
Compared with tinygltf, where, for example, optional values are simply represented by a boolean or a `-1` for indices, this is a big improvement.

The biggest difference, which may not be as relevant to everyone, is the drastic increase in deserialization speed.
In some cases, fastgltf is at least 2 times quicker than its competitors, while in others it can be as much as 20 times.
You can read more about fastgltf's performance in the [performance chapter](#performance).

## Usage

fastgltf is a pure C++17 library and only depends on simdjson.
By using the included CMake 3.11 script, simdjson is automatically downloaded while configuring by default.
The library is tested on GCC 9, GCC 10, Clang 12, and MSVC 14 (Visual Studio 2022) using CI.
fastgltf is also available from [vcpkg](https://github.com/microsoft/vcpkg) and [conan](https://conan.io/).

The following snippet illustrates how to use fastgltf to load a simple glTF file.

```cpp
#include <fastgltf_parser.hpp>
#include <fastgltf_types.hpp>

void load(std::filesystem::path path) {
    // Creates a Parser instance. Optimally, you should reuse this across loads, but don't use it
    // across threads. To enable extensions, you have to pass them into the parser's constructor.
    fastgltf::Parser parser;

    // The GltfDataBuffer class is designed for re-usability of the same JSON string. It contains
    // utility functions to load data from a std::filesystem::path, copy from an existing buffer,
    // or re-use an already existing allocation. Note that it has to outlive the process of every
    // parsing function you call.
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(path);

    // This loads the glTF file into the gltf object and parses the JSON. For GLB files, use
    // Parser::loadBinaryGLTF instead.
    auto asset = parser.loadGLTF(&data, path.parent_path(), fastgltf::Options::None);
    if (auto error = asset.error(); error != fastgltf::Error::None) {
        // Some error occurred while reading the buffer, parsing the JSON, or validating the data.
    }

    // The glTF 2.0 asset is now ready to be used. Simply call asset.get(), asset.get_if() or
    // asset-> to get a direct reference to the Asset class. You can then access the glTF data
    // structures, like, for example, with buffers:
    for (auto& buffer : asset->buffers) {
        // Process the buffers.
    }

    // Optionally, you can now also call the Parser::validate method. This will more strictly
    // enforce the glTF spec and is not needed most of the time, though I would certainly
    // recommend it in a development environment or when debugging to avoid mishaps.

    //  parser.validate(asset.get());
}
```

All the nodes, meshes, buffers, textures, ... can now be accessed through the `fastgltf::Asset` type.
References in between objects are done with a single `size_t`,
which is used to index into the various vectors in the asset.

Tests and examples are also available and can be built by enabling the respective options,
`FASTGLTF_ENABLE_TESTS` and `FASTGLTF_ENABLE_EXAMPLES`.
fastgltf will then require a few extra dependencies for the test framework, and OpenGL tools,
which have to be downloaded by running `fetch_test_deps.py`.

When you use `simdjson` yourself or want to target a specific option, you can set `FASTGLTF_DOWNLOAD_SIMDJSON` to `OFF`.
fastgltf will now not try to download simdjson and will instead only try to link to the `simdjson::simdjson` target.

It can sometimes also be useful to enable `FASTGLTF_USE_CUSTOM_SMALLVECTOR` (either through CMake or the preprocessor).
fastgltf comes with a custom `SmallVector`, which has the same API as a `std::vector` but can store up to N elements
(the exact amount is a hardcoded template parameter)
within the object itself to avoid possibly expensive allocations for only a few bytes.

## Accessor tools

fastgltf provides a utility header for working with accessors. The header contains various functions
and utilities for reading, copying, and converting accessor data. All of these tools also directly
support sparse accessors to help add support for these without having to understand how they work.
This header was written by [forenoonwatch](https://github.com/forenoonwatch) with the help of
[Eearslya](https://github.com/Eearslya) and me.

Loading the indices of a mesh primitive is as easy as this:

```cpp
fastgltf::Primitive& primitive = ...;

std::vector<std::uint32_t> indices;
if (primitive.indicesAccessor.has_value()) {
    auto& accessor = asset->accessors[primitive.indicesAccessor.value()];
    indices.resize(accessor.count);

    fastgltf::iterateAccessorWithIndex<std::uint32_t>(
            asset.get(), accessor, [&](std::uint32_t index, std::size_t idx) {
        indices[idx] = index;
    });
}
```

fastgltf can also directly convert data into other types such as `glm::vec3`.
For this to work, you have to define a specialization of `fastgltf::ElementTraits` with your type.
fastgltf already provides specializations for every glm type that a glTF accessor could provide data for,
which can be found in `fastgltf/glm_element_traits.hpp`.

```cpp
template<>
struct fastgltf::ElementTraits<glm::vec3> : fastgltf::ElementTraitsBase<glm::vec3, AccessorType::Vec3, float> {};
```

```cpp
fastgltf::Primitive& primitive = ...;

std::vector<Vertex> vertices;
auto& accessor = asset->accessors[primitive.findAttribute("POSITION").second];
vertices.resize(accessor.count);

fastgltf::iterateAccessorWithIndex<glm::vec3>(asset.get(), accessor, [&](glm::vec3&& position, std::size_t idx) {
    vertices[idx] = std::forward<glm::vec3>(position);
});
```

There's also an overload of `iterateAccessor` which returns an iterable type on which you can call `begin()` and `end()`.

```cpp
// Using iterators directly
auto iterable = fastgltf::iterateAccessor(asset.get(), accessor);
for (auto it = iterable.begin(); it != iterable.end(); ++it) {
    auto idx = std::distance(iterable.begin(), it);
    array[idx] = *it;
}

// Using a range-based for loop
// Note: elements can not be passed by reference and always need to be copied.
std::size_t idx = 0;
for (auto element : fastgltf::iterateAccessor(asset.get(), accessor)) {
    array[idx++] = element;
}
```

Note that, by default, these functions will only be able to load from buffers where the source is either a `sources::ByteView` or a `sources::Vector`.
For other data sources, you'll need to provide a functor similar to the already provided `DefaultBufferDataAdapter`.

In total, fastgltf provides these functions for working with accessors:
- `copyFromAccessor` which is essentially a glorified std::memcpy which respects byte stride and converts data.
- `iterateAccessor` which provides the callback function to handle every element within the accessor.
- `iterateAccessor` which provides a C++ iterator which can be used in for loops to iterate over the elements.
- `iterateAccessorWithIndex` which provides the callback function for every element and also provides the current index.
- `getAccessorElement` which allows you to get single elements from the accessor.

## Performance

[spreadsheet-link]: https://docs.google.com/spreadsheets/d/1ocdHGoty-rF0N46ZlAlswzcPHVRsqG_tncy8paD3iMY/edit?usp=sharing

In this chapter, I'll show some graphs on how fastgltf compares to the two most used glTF libraries, cgltf and tinygltf.
I've disabled loading of images and buffers to only compare the JSON parsing and deserialization of the glTF data.
I create these graphs using a spreadsheet that you can find [here][spreadsheet-link].
These numbers were benchmarked using Catch2's benchmark tool on a Ryzen 5800X (with AVX2) with 32GB of RAM using Clang 16,
as Clang showed a significant performance improvement over MSVC in every test.

First, I compared the performance with embedded buffers that are encoded with base64.
This uses the [2CylinderEngine asset](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/2CylinderEngine)
which contains a 1.7MB embedded buffer.
fastgltf includes an optimised base64 decoding algorithm that can take advantage of AVX2, SSE4, and ARM Neon.
With this asset, fastgltf is **20.56 times faster** than tinygltf using RapidJSON and **6.5 times faster** than cgltf.

[![](https://cdn.discordapp.com/attachments/442748131898032138/1088470860333060207/Mean_time_parsing_2CylinderEngine_ms_8.png)][spreadsheet-link]

[Amazon's Bistro](https://developer.nvidia.com/orca/amazon-lumberyard-bistro)
(converted to glTF 2.0 using Blender) is another excellent test subject, as it's a 148k line long JSON.
This shows the raw deserialization speed of all the parsers.
In this case fastgltf is **2.1 times faster** than tinygltf and **5.6 times faster** than cgltf.

[![](https://cdn.discordapp.com/attachments/442748131898032138/1088470983024840754/Bistro_load_from_memory_without_images_and_buffer_load_1.png)][spreadsheet-link]


## Features

fastgltf supports glTF 2.0:
- [x] glTF JSON files
- [x] GLB binary files
- [x] Scenes, nodes, meshes
- [x] Accessors, buffer views, buffers
- [x] Materials, textures, images, samplers
- [x] Animations
- [x] Skins
- [x] Cameras
- [x] Morph targets, sparse accessors
- [ ] Extra data

fastgltf supports a number of glTF extensions:
- [x] EXT_meshopt_compression
- [x] EXT_texture_webp
- [x] KHR_lights_punctual
- [x] KHR_materials_clearcoat
- [x] KHR_materials_emissive_strength
- [x] KHR_materials_ior
- [x] KHR_materials_iridescence
- [x] KHR_materials_sheen
- [x] KHR_materials_specular
- [x] KHR_materials_transmission
- [x] KHR_materials_unlit
- [x] KHR_materials_volume
- [x] KHR_texture_basisu
- [x] KHR_texture_transform
- [x] KHR_mesh_quantization
- [x] MSFT_texture_dds

fastgltf brings many utilities:
- [x] SIMD powered base64 buffer decoder
- [x] Can decompose transform matrices into independent translation, rotation, and scale factors
- [x] Allows writing directly to mapped GPU buffers for direct uploads, reducing copies
- [x] Advanced utilities to read, copy and iterate over (sparse) accessor data

## Acknowledgments

The major speedup comes from the awesome [`simdjson`](https://github.com/simdjson/simdjson) JSON library
which can use a multitude of different SIMD intrinsics to increase JSON decoding speed.
The library really shows off at massive JSON files, by parsing around 8GB/s.

The SIMD-based base64 decoding algorithms come from [Wojciech Muła](http://0x80.pl/notesen/2016-01-17-sse-base64-decoding.html#avx2-version),
which can usually nearly triple the decoding speed with AVX2.

Thanks to https://github.com/komrad36/CRC for some insight
into optimising the CRC32-C algorithm and using the SSE4 CRC instructions.

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
