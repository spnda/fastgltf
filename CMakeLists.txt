cmake_minimum_required(VERSION 3.10)
project(fastgltf)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/add_source_directory.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/compiler_flags.cmake)
include(FetchContent)

# Download and configure simdjson
set(SIMDJSON_TARGET_VERSION "2.2.2")
set(SIMDJSON_HEADER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/simdjson/simdjson.h")
set(SIMDJSON_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/simdjson/simdjson.cpp")

macro(download_simdjson)
    file(DOWNLOAD "https://github.com/simdjson/simdjson/releases/download/v${SIMDJSON_TARGET_VERSION}/simdjson.h" ${SIMDJSON_HEADER_FILE})
    file(DOWNLOAD "https://github.com/simdjson/simdjson/releases/download/v${SIMDJSON_TARGET_VERSION}/simdjson.cpp" ${SIMDJSON_SOURCE_FILE})
endmacro()

if (EXISTS ${SIMDJSON_HEADER_FILE})
    # Look for the SIMDJSON_VERSION define in the header to check the version.
    file(STRINGS ${SIMDJSON_HEADER_FILE} SIMDJSON_HEADER_VERSION_LINE REGEX "^#define SIMDJSON_VERSION ")
    string(REGEX MATCHALL "[0-9.]+" SIMDJSON_HEADER_VERSION ${SIMDJSON_HEADER_VERSION_LINE})
    message(STATUS "fastgltf: Found simdjson version: ${SIMDJSON_HEADER_VERSION}")
    if (SIMDJSON_HEADER_VERSION VERSION_LESS SIMDJSON_TARGET_VERSION)
        message(STATUS "fastgltf: simdjson outdated, downloading...")
        download_simdjson()
    endif()
else()
    message(STATUS "fastgltf: Did not find simdjson, downloading...")
    download_simdjson()
endif()

add_library(fastgltf_simdjson ${CMAKE_CURRENT_SOURCE_DIR}/simdjson/simdjson.cpp ${CMAKE_CURRENT_SOURCE_DIR}/simdjson/simdjson.h)
target_compile_features(fastgltf_simdjson PUBLIC cxx_std_17)
target_include_directories(fastgltf_simdjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/simdjson/)
compiler_flags(TARGET fastgltf_simdjson)

add_subdirectory(src)
add_subdirectory(tests)
