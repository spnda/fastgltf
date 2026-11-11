************
Installation
************

.. contents:: Table of Contents

**fastgltf** can be added to your project with CMake, or installed through a package manager.

.. _conan-install:

Conan
=====

**fastgltf** is available on `ConanCenter <https://conan.io/center/recipes/fastgltf>`_.
You can install pre-built binaries, or build it from source, using Conan 2:

.. code:: bash

   conan install --requires="fastgltf/[*]" --build=missing -s compiler.cppstd=20

``fastgltf/[*]`` resolves to the latest available version, and ``--build=missing`` builds the package from
source if no pre-built binary matches your settings. Since **fastgltf** requires C++20, the package refuses
to build with an older language standard, so make sure ``compiler.cppstd`` is set to ``20`` or newer,
either on the command line as above or in your Conan profile.

To use it in a CMake project, add **fastgltf** to a ``conanfile.txt`` next to your ``CMakeLists.txt``:

.. code:: ini

   [requires]
   fastgltf/[*]

   [generators]
   CMakeDeps
   CMakeToolchain

Then link against the ``fastgltf::fastgltf`` target in your ``CMakeLists.txt``:

.. code:: cmake

   find_package(fastgltf REQUIRED)
   target_link_libraries(my_app PRIVATE fastgltf::fastgltf)

Install the dependencies and configure the project with the generated toolchain file:

.. code:: bash

   conan install . --output-folder=build --build=missing -s compiler.cppstd=20
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
   cmake --build build

The Conan package uses Conan's own simdjson and simdutf packages instead of downloading them during configuration.

The Conan recipe is maintained by Conan maintainers and community contributors, not by **fastgltf** itself.
If the version is out of date, please `create an issue or pull request <https://github.com/conan-io/conan-center-index>`_
on the ConanCenterIndex repository.
