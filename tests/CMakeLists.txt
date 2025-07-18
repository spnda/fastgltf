set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL TRUE)

message(STATUS "fastgltf: Bulding tests")

# We want these tests to be a optional executable.
add_executable(fastgltf_tests EXCLUDE_FROM_ALL "main.cpp"
    "base64_tests.cpp" "basic_test.cpp" "benchmarks.cpp" "glb_tests.cpp" "gltf_path.hpp" "util_tests.cpp" "optional_tests.cpp"
    "vector_tests.cpp" "uri_tests.cpp" "extension_tests.cpp" "accessor_tests.cpp" "write_tests.cpp" "math_tests.cpp")
target_compile_features(fastgltf_tests PRIVATE ${FASTGLTF_COMPILE_TARGET})
target_link_libraries(fastgltf_tests PRIVATE fastgltf::fastgltf)
target_link_libraries(fastgltf_tests PRIVATE glm::glm Catch2::Catch2)
fastgltf_compiler_flags(fastgltf_tests)

# We only use tinygltf to compare against.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gltf_loaders/tinygltf/tiny_gltf.h")
    message(STATUS "fastgltf: Found tinygltf")
    set(TINYGLTF_INSTALL OFF CACHE BOOL "")
    set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "")
    set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "")

    add_subdirectory(gltf_loaders/tinygltf)
    target_link_libraries(fastgltf_tests PRIVATE tinygltf)
    target_compile_definitions(fastgltf_tests PRIVATE HAS_TINYGLTF=1)

    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gltf_loaders/RapidJSON")
        # RapidJSON's CMake is weird
        message(STATUS "fastgltf: Found RapidJSON")
        target_include_directories(fastgltf_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/gltf_loaders/RapidJSON/include")
        target_compile_definitions(fastgltf_tests PRIVATE HAS_RAPIDJSON=1 TINYGLTF_USE_RAPIDJSON=1 TINYGLTF_NO_INCLUDE_RAPIDJSON)
    endif ()
endif ()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gltf_loaders/cgltf/cgltf.h")
    message(STATUS "fastgltf: Found cgltf")
    target_include_directories(fastgltf_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/gltf_loaders/cgltf")
    target_compile_definitions(fastgltf_tests PRIVATE HAS_CGLTF=1)
endif ()

if (FASTGLTF_ENABLE_GLTF_RS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gltf-rs/src/lib.rs")
    message(STATUS "fastgltf: Found gltf-rs")
    corrosion_import_crate(MANIFEST_PATH gltf-rs/Cargo.toml)
    corrosion_add_cxxbridge(gltf-rs-bridge CRATE gltf_rs MANIFEST_PATH gltf-rs FILES lib.rs)

    target_link_libraries(fastgltf_tests PUBLIC gltf-rs-bridge)
    target_compile_definitions(fastgltf_tests PRIVATE HAS_GLTFRS=1)
endif ()

if (FASTGLTF_ENABLE_ASSIMP AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gltf_loaders/assimp")
    message(STATUS "fastgltf: Found assimp")
    # Only enable glTF importer
    set(ASSIMP_NO_EXPORT ON CACHE BOOL "")
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
    set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "")
    add_subdirectory(gltf_loaders/assimp)
    target_link_libraries(fastgltf_tests PRIVATE assimp::assimp)
    target_compile_definitions(fastgltf_tests PRIVATE HAS_ASSIMP=1)
endif ()
