*********
Changelog
*********

.. contents:: Table of contents

To view the full changelogs for each release please see the `GitHub releases <https://github.com/spnda/fastgltf/releases>`_.

1.0.0
=====

*This is a major release with many breaking API changes. Most importantly, the library now requires C++20. Please read the entries marked "Change" below when upgrading.*

As a quick overview, the most important changes are:

- Rewritten the ``Exporter`` using simdjson's new ``string_builder`` API. It is now roughly 4x faster.
- Significant memory optimizations reducing the average asset (measured on the glTF-Sample-Assets repository) by 37% and reducing the average allocation count by 59%.
    - For example, on Intel's NewSponza the Asset is 31.4% smaller (581KB vs 846KB) with 51.5% less allocations (812 vs 1674) compared to v0.9.1 (on arm64 macOS with Clang 23).
- Switched to ``simdutf`` for base64 decoding for a ~30% performance improvement as well as better error detection & handling.
- Added a ``box`` and ``boxed_optional`` type for better ergonomics working with optional glTF data.
- Rewritten all supporting containers such as ``small_vector``, ``optional``, and ``static_vector``, fixing a lot of unexpected behavior and memory and exception safety issues.
- Implemented fuzzing for the parser, exporter, and URI parser, fixing an incredible amount of issues and edge cases.
- Updated the library to C++20

This is a major milestone as **fastgltf** has now hit v1.0. However, I want to be clear that there's still a lot of things that I plan on changing in the future. v1.0 was more focused on fixing as many existing issues as possible while laying the groundwork in terms of API changes and supporting functionality to have a better base to expand upon.
I also wanted to finally ditch the 0-based versioning and stick to proper semantic versioning. Previous version numbers were always loosely based on "how much has changed" (see v0.8 vs v0.9).
Also, this is the first major version of **fastgltf** to require C++20 instead of C++17. This was done because I think nearing the end of 2026 there should be enough support for C++20 and the amount of things I had to workaround or implement myself which are features of the language or standard library with C++20 was surprisingly large.
With this release the library recognizes glTF 2.1 assets. This does *not* mean that it can load all of them: assets requiring 2.1 through ``minVersion`` and GLB version 3 files are rejected, while 2.1 assets compatible with 2.0 load as usual.

- Add: Support for more extensions
    - ``KHR_accessor_float16``
    - ``EXT_texture_avif``
- Add: Support for 16-bit floats and 64-bit integers in accessors
    - ``ComponentType`` now has ``HalfFloat``, ``Int64``, and ``UnsignedInt64``.
    - ``std::float16_t`` is also supported on supporting compilers. Otherwise ``fastgltf::HalfFloat`` can be used as a storage type. See the documentation for more details.
- Add: ``GltfDataString`` to easily pass strings or literals to the parser
- Add: ``validateAccessorBounds`` to validate a single accessor against its buffer view and buffer
- Add: ``box`` (a conformant implementation of ``std::indirect`` (C++26)) and ``boxed_optional`` containers
    - This type is essentially a fancy wrapper around a ``std::unique_ptr`` but with value-like semantics
    - ``boxed_optional`` provides the ``std::optional`` interface but uses a ``box`` internally with ``nullptr`` being the valueless state.
    - Members data such as ``Accessor::sparse``, ``Material::clearcoat`` and ``BufferView::meshoptCompression`` are now a ``boxed_optional`` instead of a ``std::unique_ptr`` for a better usage API
- Add: Fuzzing targets, which can be enabled with the ``FASTGLTF_ENABLE_FUZZING`` CMake option (`#158 <https://github.com/spnda/fastgltf/pull/158>`_)
    - Fix `#160 <https://github.com/spnda/fastgltf/issues/160>`_: Guard against invalid accessor indices when generating mesh indices
    - Fix: Integer underflow when generating mesh indices causing large allocations
    - Fix: Malformed data URIs could read out of bounds or overflow the output (@tim-tim707)
    - Fix: GLB JSON chunk total length check
    - Fix: Always reset the GLB buffer when reusing a parser
    - Fix: Avoid NaNs when decomposing a matrix with zero scale
    - Fix: Catch out-of-range and non-finite numbers, and use ``num`` in all remaining types
    - Fix: Guard against large and out-of-bounds material variants
    - Fix: Check glTF indices against the sentinel values used by ``flagged_optional``
    - Fix: Check invalid sparse accessor buffer views and animation channel accessors in ``validate``
    - Change: Reject duplicated root JSON objects, meshes with no primitives, non-finite node transforms, accessor bounds that don't fit into 2^63, and unknown enumeration values
- Add: CMake options ``FASTGLTF_DOWNLOAD_SIMDJSON`` and ``FASTGLTF_DOWNLOAD_SIMDUTF``
    - Downloaded dependencies are now checked against their SHA256, and are installed alongside fastgltf.
    - Disabling these options makes fastgltf fail if ``find_package`` cannot resolve either library.
- Add: ``std::ostream`` overload and ``std::formatter`` specialization for ``Error``
- Change: Completely rewrite ``fastgltf::small_vector``, ``fastgltf::static_vector``, and add ``fastgltf::inplace_vector``
    - ``fastgltf::small_vector`` is now completely compatible with ``std::vector``
    - The containers now all offer the same exception guarantees that comparable ``std`` containers do.
    - The containers are now completely allocator compliant
    - Fixed a significant amount of memory and exception safety issues
    - Renamed the containers and all of their member functions to snake_case to match the C++ standard containers
    - Support for C++23's ``std::from_range``, ``assign_range``, and ``append_range``
- Change: Rename ``Optional`` to ``optional`` and ``OptionalWithFlagValue`` to ``flagged_optional``
    - Flag traits now also allow complex types in ``flagged_optional``.
    - ``flagged_optional`` and ``boxed_optional`` now share a generic ``basic_optional`` interface
- Change: Replace the custom base64 implementations with simdutf (`#159 <https://github.com/spnda/fastgltf/pull/159>`_)
    - fastgltf now depends on simdutf, which is downloaded by default.
    - ``base64::decode`` now returns an ``Expected``, and ``base64::decode_inplace`` returns a bool. Both now report malformed input.
    - The custom NEON/SSE/AVX decode functions have been removed in favor of the simdutf functions since benchmarks showed them to be more performant while being stricter.
    - ``Parser::setBase64DecodeCallback`` and ``Base64DecodeCallback`` were removed.
- Change: Rewrite ``Expected`` as a tagged union, which also allows any error type
    - Using ``Expected`` as a tuple with structured bindings is not possible anymore since it has become a union.
    - New errors: ``InvalidVersion``, ``ValueOutOfRange``, ``UnexpectedJsonType``, ``MissingExternalFile``, and ``Unknown``.
    - ``Error::MissingExternalBuffer`` and ``Error::InvalidOrMissingAssetField`` are now deprecated aliases for ``Error::MissingExternalFile`` and ``Error::InvalidGltf``.
- Change: Harden version parsing and checks (`#165 <https://github.com/spnda/fastgltf/pull/165>`_)
    - This was done to reject assets that advertise minVersion as 2.1.
    - ``AssetInfo::gltfVersion`` is now ``AssetInfo::version`` and uses the new ``Version`` type. ``AssetInfo::minVersion`` is now an ``optional<Version>``.
    - ``Asset::assetInfo`` is no longer optional, and ``Options::DontRequireValidAssetMember`` was removed.
    - Version strings are now parsed using the stricter syntax defined by glTF 2.1.
- Change: Remove all usage of polymorphic allocators and the custom memory pool
    - Nearly all strings and vectors in the glTF types are now ``std::string`` and ``std::vector``.
    - ``Primitive::findAttribute`` and similar functions now return ``std::vector`` iterators instead of pointers.
    - The CMake options ``FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL`` and ``FASTGLTF_USE_CUSTOM_SMALLVECTOR`` were removed.
    - Removed ``MaybeSmallVector``.
- Change: ``sources::URI::uri`` is now boxed to reduce the size of ``DataSource``
- Change: ``Shape`` is now a struct holding the variant in ``Shape::data``, and also has a ``name`` for better future compatibility with glTF 2.1
- Change: ``KHR_implicit_shapes`` and ``KHR_physics_rigid_bodies`` are now always available
    - The CMake options ``FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES`` and ``FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES`` were removed. Enable the extensions through the ``Parser`` like any other extension.
- Change: Split ``core.hpp`` into ``parser.hpp`` and ``exporter.hpp``
    - ``core.hpp`` continues to exist and holds the validation functionality
- Change: Deprecate ``fastgltf::span`` and ``fastgltf::dynamic_extent``. Use the ``std`` types instead.
- Change: Removed ``math::pi`` in favor of ``std::numbers::pi``.
- Removed macros: FASTGLTF_CPP_17, FASTGLTF_CPP_20, FASTGLTF_HAS_BIT, FASTGLTF_HAS_CONCEPTS, FASTGLTF_CONSTEXPR_BITCAST, FASTGLTF_LIKELY / FASTGLTF_UNLIKELY
- Removed CMake option: FASTGLTF_COMPILE_AS_CPP20.
- Various fixes and improvements to the URI parser
    - The parser was rewritten.
    - ``URI::decodePercents`` is now the free function ``decodePercents``, which returns the decoded string.
    - Fix: A ``%`` without two hex digits read out of bounds (@tim-tim707)
    - Fix: Treat ``?`` after ``#`` as part of the URI fragment (@Shaisolaris, `#161 <https://github.com/spnda/fastgltf/pull/161>`_)
- Various fixes and improvements to the Exporter interface
    - Fix: Percent-encode file paths written as URIs (@khiner)
    - Fix: Add missing ``Color`` meshopt compression filter
- Fix `#86 <https://github.com/spnda/fastgltf/issues/86>`_: Move ordinary includes before the module purview
- Fix: Keep scenes that omit nodes (@khiner)
- Fix: ``GenerateMeshIndices`` now generates correct indices for strip, fan, and loop topologies
- Fix: ``copyComponentsFromAccessor`` used the wrong destination address when converting components
- Fix: Wrong accessor element trait type for signed 32-bit vectors
- Fix: Don't throw an error when ``visible``, ``selectable``, or ``hoverable`` is missing
- Fix `#164 <https://github.com/spnda/fastgltf/issues/164>`_: Check the correct error for a missing ``KHR_lights_punctual`` object
- Fix: Check errors returned from parsing material extensions
- Fix: Skip the directory check in ``loadGltfBinary`` on Android to match ``loadGltfJson`` (@1runeberg, `#155 <https://github.com/spnda/fastgltf/pull/155>`_)
- Fix: Don't leak the file descriptor in ``MappedGltfFile`` when ``fstat`` fails
- Fix: Compile with more warnings enabled and ``/W4`` on MSVC, and fix all of them accordingly
- Docs: Clarify the external buffer and image loading options (@snowyukitty, `#162 <https://github.com/spnda/fastgltf/pull/162>`_)
- Bump builtin simdjson version from 4.6.0 to 5.1.0

... and a significant amount of various minor improvements and fixes!

0.9.1
=====

- Add support for more extensions
    - KHR_node_visibility (`#122 <https://github.com/spnda/fastgltf/pull/122>`_)
    - KHR_node_selectability (`#122 <https://github.com/spnda/fastgltf/pull/122>`_)
    - KHR_node_hoverability (`#122 <https://github.com/spnda/fastgltf/pull/122>`_)
    - KHR_meshopt_compression
- Add: CMake install option
- Add: Utility functions for base64 encoding
- Change: Rename invert -> inverse function, improve performance of matrix inversion calculation
- Change: Editor config, reformat everything to use tabs
- Fix (Exporter): KHR_lights_punctual spot light serialization
- Fix (Exporter): Multiple fixes for KHR_physics_rigid_bodies
- Fix (Exporter): Emit mimeType for URI images
- Fix: Node weights were never parsed
- Fix: Correctly move objects in SmallVector constructors
- Bump builtin simdjson version from 3.12 to 4.6

... and a significant amount of various minor improvements and fixes!

0.9.0
=====

- An incredible amount of validation fixes, internal parsing/exporting fixes, and general code improvements
- Add support for more extensions
    - KHR_draco_mesh_compression (`#66 <https://github.com/spnda/fastgltf/issues/66>`_)
    - KHR_physics_rigid_bodies (`#95 <https://github.com/spnda/fastgltf/pull/95>`_)
    - GODOT_single_root (`#100 <https://github.com/spnda/fastgltf/pull/100>`_)
    - KHR_materials_diffuse_transmission
- Add: More support for DXMath vectors/matrices
- Add: C++23 monadic operators on custom OptionalWithFlagValue
- Add: ``IterableAccessor`` satisfies ``ranges::input_range``
- Add: Custom accessor bound type for compile-time type checks
- Fix: Guarantee floating point round-trip precision while parsing/exporting (`#88 <https://github.com/spnda/fastgltf/issues/88>`_)
- Fix: Detect armv8 crc32 programmatically on Apple platforms
- Fix: Use std::byte correctly in Android implementation
- Fix: Make vectors and matrices trivially copyable
- Fix: Validate GLB file/chunk byte length

... and quite a few more various minor improvements and fixes!

0.8.0
=====

- Add: GltfFileStream and rewritten GltfDataBuffer (`#49 <https://github.com/spnda/fastgltf/pull/49>`_)
    - The interface now uses factory constructors, and allows for custom methods for providing the glTF. See documentation for more details.
    - There is also a ``GltfFileStream`` class which wraps a ``std::ifstream`` which can potentially increase memory loading speeds.
    - This allows support for memory mapped files on platforms which provide this functionality using ``MappedGltfFile``.
- Add: Integrated math library (@spnda)
    - All colors and the node transform properties now use vectors/matrices from the built-in library. This library only covers basic maths, and just about everything one needs to work with glTF. There will be additions in the near future for adding more functionality. I've tried not to include too much bloat, but the header might still be a little template-heavy.
    - This change does not require any changes to old code, since the types are essentially just wrappers around ``std::array``. And code such as ``glm::make_vec3(pTranslation->data())`` will continue to work as expected.
- Add: Support C++20 modules (@stripe2933, @spnda, `#61 <https://github.com/spnda/fastgltf/pull/61>`_)
    - Initial revision of a fastgltf.ixx [named module](https://en.cppreference.com/w/cpp/language/modules), which is accessible through CMake with the ``fastgltf::module`` target. This feature is still somewhat experimental, so please report any issues you find.
- Add: Load external files from an APK (`#15 <https://github.com/spnda/fastgltf/pull/15>`_)
    - The interface for letting fastgltf interact with Android assets has fully changed. See documentation for more details.
- Add: Documentation on how to use specific features
    - The 'Guides' subsection now includes short documentation about various features which might not be directly obvious how to use. This also covers the recent changes to ``GltfDataBuffer`` and the Android interface.
- Add: Support for ``KHR_accessor_float64``
- Add: Support for std::float{32,64}_t with accessor tools
- Add: Predefined element traits for DirectXMath types
- Change: Only pass buffer view index to data adapter in tools
    - The ``BufferDataAdapter`` interface now takes a buffer view index instead of the ``Buffer`` reference. Older custom functors will need upgrading.
- Change: Reduce ComponentType to 16 bits & AccessorType to 8 bits.
- Change: Use ``std::invoke`` in the tools header to be compatible with more function wrappers.
- Change: Make Buffer data ``std::byte`` instead of ``std::uint8_t``
- Fix: The ``EXT_meshopt_compression`` wasn't parsed correctly.
    - The test suite around extensions has been expanded to cover more edge cases, and bugs have been fixed accordingly.
- Various fixes and improvements to the Exporter interface
    - Fix `#51 <https://github.com/spnda/fastgltf/issues/51>`_: Normalize resource paths & always use forward slashes
    - Fix `#55 <https://github.com/spnda/fastgltf/issues/55>`_: Cache positionCount & use correct buffer size
    - Fix `#54 <https://github.com/spnda/fastgltf/issues/54>`_: Use correct field name for primitive topology
    - Fix `#53 <https://github.com/spnda/fastgltf/issues/53>`_: Let FileExporter create target directories
    - Fix `#52 <https://github.com/spnda/fastgltf/issues/52>`_: Use const references to GLB buffer sources
    - Fix: Typo in wrapT field name (`#56 <https://github.com/spnda/fastgltf/pull/56>`_)
    - Fix: Bad chunk padding for GLB export (`#57 <https://github.com/spnda/fastgltf/pull/57>`_)
    - Add: Animation support to Exporter (`#64 <https://github.com/spnda/fastgltf/pull/64>`_)
- Fix: Guard usages of throw (`#50 <https://github.com/spnda/fastgltf/pull/50>`_)
- Fix: Accessor's component type is forced (`#59 <https://github.com/spnda/fastgltf/pull/59>`_)
- Fix: Validate UTF-8 for exported JSON strings
- Fix: Always use exceptionless ``std::filesystem`` API
- Fix: Always write/read GLB data in little-endian
- Fix *many* different compiler and clang-tidy warnings

0.7.2
=====

*This release only contains backported fixes from the v0.8 release.*

0.7.1
=====
- Add: Support for **glTF extras**
- Add: ``KHR_materials_variants``
- Fix: Inline ``deserializeComponent`` template specializations (`#47 <https://github.com/spnda/fastgltf/pull/47>`_)
- Fix `#48 <https://github.com/spnda/fastgltf/issues/48>`_: Allow exporting ``ByteView`` in GLB & catch file write errors
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
- Fix `#34 <https://github.com/spnda/fastgltf/issues/34>`_: Support for fallback buffers with ``EXT_meshopt_compression``
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
