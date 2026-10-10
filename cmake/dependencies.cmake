include(FetchContent)

function(fastgltf_fetch_file URL DEST HASH)
    # check if the existing file on disk has the correct hash
    if (EXISTS "${DEST}")
        file(SHA256 "${DEST}" EXISTING_HASH)
        if (EXISTING_HASH STREQUAL HASH)
            return()
        endif ()
        message(STATUS "fastgltf: ${DEST} has an unexpected hash")
    endif()

    message(STATUS "fastgltf: Downloading ${URL}...")
    file(
            DOWNLOAD "${URL}" "${DEST}.tmp"
            STATUS DOWNLOAD_STATUS
            EXPECTED_HASH SHA256=${HASH}
            TLS_VERIFY ON
    )

    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)

    if (NOT STATUS_CODE EQUAL 0)
        list(GET DOWNLOAD_STATUS 1 ERROR_MESSAGE)
        file(REMOVE "${DEST}.tmp")
        message(FATAL_ERROR "fastgltf: Error downloading ${URL}: ${ERROR_MESSAGE}")
    endif()

    file(RENAME "${DEST}.tmp" "${DEST}")
endfunction()

function(fastgltf_download_simdjson)
    fastgltf_fetch_file(
            "https://github.com/simdjson/simdjson/releases/download/v${FASTGLTF_SIMDJSON_TARGET_VERSION}/simdjson.h"
            ${FASTGLTF_SIMDJSON_HEADER_FILE}
            c23412d6a5aedb3215ca6617db882eee47642318c330173c319a8fca454b9f3d
    )

    fastgltf_fetch_file(
            "https://github.com/simdjson/simdjson/releases/download/v${FASTGLTF_SIMDJSON_TARGET_VERSION}/simdjson.cpp"
            ${FASTGLTF_SIMDJSON_SOURCE_FILE}
            f44b2798458dd6473afcd3c564a3433c657e38493a10bd50812589a20e2ec00d
    )
endfunction()

function(fastgltf_download_simdutf)
    fastgltf_fetch_file(
            "https://github.com/simdutf/simdutf/releases/download/v${FASTGLTF_SIMDUTF_TARGET_VERSION}/simdutf.h"
            ${FASTGLTF_SIMDUTF_HEADER_FILE}
            27cbcc731b268c36932fabeccd8b7a940804cc51b1325e2221a76b26c5bfab23
    )

    fastgltf_fetch_file(
            "https://github.com/simdutf/simdutf/releases/download/v${FASTGLTF_SIMDUTF_TARGET_VERSION}/simdutf.cpp"
            ${FASTGLTF_SIMDUTF_SOURCE_FILE}
            9d99344ce132040987d11841b1d69900a90ab902efcda61b6552fc6c4515589d
    )
endfunction()

function(fastgltf_find_header_semantic_version HEADER_FILE VERSION_CONSTANT VERSION_OUT_VAR)
    file(STRINGS ${HEADER_FILE} HEADER_VERSION_LINE REGEX "^#define ${VERSION_CONSTANT} ")
    string(REGEX MATCH "\"?([0-9]+\\.[0-9]+\\.[0-9]+)\"?" _ "${HEADER_VERSION_LINE}")
    set(${VERSION_OUT_VAR} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# If the target already exists due to the parent script already including it as a dependency, just directly link it.
if (TARGET simdjson::simdjson)
    set(FASTGLTF_SIMDJSON_TARGET simdjson::simdjson)
else()
    # Try to find simdjson through a find_package call.
    find_package(simdjson CONFIG QUIET)
    if (simdjson_FOUND)
        message(STATUS "fastgltf: Found simdjson config (${simdjson_VERSION})")
        set(FASTGLTF_SIMDJSON_TARGET simdjson::simdjson)
    elseif (FASTGLTF_DOWNLOAD_SIMDJSON)
        # Download and configure simdjson
        set(FASTGLTF_SIMDJSON_TARGET_VERSION "5.0.3")
        file(MAKE_DIRECTORY ${FASTGLTF_SIMDJSON_DL_DIR})

        set(FASTGLTF_SIMDJSON_HEADER_FILE "${FASTGLTF_SIMDJSON_DL_DIR}/simdjson.h")
        set(FASTGLTF_SIMDJSON_SOURCE_FILE "${FASTGLTF_SIMDJSON_DL_DIR}/simdjson.cpp")

        # this will check the hash of existing files and re-download if necessary
        fastgltf_download_simdjson()

        fastgltf_find_header_semantic_version(
                ${FASTGLTF_SIMDJSON_HEADER_FILE} "SIMDJSON_VERSION" FASTGLTF_SIMDJSON_HEADER_VERSION)
        message(STATUS "fastgltf: Found local simdjson (${FASTGLTF_SIMDJSON_HEADER_VERSION})")

        # create a static library for simdjson in the fastgltf "namespace"
        add_library(fastgltf_simdjson STATIC "${FASTGLTF_SIMDJSON_DL_DIR}/simdjson.cpp")
        fastgltf_compiler_flags(fastgltf_simdjson)
        target_include_directories(fastgltf_simdjson PUBLIC
                $<BUILD_INTERFACE:${FASTGLTF_SIMDJSON_DL_DIR}>
                $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/fastgltf/simdjson>)
        target_compile_features(fastgltf_simdjson PUBLIC ${FASTGLTF_COMPILE_TARGET})
        set_target_properties(fastgltf_simdjson PROPERTIES POSITION_INDEPENDENT_CODE ON EXPORT_NAME simdjson)

        add_library(fastgltf::simdjson ALIAS fastgltf_simdjson)
        set(FASTGLTF_SIMDJSON_TARGET fastgltf::simdjson)
    else()
        message(FATAL_ERROR "fastgltf: Failed to find simdjson")
    endif ()
endif ()

if (TARGET simdutf::simdutf)
    set(FASTGLTF_SIMDUTF_TARGET simdutf::simdutf)
else()
    find_package(simdutf CONFIG QUIET)
    if (simdutf_FOUND)
        message(STATUS "fastgltf: Found simdutf config (${simdutf_VERSION})")
        set(FASTGLTF_SIMDUTF_TARGET simdutf::simdutf)
    elseif(FASTGLTF_DOWNLOAD_SIMDUTF)
        set(FASTGLTF_SIMDUTF_TARGET_VERSION "9.2.1")
        file(MAKE_DIRECTORY ${FASTGLTF_SIMDUTF_DL_DIR})

        set(FASTGLTF_SIMDUTF_HEADER_FILE "${FASTGLTF_SIMDUTF_DL_DIR}/simdutf.h")
        set(FASTGLTF_SIMDUTF_SOURCE_FILE "${FASTGLTF_SIMDUTF_DL_DIR}/simdutf.cpp")

        # this will check the hash of existing files and re-download if necessary
        fastgltf_download_simdutf()

        fastgltf_find_header_semantic_version(
                ${FASTGLTF_SIMDUTF_HEADER_FILE} "SIMDUTF_VERSION" FASTGLTF_SIMDUTF_HEADER_VERSION)
        message(STATUS "fastgltf: Found local simdutf (${FASTGLTF_SIMDUTF_HEADER_VERSION})")

        add_library(fastgltf_simdutf STATIC "${FASTGLTF_SIMDUTF_DL_DIR}/simdutf.cpp")
        fastgltf_compiler_flags(fastgltf_simdutf)
        target_include_directories(fastgltf_simdutf PUBLIC
                $<BUILD_INTERFACE:${FASTGLTF_SIMDUTF_DL_DIR}>
                $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/fastgltf/simdutf>)
        target_compile_features(fastgltf_simdutf PUBLIC ${FASTGLTF_COMPILE_TARGET})
        set_target_properties(fastgltf_simdutf PROPERTIES POSITION_INDEPENDENT_CODE ON EXPORT_NAME simdutf)

        # Disable all features except for the base64 feature to save compile time and binary size
        target_compile_definitions(fastgltf_simdutf PUBLIC
                SIMDUTF_FEATURE_BASE64=1
                SIMDUTF_FEATURE_DETECT_ENCODING=0
                SIMDUTF_FEATURE_ASCII=0
                SIMDUTF_FEATURE_LATIN1=0
                SIMDUTF_FEATURE_UTF8=0
                SIMDUTF_FEATURE_UTF16=0
                SIMDUTF_FEATURE_UTF32=0
        )

        add_library(fastgltf::simdutf ALIAS fastgltf_simdutf)
        set(FASTGLTF_SIMDUTF_TARGET fastgltf::simdutf)
    else()
        message(FATAL_ERROR "fastgltf: Failed to find simdutf")
    endif()
endif()

# glm
if (FASTGLTF_ENABLE_TESTS OR FASTGLTF_ENABLE_EXAMPLES)
    message(STATUS "fastgltf: Fetching glm")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm
        GIT_TAG 1.0.1
        GIT_SHALLOW TRUE
    )

    # glm breaks because it apparently can't properly detect constexpr/C++ version and
    # sets -Weverything which enables C++98 compatibility on Clang??
    option(GLM_ENABLE_CXX_17 "" ON)

    FetchContent_MakeAvailable(glm)

    add_library(glm::glm ALIAS glm)
    target_compile_definitions(glm PUBLIC GLM_ENABLE_EXPERIMENTAL=1)

    if (TARGET glm_static)
        set_target_properties(glm_static PROPERTIES EXCLUDE_FROM_ALL 1)
        add_library(glm::static ALIAS glm_static)
    endif ()
    if (TARGET glm_shared)
        set_target_properties(glm_shared PROPERTIES EXCLUDE_FROM_ALL 1)
        add_library(glm::shared ALIAS glm_shared)
    endif ()
endif ()

# Catch2
if (FASTGLTF_ENABLE_TESTS)
    message(STATUS "fastgltf: Fetching catch2")
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2
        GIT_TAG v3.16.0
        GIT_SHALLOW TRUE
    )

    if (NOT DEFINED CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD STREQUAL "" OR CMAKE_CXX_STANDARD LESS 17)
        set(CMAKE_CXX_STANDARD "17" CACHE STRING "C++ standard" FORCE)
    endif ()

    # We can set Catch2 config options like this. We want all the C++17 features.
    option(CATCH_CONFIG_CPP17_UNCAUGHT_EXCEPTIONS "" ON)
    option(CATCH_CONFIG_CPP17_STRING_VIEW "" ON)
    option(CATCH_CONFIG_CPP17_VARIANT "" ON)
    option(CATCH_CONFIG_CPP17_OPTIONAL "" ON)
    option(CATCH_CONFIG_CPP17_BYTE "" ON)
    option(CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS "" ON)

    # Make sure that exceptions are never disabled, as some other library seems to disable them.
    option(CATCH_CONFIG_NO_DISABLE_EXCEPTIONS "" ON)

    if (MSVC)
        option(CATCH_CONFIG_WINDOWS_CRTDBG "" ON)
    endif ()

    FetchContent_MakeAvailable(catch2)
endif ()

# corrosion
if (FASTGLTF_ENABLE_GLTF_RS)
    message(STATUS "fastgltf: Fetching corrosion")
    FetchContent_Declare(
        corrosion
        GIT_REPOSITORY https://github.com/corrosion-rs/corrosion
        GIT_TAG v0.5
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(corrosion)
endif ()

# glfw. We emulate parts of glfw's CMakeLists but don't want to include any examples or tests.
if (FASTGLTF_ENABLE_EXAMPLES)
    message(STATUS "fastgltf: Fetching glfw")
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )

    # copied from glfw's CMakeLists. We seriously don't want any examples/tests/docs.
    option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
    option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
    option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
    option(GLFW_INSTALL "Generate installation target" OFF)

    FetchContent_MakeAvailable(glfw)

    set_target_properties(glfw PROPERTIES EXCLUDE_FROM_ALL 1)
    set_target_properties(update_mappings PROPERTIES EXCLUDE_FROM_ALL 1)

    add_library(glfw::glfw ALIAS glfw)
endif ()

# glad
if (FASTGLTF_ENABLE_EXAMPLES)
    message(STATUS "fastgltf: Fetching glad")
    FetchContent_Declare(
        glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad
        GIT_TAG glad2
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR cmake
    )

    FetchContent_MakeAvailable(glad)

    # Let the glad script generate the glad headers
    glad_add_library(fg_glad_gl46 REPRODUCIBLE EXCLUDE_FROM_ALL LOADER API gl:core=4.6)
endif ()

# imgui
if (FASTGLTF_ENABLE_EXAMPLES)
    message(STATUS "fastgltf: Fetching imgui")
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui
        GIT_TAG v1.90.9
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR doesnt-exist # This is so that MakeAvailable doesn't call add_subdirectory
    )

    FetchContent_GetProperties(imgui)
    if (NOT imgui_POPULATED)
        FetchContent_MakeAvailable(imgui)

        add_library(imgui)
        add_library(imgui::imgui ALIAS imgui)
        fastgltf_add_source_directory(TARGET imgui FOLDER "${imgui_SOURCE_DIR}")
        fastgltf_add_source_directory(TARGET imgui FOLDER "${imgui_SOURCE_DIR}/misc/cpp/")

        target_include_directories(imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}")
        target_include_directories(imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}/backends")
        target_include_directories(imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}/misc/cpp")

        target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1)

        # We only need the GLFW implementation
        target_sources(imgui PUBLIC "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.h" "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp")
        target_sources(imgui PUBLIC "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.h" "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp")

        target_link_libraries(imgui PUBLIC glfw::glfw)
    endif ()
endif ()

# stb
if (FASTGLTF_ENABLE_EXAMPLES)
    message(STATUS "fastgltf: Fetching stb")
    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb
        GIT_TAG master
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR doesnt-exist # This is so that MakeAvailable doesn't call add_subdirectory
    )

    FetchContent_GetProperties(stb)
    if (NOT stb_POPULATED)
        FetchContent_MakeAvailable(stb)

        add_library(stb INTERFACE)
        target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
    endif ()
endif ()
