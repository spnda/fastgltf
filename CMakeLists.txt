cmake_minimum_required(VERSION 3.13)

if ("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW)
endif ()

project(fastgltf VERSION 1.0.0 LANGUAGES C CXX)

option(FASTGLTF_USE_CUSTOM_SMALLVECTOR "Uses a custom small_vector type optimised for small arrays" OFF)
option(FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL "Disables the memory allocation algorithm based on polymorphic resources" OFF)
option(FASTGLTF_USE_64BIT_FLOAT "Default to 64-bit double precision floats for everything" OFF)
option(FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES "Enable support for the experimental KHR_implicit_shapes extension" OFF)
option(FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES "Enable support for the experimental KHR_physics_rigid_bodies extension" OFF)

# physics rigid bodies depends on implicit shapes
if (FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES)
    set(FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES ON)
endif ()

option(FASTGLTF_ENABLE_CPP_MODULES "Enables the fastgltf::module target, which uses C++20 modules" OFF)
option(FASTGLTF_USE_STD_MODULE "Use the std module when compiling using C++ modules" OFF)

option(FASTGLTF_DOWNLOAD_SIMDJSON "Download simdjson if not found" ON)
option(FASTGLTF_DOWNLOAD_SIMDUTF "Download simdutf if not found" ON)
option(FASTGLTF_ENABLE_INSTALL "Enables installation of library using CMake's install()" ON)

option(FASTGLTF_ENABLE_TESTS "Enables test targets for fastgltf" OFF)
option(FASTGLTF_ENABLE_EXAMPLES "Enables example targets for fastgltf" OFF)
option(FASTGLTF_ENABLE_DOCS "Enables the configuration of targets that build/generate documentation" OFF)
option(FASTGLTF_ENABLE_GLTF_RS "Enables the benchmark usage of gltf-rs" OFF)
option(FASTGLTF_ENABLE_ASSIMP "Enables the benchmark usage of assimp" OFF)

set(FASTGLTF_COMPILE_TARGET cxx_std_20)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/add_source_directory.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/compilers.cmake)

set(FASTGLTF_SIMDJSON_DL_DIR "${CMAKE_CURRENT_BINARY_DIR}/simdjson")
set(FASTGLTF_SIMDUTF_DL_DIR "${CMAKE_CURRENT_BINARY_DIR}/simdutf")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies.cmake)

# Create the library target
set(FASTGLTF_HEADERS
        "include/fastgltf/base64.hpp"
        "include/fastgltf/core.hpp"
        "include/fastgltf/dxmath_element_traits.hpp"
        "include/fastgltf/glm_element_traits.hpp"
        "include/fastgltf/tools.hpp"
        "include/fastgltf/types.hpp"
        "include/fastgltf/util.hpp"
        "include/fastgltf/uri.hpp"
        "include/fastgltf/math.hpp"
        "include/fastgltf/containers/flagged_optional.hpp"
        "include/fastgltf/containers/inplace_vector.hpp"
        "include/fastgltf/containers/small_vector.hpp"
        "include/fastgltf/containers/static_vector.hpp"
)

add_library(fastgltf
        ${FASTGLTF_HEADERS}
        "src/base64.cpp"
        "src/crc32.cpp"
        "src/fastgltf.cpp"
        "src/io.cpp"
        "src/uri.cpp"
)
add_library(fastgltf::fastgltf ALIAS fastgltf)

fastgltf_compiler_flags(fastgltf)
fastgltf_enable_debug_inlining(fastgltf)
target_compile_features(fastgltf PUBLIC ${FASTGLTF_COMPILE_TARGET})
target_include_directories(fastgltf PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include> $<INSTALL_INTERFACE:include>)

set_target_properties(fastgltf PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS YES)
set_target_properties(fastgltf PROPERTIES VERSION ${PROJECT_VERSION})

target_link_libraries(fastgltf PRIVATE ${FASTGLTF_SIMDJSON_TARGET} ${FASTGLTF_SIMDUTF_TARGET})
if (FASTGLTF_SIMDJSON_TARGET_VERSION)
    target_compile_definitions(fastgltf PRIVATE FASTGLTF_SIMDJSON_TARGET_VERSION="${FASTGLTF_SIMDJSON_TARGET_VERSION}")
endif ()

if (ANDROID)
    target_link_libraries(fastgltf PRIVATE android)
endif ()

# Old versions of GCC and Clang require linking with the standard filesystem library
# See https://en.cppreference.com/w/cpp/filesystem.html#Notes
set(FILESYSTEM_LIB "")

if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
    set(FILESYSTEM_LIB stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    set(FILESYSTEM_LIB c++fs)
endif()

if(FILESYSTEM_LIB)
    target_link_libraries(fastgltf PRIVATE ${FILESYSTEM_LIB})
endif()

target_compile_definitions(fastgltf PUBLIC "FASTGLTF_USE_CUSTOM_SMALLVECTOR=$<BOOL:${FASTGLTF_USE_CUSTOM_SMALLVECTOR}>")
target_compile_definitions(fastgltf PUBLIC "FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL=$<BOOL:${FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL}>")
target_compile_definitions(fastgltf PUBLIC "FASTGLTF_USE_64BIT_FLOAT=$<BOOL:${FASTGLTF_USE_64BIT_FLOAT}>")
target_compile_definitions(fastgltf PUBLIC "FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES=$<BOOL:${FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES}>")
target_compile_definitions(fastgltf PUBLIC "FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES=$<BOOL:${FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES}>")

fastgltf_check_modules_support()
if (FASTGLTF_ENABLE_CPP_MODULES AND FASTGLTF_SUPPORTS_MODULES AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
    message(STATUS "fastgltf: Found compiler support for CXX modules")

    # 3.29.20240416 is what the CMake blog used for talking about import std, so this should roughly be the first
    # version to support the feature.
    if (FASTGLTF_USE_STD_MODULE AND CMAKE_VERSION VERSION_LESS "3.29.20240416")
        message(AUTHOR_WARNING "fastgltf: Using the std module is only natively supported with CMake 3.30 or newer. This might cause compilation errors.")
    endif ()

    add_library(fastgltf_module)
    add_library(fastgltf::module ALIAS fastgltf_module)

    if (FASTGLTF_USE_STD_MODULE)
        target_compile_features(fastgltf_module PRIVATE cxx_std_23 INTERFACE cxx_std_20)
    else ()
        target_compile_features(fastgltf_module PUBLIC cxx_std_20)
    endif ()
    target_sources(fastgltf_module PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/fastgltf.ixx
    )
    target_link_libraries(fastgltf_module PRIVATE fastgltf::fastgltf)
    target_compile_definitions(fastgltf_module PRIVATE "FASTGLTF_MODULE=1")
    target_compile_definitions(fastgltf_module PUBLIC "FASTGLTF_USE_STD_MODULE=$<BOOL:${FASTGLTF_USE_STD_MODULE}>")
elseif (FASTGLTF_ENABLE_CPP_MODULES)
    message(WARNING "FASTGLTF_ENABLE_CPP_MODULES is ON but compiler does not support them")
endif ()

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/fastgltfConfigVersion.cmake"
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY SameMajorVersion
)

if (FASTGLTF_ENABLE_INSTALL)
    install(
        DIRECTORY include/fastgltf
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp"
    )

    set(FASTGLTF_INSTALL_TARGETS fastgltf)
    if (TARGET fastgltf_simdjson)
        # We downloaded simdjson so we need to also install simdjson.h alongside the fastgltf headers
        list(APPEND FASTGLTF_INSTALL_TARGETS fastgltf_simdjson)
        install(
                FILES "${FASTGLTF_SIMDJSON_DL_DIR}/simdjson.h"
                DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fastgltf/simdjson
        )
        set(FASTGLTF_FIND_SIMDJSON OFF)
    else ()
        set(FASTGLTF_FIND_SIMDJSON ON)
    endif ()

    if (TARGET fastgltf_simdutf)
        list(APPEND FASTGLTF_INSTALL_TARGETS fastgltf_simdutf)
        install(
                FILES "${FASTGLTF_SIMDUTF_DL_DIR}/simdutf.h"
                DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fastgltf/simdutf
        )
        set(FASTGLTF_FIND_SIMDUTF OFF)
    else ()
        set(FASTGLTF_FIND_SIMDUTF ON)
    endif ()

    install(
        TARGETS ${FASTGLTF_INSTALL_TARGETS}
        EXPORT fastgltf-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    set(FASTGLTF_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/fastgltf")

    install(
        EXPORT fastgltf-targets
        FILE fastgltf-targets.cmake
        NAMESPACE fastgltf::
        DESTINATION ${FASTGLTF_CONFIG_INSTALL_DIR}
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fastgltfConfig.cmake.in"
        "${PROJECT_BINARY_DIR}/fastgltfConfig.cmake"
        INSTALL_DESTINATION ${FASTGLTF_CONFIG_INSTALL_DIR}
    )

    install(FILES
        "${PROJECT_BINARY_DIR}/fastgltfConfig.cmake"
        "${PROJECT_BINARY_DIR}/fastgltfConfigVersion.cmake"
        DESTINATION ${FASTGLTF_CONFIG_INSTALL_DIR}
    )
endif ()


if (FASTGLTF_ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif ()
if (FASTGLTF_ENABLE_TESTS)
    add_subdirectory(tests)
endif ()
if (FASTGLTF_ENABLE_DOCS)
    add_subdirectory(docs)
endif ()
