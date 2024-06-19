cmake_minimum_required(VERSION 3.12)

cmake_policy(SET CMP0077 NEW)

if ("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW)
endif()

project(fastgltf VERSION 0.7.2 LANGUAGES C CXX)

option(FASTGLTF_USE_CUSTOM_SMALLVECTOR "Uses a custom SmallVector type optimised for small arrays" OFF)
option(FASTGLTF_ENABLE_TESTS "Enables test targets for fastgltf" OFF)
option(FASTGLTF_ENABLE_EXAMPLES "Enables example targets for fastgltf" OFF)
option(FASTGLTF_ENABLE_DOCS "Enables the configuration of targets that build/generate documentation" OFF)
option(FASTGLTF_ENABLE_GLTF_RS "Enables the benchmark usage of gltf-rs" OFF)
option(FASTGLTF_ENABLE_ASSIMP "Enables the benchmark usage of assimp" OFF)
option(FASTGLTF_ENABLE_DEPRECATED_EXT "Enables support for deprecated extensions" OFF)
option(FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL "Disables the memory allocation algorithm based on polymorphic resources" OFF)
option(FASTGLTF_USE_64BIT_FLOAT "Default to 64-bit double precision floats for everything" OFF)
option(FASTGLTF_COMPILE_AS_CPP20 "Have the library compile as C++20" OFF)

if (FASTGLTF_COMPILE_AS_CPP20)
    set(FASTGLTF_COMPILE_TARGET cxx_std_20)
else()
    set(FASTGLTF_COMPILE_TARGET cxx_std_17)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/add_source_directory.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/compiler_flags.cmake)

# Create the library target
set(FASTGLTF_HEADERS "include/fastgltf/base64.hpp" "include/fastgltf/core.hpp"
        "include/fastgltf/dxmath_element_traits.hpp" "include/fastgltf/glm_element_traits.hpp"
        "include/fastgltf/tools.hpp" "include/fastgltf/types.hpp" "include/fastgltf/util.hpp")
add_library(fastgltf
    "src/fastgltf.cpp" "src/base64.cpp" ${FASTGLTF_HEADERS})
add_library(fastgltf::fastgltf ALIAS fastgltf)

fastgltf_compiler_flags(fastgltf)
fastgltf_enable_debug_inlining(fastgltf)
target_compile_features(fastgltf PUBLIC ${FASTGLTF_COMPILE_TARGET})
target_include_directories(fastgltf PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include> $<INSTALL_INTERFACE:include>)

set_target_properties(fastgltf PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS YES)
set_target_properties(fastgltf PROPERTIES VERSION ${PROJECT_VERSION})

# If the target already exists due to the parent script already including it as a dependency, just directly link it.
if (TARGET simdjson::simdjson)
    target_link_libraries(fastgltf PRIVATE simdjson::simdjson)
else()
    # Try to find simdjson through a find_package call.
    find_package(simdjson CONFIG)
    if (simdjson_FOUND)
        message(STATUS "fastgltf: Found simdjson config")
        target_link_libraries(fastgltf PUBLIC simdjson::simdjson)
    else()
        # Download and configure simdjson
        set(SIMDJSON_TARGET_VERSION "3.9.4")
        set(SIMDJSON_DL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/simdjson")
        file(MAKE_DIRECTORY ${SIMDJSON_DL_DIR})

        set(SIMDJSON_HEADER_FILE "${SIMDJSON_DL_DIR}/simdjson.h")
        set(SIMDJSON_SOURCE_FILE "${SIMDJSON_DL_DIR}/simdjson.cpp")

        macro(download_simdjson)
            file(DOWNLOAD "https://raw.githubusercontent.com/simdjson/simdjson/v${SIMDJSON_TARGET_VERSION}/singleheader/simdjson.h" ${SIMDJSON_HEADER_FILE})
            file(DOWNLOAD "https://raw.githubusercontent.com/simdjson/simdjson/v${SIMDJSON_TARGET_VERSION}/singleheader/simdjson.cpp" ${SIMDJSON_SOURCE_FILE})
        endmacro()

        if (EXISTS ${SIMDJSON_HEADER_FILE})
            # Look for the SIMDJSON_VERSION define in the header to check the version.
            file(STRINGS ${SIMDJSON_HEADER_FILE} SIMDJSON_HEADER_VERSION_LINE REGEX "^#define SIMDJSON_VERSION ")
            string(REGEX MATCHALL "[0-9.]+" SIMDJSON_HEADER_VERSION "${SIMDJSON_HEADER_VERSION_LINE}")
            message(STATUS "fastgltf: Found simdjson (Version ${SIMDJSON_HEADER_VERSION})")

            if (SIMDJSON_HEADER_VERSION STREQUAL "")
                message(FATAL_ERROR "fastgltf: Failed to download simdjson")
            endif()

            if (SIMDJSON_HEADER_VERSION VERSION_LESS SIMDJSON_TARGET_VERSION)
                message(STATUS "fastgltf: simdjson outdated, downloading...")
                download_simdjson()
            endif()
        else()
            message(STATUS "fastgltf: Did not find simdjson, downloading...")
            download_simdjson()

            if (NOT EXISTS "${SIMDJSON_HEADER_FILE}")
                message(FATAL_ERROR "fastgltf: Failed to download simdjson.")
            endif()
        endif()

        fastgltf_add_source_directory(TARGET fastgltf FOLDER ${SIMDJSON_DL_DIR})
        target_include_directories(fastgltf PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/deps/simdjson> $<INSTALL_INTERFACE:include>)
    endif()
endif()

if (SIMDJSON_TARGET_VERSION)
    target_compile_definitions(fastgltf PRIVATE SIMDJSON_TARGET_VERSION="${SIMDJSON_TARGET_VERSION}")
endif()

target_compile_definitions(fastgltf PUBLIC "FASTGLTF_USE_CUSTOM_SMALLVECTOR=$<BOOL:${FASTGLTF_USE_CUSTOM_SMALLVECTOR}>")
target_compile_definitions(fastgltf PUBLIC "FASTGLTF_ENABLE_DEPRECATED_EXT=$<BOOL:${FASTGLTF_ENABLE_DEPRECATED_EXT}>")
target_compile_definitions(fastgltf PUBLIC "FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL=$<BOOL:${FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL}>")
target_compile_definitions(fastgltf PUBLIC "FASTGLTF_USE_64BIT_FLOAT=$<BOOL:${FASTGLTF_USE_64BIT_FLOAT}>")

if (ANDROID)
    target_link_libraries(fastgltf PRIVATE android)
endif()

install(
    FILES ${FASTGLTF_HEADERS}
    DESTINATION include/fastgltf
)

install(
    TARGETS fastgltf
    EXPORT fastgltf-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(
    EXPORT fastgltf-targets
    FILE fastgltfConfig.cmake
    NAMESPACE fastgltf::
    DESTINATION lib/cmake/fastgltf
)

if (FASTGLTF_ENABLE_TESTS OR FASTGLTF_ENABLE_EXAMPLES)
    # This is required so that Catch2 compiles with C++17, enabling various features we use in tests.
    if (NOT DEFINED CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD STREQUAL "" OR CMAKE_CXX_STANDARD LESS 17)
        set(CMAKE_CXX_STANDARD "17" CACHE STRING "C++ standard" FORCE)
    endif()

    add_subdirectory(deps)
endif()

if (FASTGLTF_ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()
if (FASTGLTF_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
if (FASTGLTF_ENABLE_DOCS)
    add_subdirectory(docs)
endif()
