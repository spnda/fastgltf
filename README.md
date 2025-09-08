# fastgltf

![vcpkg](https://img.shields.io/vcpkg/v/fastgltf?style=flat-square)
![conan center](https://img.shields.io/conan/v/fastgltf?style=flat-square)
![CI_x64 workflow status](https://img.shields.io/github/actions/workflow/status/spnda/fastgltf/ci_x64.yml?label=CI%20x64&style=flat-square)
![CI_arm workflow status](https://img.shields.io/github/actions/workflow/status/spnda/fastgltf/ci_arm.yml?label=CI%20ARM&style=flat-square)
[![Documentation Status](https://readthedocs.org/projects/fastgltf/badge/?version=latest)](https://fastgltf.readthedocs.io/latest/?badge=latest)


**fastgltf** is a speed and usability focused glTF 2.0 library written in modern C++20 with minimal dependencies.
It uses SIMD in various areas to decrease the time the application spends parsing and loading glTF data.
By taking advantage of modern C++20 it also provides easy and safe access to the properties and data.
It is also available as a C++20 [named module](https://en.cppreference.com/w/cpp/language/modules).

The library supports the entirety of glTF 2.0 specification, including many extensions.
By default, fastgltf will only do the absolute minimum to work with a glTF model.
However, it brings many additional features to ease working with the data,
including accessor tools, the ability to directly write to mapped GPU buffers, and decomposing transform matrices.

To learn more about fastgltf, its features, performance and API you can read [the docs](https://fastgltf.readthedocs.io/).

> [!NOTE]  
> For C++17 compatibility, please use v0.9.x. Later versions require C++20.

## Examples and real-world usage

You can find some examples in the `examples/` directory of this repository on how to use fastgltf in a 3D renderer to load glTF files.
Additionally, this is a list of some interesting projects using fastgltf:

- [Fwog](https://github.com/JuanDiegoMontoya/Fwog): The examples of this modern OpenGL 4.6 abstraction make use of fastgltf.
- [wad2gltf](https://github.com/DethRaid/wad2gltf): A WAD to glTF converter
- [Castor3D](https://github.com/DragonJoker/Castor3D): A multi-OS 3D engine
- [Raz](https://github.com/Razakhel/RaZ): A modern & multiplatform 3D game engine in C++17
- [vkguide](https://vkguide.dev): A modern Vulkan tutorial


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
