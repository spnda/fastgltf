cmake_minimum_required(VERSION 3.11)

if ("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW)
endif()

project(fastgltf VERSION 0.3.0 LANGUAGES C CXX)

option(FASTGLTF_DOWNLOAD_SIMDJSON "Downloads a copy of simdjson itself to satisfy the dependency" ON)
option(FASTGLTF_USE_CUSTOM_SMALLVECTOR "Uses a custom SmallVector type optimised for small arrays" OFF)
option(FASTGLTF_ENABLE_TESTS "Enables test targets for fastlgtf" OFF)
option(FASTGLTF_ENABLE_EXAMPLES "Enables example targets for fastgltf" OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/add_source_directory.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/compiler_flags.cmake)

if (FASTGLTF_DOWNLOAD_SIMDJSON)
    # Download and configure simdjson
    set(SIMDJSON_TARGET_VERSION "3.1.0")
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

    add_library(fastgltf_simdjson ${SIMDJSON_HEADER_FILE} ${SIMDJSON_SOURCE_FILE})
    target_compile_features(fastgltf_simdjson PUBLIC cxx_std_17)
    target_include_directories(fastgltf_simdjson PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/deps/simdjson> $<INSTALL_INTERFACE:include>)
    compiler_flags(fastgltf_simdjson)
    enable_debug_inlining(fastgltf_simdjson)

    install(
        FILES deps/simdjson/simdjson.h
        DESTINATION include
    )

    install(
        TARGETS fastgltf_simdjson
        EXPORT fastgltf_simdjson-targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )

    install(
        EXPORT fastgltf_simdjson-targets
        FILE fastgltf_simdjsonTargets.cmake
        NAMESPACE fastgltf::
        DESTINATION lib/cmake/fastgltf
    )
else()
    find_package(simdjson CONFIG REQUIRED)
endif()

add_subdirectory(src)

if (FASTGLTF_ENABLE_TESTS OR FASTGLTF_ENABLE_EXAMPLES)
    add_subdirectory(deps)
endif()

if (FASTGLTF_ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()
if (FASTGLTF_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
