set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL TRUE)

# glad
add_library(fastgltf_examples_glad EXCLUDE_FROM_ALL)
target_include_directories(fastgltf_examples_glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/include")

add_source_directory(TARGET fastgltf_examples_glad FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/src")
add_source_directory(TARGET fastgltf_examples_glad FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/include")

add_subdirectory(gl_viewer)

# glfw. We emulate parts of glfw's CMakeLists but don't want to include any examples or tests.
FetchContent_Declare(glfw_zip
    URL "https://github.com/glfw/glfw/releases/download/3.3.8/glfw-3.3.8.zip"
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/glfw")

if(NOT glfw_zip_POPULATED AND EXISTS "deps/glfw")
    FetchContent_Populate(glfw_zip)

    # copied from glfw's CMakeLists. We seriously don't want any examples/tests/docs.
    option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
    option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
    option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
    option(GLFW_INSTALL "Generate installation target" OFF)

    add_subdirectory(${glfw_zip_SOURCE_DIR} ${glfw_zip_BINARY_DIR})

    set_target_properties(glfw PROPERTIES EXCLUDE_FROM_ALL 1)
    set_target_properties(update_mappings PROPERTIES EXCLUDE_FROM_ALL 1)
endif()

# glm
FetchContent_Declare(glm_zip
    URL "https://github.com/g-truc/glm/releases/download/0.9.9.8/glm-0.9.9.8.zip"
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/glm")

if(NOT glm_zip_POPULATED AND EXISTS "deps/glm")
    FetchContent_Populate(glm_zip)

    add_subdirectory(${glm_zip_SOURCE_DIR} ${glm_zip_BINARY_DIR})

    if (TARGET glm_static)
        set_target_properties(glm_static PROPERTIES EXCLUDE_FROM_ALL 1)
    endif()
    if (TARGET glm_shared)
        set_target_properties(glm_shared PROPERTIES EXCLUDE_FROM_ALL 1)
    endif()
endif()
