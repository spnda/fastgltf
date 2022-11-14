set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL TRUE)

set(FASTGLTF_EXAMPLE_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps")

# glad
add_library(fastgltf_examples_glad EXCLUDE_FROM_ALL)
target_include_directories(fastgltf_examples_glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/include")

add_source_directory(TARGET fastgltf_examples_glad FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/src")
add_source_directory(TARGET fastgltf_examples_glad FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/include")

# glfw. We emulate parts of glfw's CMakeLists but don't want to include any examples or tests.
if(EXISTS "${FASTGLTF_EXAMPLE_DEPS_DIR}/glfw")
    message(STATUS "fastgltf: Found glfw")
    # copied from glfw's CMakeLists. We seriously don't want any examples/tests/docs.
    option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
    option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
    option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
    option(GLFW_INSTALL "Generate installation target" OFF)

    add_subdirectory("${FASTGLTF_EXAMPLE_DEPS_DIR}/glfw")

    set_target_properties(glfw PROPERTIES EXCLUDE_FROM_ALL 1)
    set_target_properties(update_mappings PROPERTIES EXCLUDE_FROM_ALL 1)

    add_library(glfw::glfw ALIAS glfw)
endif()

# glm
if(EXISTS "${FASTGLTF_EXAMPLE_DEPS_DIR}/glm")
    message(STATUS "fastgltf: Found glm")
    add_subdirectory("${FASTGLTF_EXAMPLE_DEPS_DIR}/glm")
    add_library(glm::glm ALIAS glm)

    if (TARGET glm_static)
        set_target_properties(glm_static PROPERTIES EXCLUDE_FROM_ALL 1)
        add_library(glm::static ALIAS glm_static)
    endif()
    if (TARGET glm_shared)
        set_target_properties(glm_shared PROPERTIES EXCLUDE_FROM_ALL 1)
        add_library(glm::shared ALIAS glm_shared)
    endif()
endif()

# stb
if(EXISTS "${FASTGLTF_EXAMPLE_DEPS_DIR}/stb")
    message(STATUS "fastgltf: Found stb")
    add_library(stb)
    target_include_directories(stb PUBLIC "${FASTGLTF_EXAMPLE_DEPS_DIR}/stb")
    target_sources(stb PUBLIC "${FASTGLTF_EXAMPLE_DEPS_DIR}/stb/stb_vorbis.c")
endif()

add_subdirectory(gl_viewer)
