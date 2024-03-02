*********
Changelog
*********

.. contents:: Table of contents

To view the full changelogs for each release please see the `GitHub releases <https://github.com/spnda/fastgltf/releases>`_.

0.7.1
=====
- Add: Support for **glTF extras**
- Add: ``KHR_materials_variants``
- Fix: Inline ``deserializeComponent`` template specializations (#47)
- Fix #48: Allow exporting ``ByteView`` in GLB & catch file write errors
- Fix: Support ``KHR_materials_dispersion`` when exporting
- Fix: Decode percents when loading local files
- Fix: Set ``MimeType::None`` in all sources
- Fix: Use correct error variable when parsing gpu instancing extension
- Fix: Make sure gltf buffer is valid before determining type
- Fix: Allow the GLB BIN chunk to be empty

0.7.0
=====
- Add: glTF Exporter (`#33 <https://github.com/spnda/fastgltf/pull/33>`_)
- Add: ``MSFT_packing`` texture extensions
- Add: ``KHR_materials_dispersion`` extension
- Add: More validation around byteOffsets & byteStride
- Add `#42 <https://github.com/spnda/fastgltf/issues/42>`_: Set default values in struct initializers
- Add: Validation for correctly enabled material extensions
- Change: Combine ``loadGLTF`` and ``loadBinaryGLTF``
- Change: Move ``TRS`` struct out of Node
- Change: Use custom ``StaticVector`` for large (buffer) allocations
- Change `#35 <https://github.com/spnda/fastgltf/issues/35>`_: Remove fastgltf_simdjson target
- Change `#45 <https://github.com/spnda/fastgltf/issues/45>`_: Assert when using accessor tools with unloaded buffers
- Fix `#38 <https://github.com/spnda/fastgltf/issues/38>`_: Switch documentation of rotation and uvOffset
- Fix: ``EXT_meshopt_compression`` used wrong json object
- Fix: Correctly load iridescence texture infos
- Fix `#46 <https://github.com/spnda/fastgltf/issues/46>`_: Make an animation channels' node index optional
- Fix: Always read accessor data as little-endian
- Fix: Support padded matrix accessor data
- Fix: Remove MeshoptCompressionMode::None
- Fix: Also use unreachable macro on Clang
- Fix: Use prefixed CMake variables
- Fix: Only enable ``-Og`` fix for MINGW
- Fix: Don't needlessly parse entire data URIs

0.6.1
=====

- Add: Option to disable polymorphic allocator
- Add: Option to use 64-bit floats for parsing
- Fix #34: Support for fallback buffers with ``EXT_meshopt_compression``
- Fix: Support old GCC ABI
- Fix: Automatically disable polymorphic allocators on non-supporting compilers

0.6.0
=====

- Add: Doxygen & Sphinx online documentation (https://spnda.github.io/fastgltf/)
- Add: Custom optimised ``Optional`` class
- Add: Accessor iterators
- Add: Header with ElementTraits for glm types
- Add: ``GenerateMeshIndices`` option
- Added support for ``EXT_mesh_gpu_instancing`` by @DragonJoker in `#30 <https://github.com/spnda/fastgltf/pull/30>`_
- Added support for ``KHR_materials_pbrSpecularGlossiness`` by @DragonJoker in `#31 <https://github.com/spnda/fastgltf/pull/31>`_
- Add: ``KHR_materials_anisotropy``
- Add: Error-to-string functions
- Add: ``iterateAccessorWithIndex``
- Add: Support normalized accessors in accessor tools
- Add: Polymorphic allocator support for SmallVector
- Change: Combine ``Parser`` and ``glTF`` class
- Change: Use individual image indices for each texture extension
- Change: Use linear polymorphic allocator
- Change: Replaced unordered_map with small_vector for primitive attributes
- Fix: Mark findSparseIndex as inline
- Fix: Properly supported UTF-8 strings & URIs
- Fix: Add missing 'strength' field for occlusion textures
- Fixed various issues with URI parsing & introduced URIView
- Fixed various issues with ``SmallVector``
- Fixed spot light cone angles not being loaded by @JuanDiegoMontoya in `#27 <https://github.com/spnda/fastgltf/pull/27>`_
- Silenced unused parameter warning by @JuanDiegoMontoya in `#23 <https://github.com/spnda/fastgltf/pull/23>`_
- Fixed narrowing conversion warnings by @JuanDiegoMontoya in `#24 <https://github.com/spnda/fastgltf/pull/24>`_
- Fixed multiple typos in documentation

0.5.0
=====

- Add: Android file utilities by @DethRaid in `#14 <https://github.com/spnda/fastgltf/pull/14>`_
- Add: Utilities to extract data from accessors by @forenoonwatch in `#20 <https://github.com/spnda/fastgltf/pull/20>`_
- Add: URI parsing
- Add: Accessor min/max
- Add: ``KHR_materials_specular``, ``KHR_materials_ior``, ``KHR_materials_iridescence``, ``KHR_materials_volume``, ``KHR_materials_transmission``, ``KHR_materials_clearcoat``, ``KHR_materials_emissive_strength``, ``KHR_materials_sheen``, ``KHR_materials_unlit``
- Change: Add ``LoadExternalImages``
- Change: Move headers to dedicated include folder
- Change: Represent GLB buffers using a span
- Change: Rename SparseAccessor fields
- And many other various fixes

0.4.0
=====

- Add: Custom base64 decode callbacks
- Change: Rework DataSource to use std::variant
- Change: Remove ``Options::DontUseSIMD``
- Change: Don't always check if the given directory is valid
- Change: Use std:: prefixed integer types
- Fix: Avoid segfault with MinGW
- Fix: Rework Category enum due to parsing bug
- Fix: More C++ version checks
- Fix: Avoid dangling pointer to GLB bytes
- Fix: Rare overflow warning due to signed-by-default char
- Fix: Various minor CMake issues
- Bump simdjson to 3.1.6
- Const-ify ``GltfBufferData::copyBytes`` by @Eearslya in `#10 <https://github.com/spnda/fastgltf/pull/10>`_

0.3.0
=====

- Add: ``KHR_lights_punctual``
- Add: ``EXT_texture_webp``
- Add: glTF and GLB detection
- Add: Optional ``SmallVector`` optimisation
- Add: Asset information
- Add: Morph targets & sparse accessors
- Add: Optionally minimise JSON before parsing
- Change: Cache cpuid calls for base64 decoding
- Fix: Invalid C++20 concept
- Fix: Loosen ARM64 detection for more platforms
- Bump simdjson from 3.0.0 to 3.1.0

0.2.0
=====

- Add ``KHR_mesh_quantization`` by @Eearslya in `#2 <https://github.com/spnda/fastgltf/pull/2>`_
- Add material parameters and fix defaults by @Eearslya in `#3 <https://github.com/spnda/fastgltf/pull/3>`_
- Add: ``glTF::validate`` function
- Add: Custom buffer memory allocator callbacks
- Add: Ability to decompose transformation matrices
- Fix: Set dataLocation for images with buffer view
- Fix Base64 decoding for + and / by @Eearslya in `#4 <https://github.com/spnda/fastgltf/pull/4>`_
- Perf: Avoid (large) allocations in base64 decoders
- Perf: Always move vectors if possible
- Change: Expose more raw base64 decode functions
- Change: New parse API
- Change: Use ``std::variant`` over raw unions
- Change: Use of concepts in headers if C++20 is used
