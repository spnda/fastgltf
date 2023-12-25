set(FASTGLTF_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# glfw. We emulate parts of glfw's CMakeLists but don't want to include any examples or tests.
if(EXISTS "${FASTGLTF_DEPS_DIR}/glfw")
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

# glm
if(EXISTS "${FASTGLTF_DEPS_DIR}/glm")
    message(STATUS "fastgltf: Found glm")
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

# stb
if(EXISTS "${FASTGLTF_DEPS_DIR}/stb")
    message(STATUS "fastgltf: Found stb")
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE "${FASTGLTF_DEPS_DIR}/stb")
endif()

# Catch2
if (EXISTS "${FASTGLTF_DEPS_DIR}/catch2")
    message(STATUS "fastgltf: Found catch2")
    # We can set Catch2 config options like this. We want all the C++17 features.
    option(CATCH_CONFIG_CPP17_UNCAUGHT_EXCEPTIONS "" ON)
    option(CATCH_CONFIG_CPP17_STRING_VIEW "" ON)
    option(CATCH_CONFIG_CPP17_VARIANT "" ON)
    option(CATCH_CONFIG_CPP17_OPTIONAL "" ON)
    option(CATCH_CONFIG_CPP17_BYTE "" ON)
    option(CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS "" ON)

    if (MSVC)
        option(CATCH_CONFIG_WINDOWS_CRTDBG "" ON)
    endif()
    add_subdirectory(${FASTGLTF_DEPS_DIR}/catch2)
endif()

# glad
if (EXISTS "${FASTGLTF_DEPS_DIR}/glad")
    message(STATUS "fastgltf: Found glad")
    add_subdirectory(${FASTGLTF_DEPS_DIR}/glad/cmake glad_cmake)

    # Let the glad script generate the glad headers
    glad_add_library(fg_glad_gl46 REPRODUCIBLE EXCLUDE_FROM_ALL LOADER API gl:core=4.6)
endif()

# corrosion
if (FASTGLTF_ENABLE_GLTF_RS AND EXISTS "${FASTGLTF_DEPS_DIR}/corrosion")
    message(STATUS "fastgltf: Found corrosion")
    add_subdirectory(${FASTGLTF_DEPS_DIR}/corrosion)
endif()

if (EXISTS "${FASTGLTF_DEPS_DIR}/vulkan-headers")
    message(STATUS "fastgltf: Found Vulkan-Headers")
    add_library(Vulkan-Headers INTERFACE)
    add_library(Vulkan::Headers ALIAS Vulkan-Headers)
    target_include_directories(Vulkan-Headers SYSTEM INTERFACE "${FASTGLTF_DEPS_DIR}/vulkan-headers/include")
endif()

if (EXISTS "${FASTGLTF_DEPS_DIR}/volk")
    message(STATUS "fastgltf: Found volk")
    # volk automatically calls find_package(Vulkan) if VOLK_PULL_IN_VULKAN is ON. VOLK_HEADERS_ONLY makes it only add volk::volk_headers and
    # doesn't try compiling volk::volk statically, which would fail because VOLK_PULL_IN_VULKAN is set to OFF.
    option(VOLK_PULL_IN_VULKAN "" OFF)
    option(VOLK_HEADERS_ONLY "" ON)
    add_subdirectory(${FASTGLTF_DEPS_DIR}/volk)
endif()

if (EXISTS "${FASTGLTF_DEPS_DIR}/vk-bootstrap")
    message(STATUS "fastgltf: Found vkb")
    add_subdirectory(${FASTGLTF_DEPS_DIR}/vk-bootstrap)
endif()

if (EXISTS "${FASTGLTF_DEPS_DIR}/vulkanmemoryallocator")
    message(STATUS "fastgltf: Found VMA")
    add_library(VulkanMemoryAllocator INTERFACE)
    add_library(Vulkan::MemoryAllocator ALIAS VulkanMemoryAllocator)
    target_include_directories(VulkanMemoryAllocator SYSTEM INTERFACE "${FASTGLTF_DEPS_DIR}/vulkanmemoryallocator/include")
endif()

if (EXISTS "${FASTGLTF_DEPS_DIR}/enkits")
    message(STATUS "fastgltf: Found enkiTS")
    option(ENKITS_BUILD_C_INTERFACE "" OFF)
    option(ENKITS_BUILD_EXAMPLES "" OFF)
    add_subdirectory("${FASTGLTF_DEPS_DIR}/enkiTS")
    add_library(enkiTS::enkiTS ALIAS enkiTS)
endif()
