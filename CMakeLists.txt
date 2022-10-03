cmake_minimum_required(VERSION 3.11)

if ("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW)
endif()

project(fastgltf VERSION 0.1.0 LANGUAGES C CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/add_source_directory.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/compiler_flags.cmake)
include(ExternalProject)
include(FetchContent)

# Download and configure simdjson
set(SIMDJSON_TARGET_VERSION "2.2.3")
set(SIMDJSON_HEADER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/simdjson/simdjson.h")
set(SIMDJSON_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/simdjson/simdjson.cpp")

macro(download_simdjson)
    file(DOWNLOAD "https://raw.githubusercontent.com/simdjson/simdjson/v${SIMDJSON_TARGET_VERSION}/singleheader/simdjson.h" ${SIMDJSON_HEADER_FILE})
    file(DOWNLOAD "https://raw.githubusercontent.com/simdjson/simdjson/v${SIMDJSON_TARGET_VERSION}/singleheader/simdjson.cpp" ${SIMDJSON_SOURCE_FILE})
endmacro()

if (EXISTS ${SIMDJSON_HEADER_FILE})
    # Look for the SIMDJSON_VERSION define in the header to check the version.
    file(STRINGS ${SIMDJSON_HEADER_FILE} SIMDJSON_HEADER_VERSION_LINE REGEX "^#define SIMDJSON_VERSION ")
    string(REGEX MATCHALL "[0-9.]+" SIMDJSON_HEADER_VERSION "${SIMDJSON_HEADER_VERSION_LINE}")
    message(STATUS "fastgltf: Found simdjson version: ${SIMDJSON_HEADER_VERSION}")
    if (SIMDJSON_HEADER_VERSION VERSION_LESS SIMDJSON_TARGET_VERSION)
        message(STATUS "fastgltf: simdjson outdated, downloading...")
        download_simdjson()
    endif()
else()
    message(STATUS "fastgltf: Did not find simdjson, downloading...")
    download_simdjson()

    if (NOT EXISTS "${SIMDJSON_HEADER_FILE}")
        message(FATAL_ERROR "Failed to download simdjson.")
    endif()
endif()

add_library(fastgltf_simdjson ${CMAKE_CURRENT_SOURCE_DIR}/simdjson/simdjson.cpp ${CMAKE_CURRENT_SOURCE_DIR}/simdjson/simdjson.h)
target_compile_features(fastgltf_simdjson PUBLIC cxx_std_17)
target_include_directories(fastgltf_simdjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/simdjson/)
compiler_flags(TARGET fastgltf_simdjson)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(examples)
