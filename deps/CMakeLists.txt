set(FASTGLTF_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# glm
if((FASTGLTF_ENABLE_TESTS OR FASTGLTF_ENABLE_EXAMPLES) AND EXISTS "${FASTGLTF_DEPS_DIR}/glm")
    message(STATUS "fastgltf: Found glm")

    # glm breaks because it apparently can't properly detect constexpr/C++ version and
    # sets -Weverything which enables C++98 compatibility on Clang??
    option(GLM_ENABLE_CXX_17 "" ON)

    add_subdirectory("${FASTGLTF_DEPS_DIR}/glm")
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

# Catch2
if (FASTGLTF_ENABLE_TESTS AND EXISTS "${FASTGLTF_DEPS_DIR}/catch2")
    message(STATUS "fastgltf: Found catch2")
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
    endif()
    add_subdirectory(${FASTGLTF_DEPS_DIR}/catch2)
endif()

# corrosion
if (FASTGLTF_ENABLE_GLTF_RS AND EXISTS "${FASTGLTF_DEPS_DIR}/corrosion")
    message(STATUS "fastgltf: Found corrosion")
    add_subdirectory(${FASTGLTF_DEPS_DIR}/corrosion)
endif()

# glfw. We emulate parts of glfw's CMakeLists but don't want to include any examples or tests.
if(FASTGLTF_ENABLE_EXAMPLES AND EXISTS "${FASTGLTF_DEPS_DIR}/glfw")
    message(STATUS "fastgltf: Found glfw")
    # copied from glfw's CMakeLists. We seriously don't want any examples/tests/docs.
    option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
    option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
    option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
    option(GLFW_INSTALL "Generate installation target" OFF)

    add_subdirectory("${FASTGLTF_DEPS_DIR}/glfw")

    set_target_properties(glfw PROPERTIES EXCLUDE_FROM_ALL 1)
    set_target_properties(update_mappings PROPERTIES EXCLUDE_FROM_ALL 1)

    add_library(glfw::glfw ALIAS glfw)
endif()

# glad
if (FASTGLTF_ENABLE_EXAMPLES AND EXISTS "${FASTGLTF_DEPS_DIR}/glad")
    message(STATUS "fastgltf: Found glad")
    add_subdirectory(${FASTGLTF_DEPS_DIR}/glad/cmake glad_cmake)

    # Let the glad script generate the glad headers
    glad_add_library(fg_glad_gl46 REPRODUCIBLE EXCLUDE_FROM_ALL LOADER API gl:core=4.6)
endif()

# imgui
if (FASTGLTF_ENABLE_EXAMPLES AND EXISTS "${FASTGLTF_DEPS_DIR}/imgui")
    message(STATUS "fastgltf: Found imgui")

    set(IMGUI_DIRECTORY "${FASTGLTF_DEPS_DIR}/imgui")
    if(NOT EXISTS "${IMGUI_DIRECTORY}")
        message(FATAL_ERROR "The imgui submodule does not exist.")
    endif()

    add_library(imgui)
    add_library(imgui::imgui ALIAS imgui)
    fastgltf_add_source_directory(TARGET imgui FOLDER "${IMGUI_DIRECTORY}")
    fastgltf_add_source_directory(TARGET imgui FOLDER "${IMGUI_DIRECTORY}/misc/cpp/")

    target_include_directories(imgui SYSTEM PUBLIC "${IMGUI_DIRECTORY}")
    target_include_directories(imgui SYSTEM PUBLIC "${IMGUI_DIRECTORY}/backends")
    target_include_directories(imgui SYSTEM PUBLIC "${IMGUI_DIRECTORY}/misc/cpp")

    target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1)

    # We only need the GLFW implementation
    target_sources(imgui PUBLIC "${IMGUI_DIRECTORY}/backends/imgui_impl_glfw.h" "${IMGUI_DIRECTORY}/backends/imgui_impl_glfw.cpp")
    target_sources(imgui PUBLIC "${IMGUI_DIRECTORY}/backends/imgui_impl_opengl3.h" "${IMGUI_DIRECTORY}/backends/imgui_impl_opengl3.cpp")

    target_link_libraries(imgui PUBLIC glfw::glfw)
endif()

# stb
if(FASTGLTF_ENABLE_EXAMPLES AND EXISTS "${FASTGLTF_DEPS_DIR}/stb")
    message(STATUS "fastgltf: Found stb")
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE "${FASTGLTF_DEPS_DIR}/stb")
endif()
